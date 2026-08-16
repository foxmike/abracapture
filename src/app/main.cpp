#include "AppController.hpp"
#include "audio/CaptureSession.hpp"
#include "cli/CliApp.hpp"
#include "pipewire/PipeWireContext.hpp"
#include "pipewire/SourceCatalog.hpp"
#include "pipewire/SourceDescriptor.hpp"
#include "recording/OutputPathPolicy.hpp"
#include "recording/RecordingSink.hpp"
#include "ui/CaptureViewModel.hpp"
#include "ui/SettingsViewModel.hpp"
#include "ui/SourceListModel.hpp"
#include "ui/WaveformBridge.hpp"

#include <pipewire/context.h>
#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>

#include <QCoreApplication>
#include <QApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

QString sourceKind(const SourceCategory category) {
    switch (category) {
    case SourceCategory::ApplicationPlayback:
        return QStringLiteral("Application");
    case SourceCategory::OutputMonitor:
        return QStringLiteral("Monitor");
    case SourceCategory::HardwareInput:
        return QStringLiteral("Hardware");
    case SourceCategory::Other:
        return QStringLiteral("Other");
    }
    return QStringLiteral("Other");
}

SourceItem sourceItem(const SourceDescriptor& descriptor) {
    return {
        .id = descriptor.nodeId,
        .displayName = QString::fromStdString(descriptor.displayName),
        .appName = QString::fromStdString(descriptor.appName),
        .sourceKind = sourceKind(descriptor.category),
        .channelCount = static_cast<int>(descriptor.channels.value_or(2)),
        .sampleRate = static_cast<int>(descriptor.sampleRate.value_or(48000)),
        .isActive = false,
        .mediaClass = QString::fromStdString(descriptor.mediaClass),
        .nodeName = QString::fromStdString(descriptor.nodeName),
    };
}

QString requestedFormatDescription(const QVariantMap& source) {
    const int sampleRate = source.value(QStringLiteral("sampleRate"), 48000).toInt();
    const int channels = source.value(QStringLiteral("channelCount"), 2).toInt();
    const QString channelDescription = channels == 1
                                           ? QStringLiteral("mono")
                                           : QStringLiteral("%1 ch").arg(channels);
    return QStringLiteral("%1 kHz · %2 · 32-bit float")
        .arg(static_cast<double>(sampleRate) / 1000.0, 0, 'f', 1)
        .arg(channelDescription);
}

} // namespace

