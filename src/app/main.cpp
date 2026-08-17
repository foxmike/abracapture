#include "AppController.hpp"
#include "audio/CaptureSession.hpp"
#include "cli/CliApp.hpp"
#include "pipewire/PipeWireContext.hpp"
#include "pipewire/SourceCatalog.hpp"
#include "pipewire/SourceDescriptor.hpp"
#include "history/RecordingIndex.hpp"
#include "recording/OutputPathPolicy.hpp"
#include "recording/RecordingSink.hpp"
#include "ui/CaptureViewModel.hpp"
#include "ui/RecordingsViewModel.hpp"
#include "ui/SettingsViewModel.hpp"
#include "ui/SourceListModel.hpp"
#include "ui/WaveformBridge.hpp"

#include <pipewire/context.h>
#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>

#include <QCoreApplication>
#include <QApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
        .bitDepth = 0,
        .isActive = false,
        .mediaClass = QString::fromStdString(descriptor.mediaClass),
        .nodeName = QString::fromStdString(descriptor.nodeName),
    };
}

int spaBitDepth(const spa_audio_format format) {
    switch (format) {
    case SPA_AUDIO_FORMAT_S8: case SPA_AUDIO_FORMAT_U8: return 8;
    case SPA_AUDIO_FORMAT_S16: case SPA_AUDIO_FORMAT_S16P: return 16;
    case SPA_AUDIO_FORMAT_S24: case SPA_AUDIO_FORMAT_S24P:
    case SPA_AUDIO_FORMAT_S24_32: case SPA_AUDIO_FORMAT_S24_32P: return 24;
    case SPA_AUDIO_FORMAT_S32: case SPA_AUDIO_FORMAT_S32P:
    case SPA_AUDIO_FORMAT_F32: case SPA_AUDIO_FORMAT_F32P:
    case SPA_AUDIO_FORMAT_F64: case SPA_AUDIO_FORMAT_F64P: return 32;
    default: return 0;
    }
}

QString negotiatedFormatDescription(const int sampleRate, const int channels, const int bitDepth) {
    if (sampleRate <= 0 || channels <= 0 || bitDepth <= 0) {
        return QStringLiteral("Waiting for PipeWire format…");
    }
    const QString channelDescription = channels == 1
                                           ? QStringLiteral("mono")
                                           : QStringLiteral("%1 ch").arg(channels);
    const double bitrateMbps = static_cast<double>(sampleRate) * channels * bitDepth / 1000000.0;
    return QStringLiteral("%1 kHz · %2 · %3-bit PCM · %4 Mbps")
        .arg(static_cast<double>(sampleRate) / 1000.0, 0, 'f', 1)
        .arg(channelDescription)
        .arg(bitDepth)
        .arg(bitrateMbps, 0, 'f', 3);
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
    abracapture::history::RecordingIndex recordingIndex;
    static_cast<void>(recordingIndex.load());
    recordingIndex.importDirectory(settingsViewModel.outputDirectory());
    RecordingsViewModel recordingsViewModel(&recordingIndex);
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

    pw_thread_loop* monitorLoop = pw_thread_loop_new("abracapture-gui-monitor", nullptr);
    pw_context* monitorContext = monitorLoop != nullptr
                                      ? pw_context_new(pw_thread_loop_get_loop(monitorLoop), nullptr, 0)
                                      : nullptr;
    pw_core* monitorCore = monitorContext != nullptr
                                ? pw_context_connect(monitorContext, nullptr, 0)
                                : nullptr;
    const bool monitorLoopStarted = monitorCore != nullptr &&
                                     pw_thread_loop_start(monitorLoop) >= 0;
    if (!monitorLoopStarted) {
        if (monitorCore != nullptr) {
            pw_core_disconnect(monitorCore);
            monitorCore = nullptr;
        }
        if (monitorContext != nullptr) {
            pw_context_destroy(monitorContext);
            monitorContext = nullptr;
        }
        if (monitorLoop != nullptr) {
            pw_thread_loop_destroy(monitorLoop);
            monitorLoop = nullptr;
        }
    }

    abracapture::recording::OutputPathPolicy outputPathPolicy;
    abracapture::recording::RecordingSink recordingSink;
    bool recordingSinkOpen = false;
    bool recordingUsesFlac = false;
    bool recordingActive = false;
    QString recordingSourceName;
    std::uint16_t recordingChannels = 0;
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

    std::atomic<bool> monitorActive{false};
    bool monitorDataSeen = false;
    auto monitorSession = std::make_unique<abracapture::audio::CaptureSession>(
        monitorLoop, monitorCore, sourceCatalog, 16384,
        [&monitorActive](const abracapture::audio::SessionState state) {
            monitorActive.store(state == abracapture::audio::SessionState::Capturing,
                               std::memory_order_release);
        });
    monitorSession->setMaxReconnectAttempts(2);

    QObject::connect(&sourceListModel, &SourceListModel::sourceSelected,
                     &application, [&monitorSession, &sourceListModel, &recordingActive,
                                    &monitorDataSeen](const quint32 sourceId) {
                         if (recordingActive) {
                             return;
                         }
                         monitorDataSeen = false;
                         if (sourceId == 0) {
                             monitorSession->stop();
                             return;
                         }
                         const QVariantMap source = sourceListModel.getSourceById(sourceId);
                         const std::uint32_t rate = static_cast<std::uint32_t>(
                             source.value(QStringLiteral("sampleRate"), 48000).toUInt());
                         const std::uint32_t channels = static_cast<std::uint32_t>(
                             source.value(QStringLiteral("channelCount"), 2).toUInt());
                         fprintf(stderr, "[abracapture] monitor: connecting node=%u rate=%u ch=%u\n",
                                 sourceId, rate, channels);
                         static_cast<void>(monitorSession->begin(sourceId, SPA_AUDIO_FORMAT_F32, rate, channels));
                     });

    QObject::connect(&captureViewModel, &CaptureViewModel::startCaptureRequested,
                     &application, [&captureSession, &captureViewModel, &sourceListModel,
                                    &sourceCatalog, &settingsViewModel, &recordingSink,
                                    &outputPathPolicy, &recordingSinkOpen, &recordingUsesFlac,
                                    &recordingActive, &monitorSession, &monitorActive,
                                    &recordingSourceName, &recordingChannels](
                                       const quint32 sourceId) {
                         const QVariantMap source = sourceListModel.getSourceById(sourceId);
                         const auto sourceDescriptor = sourceCatalog.sourceById(sourceId);
                         if (!sourceDescriptor) {
                             captureViewModel.reportCaptureError(
                                 QStringLiteral("Selected audio source is no longer available."));
                             return;
                         }
                         const bool autoRate = settingsViewModel.captureSampleRate() == 0;
                         const bool autoDepth = settingsViewModel.captureBitDepth() == 0;
                         const auto monitorRate = monitorSession->negotiatedSampleRate();
                         const auto monitorFormat = monitorSession->negotiatedFormat();
                         const auto monitorChannels = monitorSession->negotiatedChannels();
                         const std::uint32_t rate = autoRate
                                                        ? (monitorRate != 0 ? monitorRate : static_cast<std::uint32_t>(source.value(QStringLiteral("sampleRate"), 48000).toUInt()))
                                                        : static_cast<std::uint32_t>(settingsViewModel.captureSampleRate());
                         const std::uint32_t channels = monitorChannels != 0
                                                            ? monitorChannels
                                                            : static_cast<std::uint32_t>(source.value(QStringLiteral("channelCount"), 2).toUInt());
                         const int outputBitDepth = autoDepth ? std::max(16, spaBitDepth(monitorFormat)) : settingsViewModel.captureBitDepth();
                         const spa_audio_format inputFormat = autoRate && autoDepth && monitorFormat != SPA_AUDIO_FORMAT_UNKNOWN
                                                                  ? monitorFormat
                                                                  : SPA_AUDIO_FORMAT_F32;

                         monitorActive.store(false, std::memory_order_release);
                         monitorSession->stop();
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
                             .sampleFormat = recordingUsesFlac
                                                 ? (outputBitDepth == 16 ? SampleFormat::Int16
                                                                          : (outputBitDepth == 24 ? SampleFormat::Int24 : SampleFormat::Int32))
                                                 : (outputBitDepth == 16 ? SampleFormat::Int16
                                                                          : (outputBitDepth == 24 ? SampleFormat::Int24 : SampleFormat::Float32))};
                         const abracapture::recording::CaptureMetadata metadata{
                             .source = sourceDescriptor->displayName.empty()
                                           ? sourceDescriptor->appName
                                           : sourceDescriptor->displayName,
                             .sampleRate = rate,
                             .channels = static_cast<std::uint16_t>(channels),
                             .format = masterFormat.toStdString(),
                         };
                         recordingSourceName = QString::fromStdString(metadata.source);
                         recordingChannels = metadata.channels;
                         recordingSink.abort();
                         recordingSinkOpen = recordingSink.open(fileFormat, metadata,
                                                                  outputPathPolicy);
                         if (!recordingSinkOpen) {
                             captureViewModel.reportCaptureError(
                                 QStringLiteral("Unable to open recording file: %1")
                                     .arg(QString::fromStdString(recordingSink.lastError())));
                             recordingActive = false;
                             return;
                         }

                         captureViewModel.updateEngineFormat(
                             negotiatedFormatDescription(static_cast<int>(rate), static_cast<int>(channels), outputBitDepth), CaptureViewModel::Native);
                         if (!captureSession->begin(sourceId, inputFormat, rate, channels)) {
                             recordingSink.abort();
                             recordingSinkOpen = false;
                             recordingActive = false;
                             captureViewModel.reportCaptureError(
                                 QStringLiteral("PipeWire could not start capture at %1 Hz, %2 channels.")
                                     .arg(rate)
                                     .arg(channels));
                         }
                     });
    QObject::connect(&captureViewModel, &CaptureViewModel::stopCaptureRequested,
                     &application, [&captureSession, &captureViewModel, &recordingSink,
                     &recordingSinkOpen, &recordingActive, &recordingIndex,
                                    &recordingsViewModel, &recordingSourceName,
                                    &recordingChannels, &sourceListModel, &monitorSession,
                                    &monitorDataSeen] {
                         captureSession->stop();
                         if (recordingSinkOpen) {
                             const auto result = recordingSink.finalize();
                             recordingSinkOpen = false;
                             if (!result.success) {
                                 captureViewModel.reportCaptureError(
                                     QStringLiteral("Recording could not be finalized: %1")
                                         .arg(QString::fromStdString(result.error)));
                             } else {
                                 abracapture::history::RecordingEntry entry;
                                 entry.filePath = QString::fromStdString(result.finalPath.string());
                                 entry.format = QString::fromStdString(
                                     result.finalPath.extension().string()).toLower();
                                 if (entry.format.startsWith(QLatin1Char('.'))) {
                                     entry.format.remove(0, 1);
                                 }
                                 entry.sampleRate = result.sampleRate;
                                 entry.bitDepth = result.sampleFormat == SampleFormat::Float32
                                                      ? 32
                                                      : (result.sampleFormat == SampleFormat::Int16
                                                             ? 16
                                                             : (result.sampleFormat == SampleFormat::Int24 ? 24 : 32));
                                 entry.channels = recordingChannels;
                                 entry.duration = result.duration.count();
                                 entry.timestamp = QFileInfo(entry.filePath).lastModified();
                                 entry.isNative = result.provenance == ConversionProvenance::Native;
                                 recordingIndex.append(entry);
                                 recordingsViewModel.refreshEntries();
                             }
                         }
                         recordingActive = false;
                         const quint32 selectedId = sourceListModel.selectedSourceId();
                         if (selectedId != 0) {
                             const QVariantMap src = sourceListModel.getSourceById(selectedId);
                             const std::uint32_t rate = static_cast<std::uint32_t>(
                                 src.value(QStringLiteral("sampleRate"), 48000).toUInt());
                             const std::uint32_t channels = static_cast<std::uint32_t>(
                                 src.value(QStringLiteral("channelCount"), 2).toUInt());
                             monitorDataSeen = false;
                             static_cast<void>(monitorSession->begin(selectedId, SPA_AUDIO_FORMAT_F32, rate, channels));
                         }
                     });

    QTimer visualizationTimer;
    visualizationTimer.setInterval(33);
    QObject::connect(&visualizationTimer, &QTimer::timeout,
                     &application, [&captureSession, &monitorSession, &monitorActive,
                                    &recordingActive, &monitorDataSeen,
                                    &waveformBridge, &captureViewModel, &sourceListModel] {
                         const bool monitoring = monitorActive.load(std::memory_order_acquire);
                         auto* activeSession = recordingActive
                                                   ? captureSession.get()
                                                   : (monitoring ? monitorSession.get() : nullptr);

                         if (activeSession != nullptr) {
                             abracapture::audio::CaptureSession::CaptureVisualizationSnapshot snapshot;
                             if (activeSession->visualizationSnapshot(snapshot)) {
                                 if (monitoring && !monitorDataSeen) {
                                     monitorDataSeen = true;
                                     fprintf(stderr, "[abracapture] monitor: first audio data received\n");
                                 }
                                 float peak = 0.0F;
                                 QVector<float> samples;
                                 samples.reserve(static_cast<qsizetype>(snapshot.sampleCount));
                                 for (std::size_t index = 0; index < snapshot.sampleCount; ++index) {
                                     peak = std::max(peak, std::abs(snapshot.samples[index]));
                                     samples.append(snapshot.samples[index]);
                                 }
                                 captureViewModel.feedAudioLevel(peak);
                                 QMetaObject::invokeMethod(
                                     &waveformBridge,
                                     [&waveformBridge, samples = std::move(samples)]() mutable {
                                         waveformBridge.submitVisualizationSnapshot(std::move(samples));
                                     },
                                     Qt::QueuedConnection);
                             }
                         }

                         if (monitoring && !recordingActive) {
                             const auto rate = monitorSession->negotiatedSampleRate();
                             const auto channels = monitorSession->negotiatedChannels();
                             const auto format = monitorSession->negotiatedFormat();
                             const int depth = spaBitDepth(format);
                             if (rate > 0 && channels > 0 && depth > 0) {
                                 sourceListModel.updateNegotiatedFormat(
                                     sourceListModel.selectedSourceId(),
                                     static_cast<int>(rate), depth,
                                     static_cast<int>(channels));
                                 captureViewModel.updateEngineFormat(
                                     negotiatedFormatDescription(
                                         static_cast<int>(rate),
                                         static_cast<int>(channels), depth),
                                     CaptureViewModel::Native);
                             }
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
                         if (monitoring) {
                             monitorSession->drainPendingSamples();
                         }
                     });
    visualizationTimer.start();

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("appController", &appController);
    engine.rootContext()->setContextProperty("appSourceListModel", &sourceListModel);
    engine.rootContext()->setContextProperty("appWaveformBridge", &waveformBridge);
    engine.rootContext()->setContextProperty("appSettingsViewModel", &settingsViewModel);
    engine.rootContext()->setContextProperty("appCaptureViewModel", &captureViewModel);
    engine.rootContext()->setContextProperty("appRecordingsViewModel", &recordingsViewModel);
    engine.load(QUrl(QStringLiteral("qrc:/Abracapture/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        visualizationTimer.stop();
        captureSession->stop();
        monitorSession->stop();
        captureSession.reset();
        monitorSession.reset();
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
        if (monitorLoopStarted) {
            pw_thread_loop_stop(monitorLoop);
        }
        if (monitorCore != nullptr) {
            pw_core_disconnect(monitorCore);
        }
        if (monitorContext != nullptr) {
            pw_context_destroy(monitorContext);
        }
        if (monitorLoop != nullptr) {
            pw_thread_loop_destroy(monitorLoop);
        }
        sourceCatalog.stop();
        pipeWireContext.stop();
        return 1;
    }

    const int result = application.exec();
    visualizationTimer.stop();
    captureSession->stop();
    monitorSession->stop();
    if (recordingSinkOpen) {
        static_cast<void>(recordingSink.finalize());
        recordingSinkOpen = false;
    }
    captureSession.reset();
    monitorSession.reset();
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
    if (monitorLoopStarted) {
        pw_thread_loop_stop(monitorLoop);
    }
    if (monitorCore != nullptr) {
        pw_core_disconnect(monitorCore);
    }
    if (monitorContext != nullptr) {
        pw_context_destroy(monitorContext);
    }
    if (monitorLoop != nullptr) {
        pw_thread_loop_destroy(monitorLoop);
    }
    sourceCatalog.stop();
    pipeWireContext.stop();
    return result;
}