int main(int argc, char *argv[]) {
    bool isCli = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "list-sources" || arg == "record") {
            isCli = true;
            break;
        }
    }

    if (isCli) {
        QCoreApplication application(argc, argv);
        CliApp cliApp;
        return cliApp.run(argc, argv);
    }

    QApplication application(argc, argv);
    AppController appController;
    SourceListModel sourceListModel;
    WaveformBridge waveformBridge;
    SettingsViewModel settingsViewModel;
    settingsViewModel.load();
    CaptureViewModel captureViewModel(&sourceListModel, &settingsViewModel);

    PipeWireContext pipeWireContext;
    SourceCatalog sourceCatalog;
    sourceCatalog.sourceAppeared = [&sourceListModel](const SourceDescriptor& descriptor) {
        const SourceItem item = sourceItem(descriptor);
        QMetaObject::invokeMethod(
            &sourceListModel,
            [&sourceListModel, item] { sourceListModel.addOrUpdateSource(item); },
            Qt::QueuedConnection);
    };
    sourceCatalog.sourceDisappeared = [&sourceListModel](const std::uint32_t sourceId) {
        QMetaObject::invokeMethod(
            &sourceListModel,
            [&sourceListModel, sourceId] { sourceListModel.removeSource(sourceId); },
            Qt::QueuedConnection);
    };
    sourceCatalog.catalogReset = [&sourceListModel] {
        QMetaObject::invokeMethod(
            &sourceListModel,
            [&sourceListModel] { sourceListModel.clear(); },
            Qt::QueuedConnection);
    };
    sourceCatalog.start(pipeWireContext);
    pipeWireContext.start();

    pw_thread_loop* captureLoop = pw_thread_loop_new("abracapture-gui-capture", nullptr);
    pw_context* captureContext = captureLoop != nullptr
                                     ? pw_context_new(pw_thread_loop_get_loop(captureLoop), nullptr, 0)
                                     : nullptr;
    pw_core* captureCore = captureContext != nullptr
                               ? pw_context_connect(captureContext, nullptr, 0)
                               : nullptr;
    const bool captureLoopStarted = captureCore != nullptr &&
                                    pw_thread_loop_start(captureLoop) >= 0;
    if (!captureLoopStarted) {
        if (captureCore != nullptr) {
            pw_core_disconnect(captureCore);
            captureCore = nullptr;
        }
        if (captureContext != nullptr) {
            pw_context_destroy(captureContext);
            captureContext = nullptr;
        }
        if (captureLoop != nullptr) {
            pw_thread_loop_destroy(captureLoop);
            captureLoop = nullptr;
        }
    }

    abracapture::recording::OutputPathPolicy outputPathPolicy;
    abracapture::recording::RecordingSink recordingSink;
    bool recordingSinkOpen = false;
    bool recordingUsesFlac = false;
    bool recordingActive = false;
    bool previewActive = false;
    std::vector<std::int32_t> flacConversionBuffer;

    // CaptureSession is the engine boundary. The source catalog retains the
    // Phase-B discovery connection while this dedicated connection owns stream
    // lifecycle; the view model only observes state and emits user requests.
    auto captureSession = std::make_unique<abracapture::audio::CaptureSession>(
        captureLoop, captureCore, sourceCatalog, 65536,
        [&captureViewModel, &recordingActive](const abracapture::audio::SessionState state) {
            if (!recordingActive && state != abracapture::audio::SessionState::Stopped) {
                return;
            }
            QMetaObject::invokeMethod(
                &captureViewModel,
                [&captureViewModel, state] { captureViewModel.handleSessionState(state); },
                Qt::QueuedConnection);
        },
        [&recordingSink, &recordingSinkOpen, &recordingUsesFlac, &flacConversionBuffer](
            const float* samples, const std::size_t frameCount) {
            if (!recordingSinkOpen) {
                return;
            }
            if (!recordingUsesFlac) {
                static_cast<void>(recordingSink.pushSamples(samples, frameCount));
                return;
            }

            flacConversionBuffer.resize(frameCount);
            for (std::size_t index = 0; index < frameCount; ++index) {
                const float clamped = std::clamp(samples[index], -1.0F, 1.0F);
                flacConversionBuffer[index] = static_cast<std::int32_t>(
                    std::lround(clamped * 2147483647.0F));
            }
            static_cast<void>(recordingSink.pushSamples(flacConversionBuffer.data(), frameCount));
        });

    auto previewSession = std::make_unique<abracapture::audio::CaptureSession>(
        captureLoop, captureCore, sourceCatalog, 65536);
    auto startPreview = [&previewSession, &sourceListModel, &previewActive](
                            const quint32 sourceId) {
        const QVariantMap source = sourceListModel.getSourceById(sourceId);
        if (source.isEmpty()) {
            previewActive = false;
            return;
        }
        previewActive = previewSession->begin(
            sourceId, SPA_AUDIO_FORMAT_F32,
            static_cast<std::uint32_t>(source.value(QStringLiteral("sampleRate"), 48000).toUInt()),
            static_cast<std::uint32_t>(source.value(QStringLiteral("channelCount"), 2).toUInt()));
    };
    QObject::connect(&sourceListModel, &SourceListModel::sourceSelected,
                     &application, [&startPreview](const quint32 sourceId) {
                         startPreview(sourceId);
                     });

    QObject::connect(&captureViewModel, &CaptureViewModel::startCaptureRequested,
                     &application, [&captureSession, &captureViewModel, &sourceListModel,
                                    &sourceCatalog, &settingsViewModel, &recordingSink,
                                    &outputPathPolicy, &recordingSinkOpen, &recordingUsesFlac,
                                    &recordingActive, &previewSession, &previewActive,
                                    &startPreview](
                                       const quint32 sourceId) {
                         const QVariantMap source = sourceListModel.getSourceById(sourceId);
                         const auto sourceDescriptor = sourceCatalog.sourceById(sourceId);
                         if (!sourceDescriptor) {
                             captureViewModel.reportCaptureError(
                                 QStringLiteral("Selected audio source is no longer available."));
                             return;
                         }
                         const std::uint32_t rate = static_cast<std::uint32_t>(
                             source.value(QStringLiteral("sampleRate"), 48000).toUInt());
                         const std::uint32_t channels = static_cast<std::uint32_t>(
                             source.value(QStringLiteral("channelCount"), 2).toUInt());

                         if (previewActive) {
                             previewSession->stop();
                             previewActive = false;
                         }
                         recordingActive = true;

                         outputPathPolicy.setBaseDirectory(
                             std::filesystem::path(settingsViewModel.outputDirectory().toStdString()));
                         outputPathPolicy.setFilenameTemplate(
                             settingsViewModel.namingPattern().toStdString());
                         const QString masterFormat = settingsViewModel.masterFormat().toLower();
                         recordingUsesFlac = masterFormat == QStringLiteral("flac");
                         outputPathPolicy.setDefaultFormat(masterFormat.toStdString());

                         const AudioFormat fileFormat{
                             .sampleRate = rate,
                             .channels = static_cast<std::uint16_t>(channels),
                             .sampleFormat = recordingUsesFlac ? SampleFormat::Int32
                                                               : SampleFormat::Float32};
                         const abracapture::recording::CaptureMetadata metadata{
                             .source = sourceDescriptor->displayName.empty()
                                           ? sourceDescriptor->appName
                                           : sourceDescriptor->displayName,
                             .sampleRate = rate,
                             .channels = static_cast<std::uint16_t>(channels),
                             .format = masterFormat.toStdString(),
                         };
                         recordingSink.abort();
                         recordingSinkOpen = recordingSink.open(fileFormat, metadata,
                                                                  outputPathPolicy);
                         if (!recordingSinkOpen) {
                             captureViewModel.reportCaptureError(
                                 QStringLiteral("Unable to open recording file: %1")
                                     .arg(QString::fromStdString(recordingSink.lastError())));
                             recordingActive = false;
                             startPreview(sourceId);
                             return;
                         }

                         captureViewModel.updateEngineFormat(
                             requestedFormatDescription(source), CaptureViewModel::Native);
                         if (!captureSession->begin(sourceId, SPA_AUDIO_FORMAT_F32, rate, channels)) {
                             recordingSink.abort();
                             recordingSinkOpen = false;
                             recordingActive = false;
                             startPreview(sourceId);
                             captureViewModel.reportCaptureError(
                                 QStringLiteral("PipeWire could not start capture at %1 Hz, %2 channels.")
                                     .arg(rate)
                                     .arg(channels));
                         }
                     });
    QObject::connect(&captureViewModel, &CaptureViewModel::stopCaptureRequested,
                     &application, [&captureSession, &captureViewModel, &recordingSink,
                                    &recordingSinkOpen, &recordingActive, &sourceListModel,
                                    &previewActive, &startPreview] {
                         captureSession->stop();
                         if (recordingSinkOpen) {
                             const auto result = recordingSink.finalize();
                             recordingSinkOpen = false;
                             if (!result.success) {
                                 captureViewModel.reportCaptureError(
                                     QStringLiteral("Recording could not be finalized: %1")
                                     .arg(QString::fromStdString(result.error)));
                             }
                         }
                         recordingActive = false;
                         if (!previewActive) {
                             startPreview(sourceListModel.selectedSourceId());
                         }
                     });

    QTimer visualizationTimer;
    visualizationTimer.setInterval(33);
    QObject::connect(&visualizationTimer, &QTimer::timeout,
                     &application, [&captureSession, &previewSession, &previewActive,
                                    &recordingActive, &waveformBridge, &captureViewModel] {
                         auto* visualizationSession = previewActive
                                                          ? previewSession.get()
                                                          : captureSession.get();
                         abracapture::audio::CaptureSession::CaptureVisualizationSnapshot snapshot;
                         if (visualizationSession->visualizationSnapshot(snapshot)) {
                             float peak = 0.0F;
                             QVector<float> samples;
                             samples.reserve(static_cast<qsizetype>(snapshot.sampleCount));
                             for (std::size_t index = 0; index < snapshot.sampleCount; ++index) {
                                 peak = std::max(peak, std::abs(snapshot.samples[index]));
                                 samples.append(snapshot.samples[index]);
                             }
                             if (!recordingActive || !previewActive) {
                                 captureViewModel.feedAudioLevel(peak);
                             }
                             QMetaObject::invokeMethod(
                                 &waveformBridge,
                                 [&waveformBridge, samples = std::move(samples)]() mutable {
                                     waveformBridge.submitVisualizationSnapshot(std::move(samples));
                                 },
                                 Qt::QueuedConnection);
                         }

                         const auto metrics = captureSession->metricsSnapshot();
                         QMetaObject::invokeMethod(
                             &waveformBridge,
                             [&waveformBridge, metrics] {
                                 waveformBridge.submitMetricsSnapshot(metrics);
                             },
                             Qt::QueuedConnection);
                         captureViewModel.updateIntegrityMetrics(
                             static_cast<qulonglong>(metrics.overflowCount));
                         captureSession->drainPendingSamples();
                         previewSession->drainPendingSamples();
                     });
    visualizationTimer.start();

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("appController", &appController);
    engine.rootContext()->setContextProperty("appSourceListModel", &sourceListModel);
    engine.rootContext()->setContextProperty("appWaveformBridge", &waveformBridge);
    engine.rootContext()->setContextProperty("appSettingsViewModel", &settingsViewModel);
    engine.rootContext()->setContextProperty("appCaptureViewModel", &captureViewModel);
    engine.load(QUrl(QStringLiteral("qrc:/Abracapture/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        visualizationTimer.stop();
        captureSession->stop();
        previewSession->stop();
        captureSession.reset();
        if (captureLoopStarted) {
            pw_thread_loop_stop(captureLoop);
        }
        if (captureCore != nullptr) {
            pw_core_disconnect(captureCore);
        }
        if (captureContext != nullptr) {
            pw_context_destroy(captureContext);
        }
        if (captureLoop != nullptr) {
            pw_thread_loop_destroy(captureLoop);
        }
        sourceCatalog.stop();
        pipeWireContext.stop();
        return 1;
    }

    const int result = application.exec();
    visualizationTimer.stop();
    captureSession->stop();
    previewSession->stop();
    if (recordingSinkOpen) {
        static_cast<void>(recordingSink.finalize());
        recordingSinkOpen = false;
    }
    captureSession.reset();
    if (captureLoopStarted) {
        pw_thread_loop_stop(captureLoop);
    }
    if (captureCore != nullptr) {
        pw_core_disconnect(captureCore);
    }
    if (captureContext != nullptr) {
        pw_context_destroy(captureContext);
    }
    if (captureLoop != nullptr) {
        pw_thread_loop_destroy(captureLoop);
    }
    sourceCatalog.stop();
    pipeWireContext.stop();
    return result;
}
