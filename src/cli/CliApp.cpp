#include "cli/CliApp.hpp"

#include "audio/AudioRingBuffer.hpp"
#include "audio/PipeWireCaptureStream.hpp"
#include "automation/AutoRecordController.hpp"
#include "core/AudioFormat.hpp"
#include "export/ExportManager.hpp"
#include "pipewire/PipeWireContext.hpp"
#include "pipewire/SourceCatalog.hpp"
#include "recording/CaptureIntegrity.hpp"
#include "recording/OutputPathPolicy.hpp"
#include "recording/RecordingSink.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <pipewire/context.h>
#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>
#include <spa/param/audio/raw.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_interrupted{false};

void signalHandler(int /*signal*/) {
    g_interrupted.store(true, std::memory_order_release);
}

std::string toLowerString(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

SampleFormat sampleFormatFromSpa(const spa_audio_format format) {
    switch (format) {
    case SPA_AUDIO_FORMAT_F32:
    case SPA_AUDIO_FORMAT_F32P:
        return SampleFormat::Float32;
    case SPA_AUDIO_FORMAT_F64:
    case SPA_AUDIO_FORMAT_F64P:
        return SampleFormat::Float64;
    case SPA_AUDIO_FORMAT_S32:
    case SPA_AUDIO_FORMAT_S32P:
        return SampleFormat::Int32;
    case SPA_AUDIO_FORMAT_S24:
    case SPA_AUDIO_FORMAT_S24P:
    case SPA_AUDIO_FORMAT_S24_32:
    case SPA_AUDIO_FORMAT_S24_32P:
        return SampleFormat::Int24;
    case SPA_AUDIO_FORMAT_S16:
    case SPA_AUDIO_FORMAT_S16P:
    default:
        return SampleFormat::Int16;
    }
}

} // namespace

int CliApp::run(int argc, char* argv[]) {
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }
    return parseArguments(arguments);
}

int CliApp::parseArguments(const QStringList& arguments) {
    if (arguments.size() < 2) {
        std::cerr << "Usage: abracapture <list-sources|record> [options]\n";
        return 1;
    }

    const QString subcommand = arguments.at(1);
    if (subcommand == QStringLiteral("list-sources")) {
        return listSources();
    }
    if (subcommand == QStringLiteral("record")) {
        QStringList recordArgs;
        recordArgs.append(arguments.at(0));
        for (int i = 2; i < arguments.size(); ++i) {
            recordArgs.append(arguments.at(i));
        }
        return runRecord(recordArgs);
    }
    if (subcommand == QStringLiteral("--help") || subcommand == QStringLiteral("-h")) {
        std::cout << "Abracapture - Linux audio capture utility\n\n"
                  << "Commands:\n"
                  << "  list-sources  List discovered PipeWire audio sources as JSON\n"
                  << "  record        Record from a PipeWire source to file\n";
        return 0;
    }

    std::cerr << "Unknown subcommand: " << subcommand.toStdString() << "\n";
    return 1;
}

int CliApp::listSources() {
    PipeWireContext ctx;
    SourceCatalog catalog;
    catalog.start(ctx);

    if (!ctx.start()) {
        std::cerr << "Error: Failed to initialize PipeWire context\n";
        return 3;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto& sources = catalog.sources();
    QJsonArray jsonArray;

    for (const auto& src : sources) {
        QJsonObject obj;
        std::string name = src.displayName;
        if (name.empty()) {
            name = src.appName.empty() ? src.nodeName : src.appName;
        }
        if (name.empty()) {
            name = "Unknown";
        }
        obj[QStringLiteral("name")] = QString::fromStdString(name);
        obj[QStringLiteral("id")] = static_cast<qint64>(src.nodeId);
        obj[QStringLiteral("media_class")] = QString::fromStdString(src.mediaClass);
        obj[QStringLiteral("activity")] = false;
        jsonArray.append(obj);
    }

    catalog.stop();
    ctx.stop();

    const QJsonDocument doc(jsonArray);
    std::cout << doc.toJson(QJsonDocument::Indented).toStdString();
    return 0;
}

int CliApp::runRecord(const QStringList& recordArgs) {
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Abracapture CLI Record"));

    const QCommandLineOption sourceOption(
        QStringList() << QStringLiteral("s") << QStringLiteral("source"),
        QStringLiteral("Source name or node ID to record from (required)."),
        QStringLiteral("source"));
    const QCommandLineOption outputOption(
        QStringList() << QStringLiteral("o") << QStringLiteral("output"),
        QStringLiteral("Output file path or destination directory."),
        QStringLiteral("output"));
    const QCommandLineOption formatOption(
        QStringList() << QStringLiteral("f") << QStringLiteral("format"),
        QStringLiteral("Master recording format: wav or flac (default: wav)."),
        QStringLiteral("format"), QStringLiteral("wav"));
    const QCommandLineOption durationOption(
        QStringList() << QStringLiteral("d") << QStringLiteral("duration"),
        QStringLiteral("Recording duration in seconds."),
        QStringLiteral("seconds"));
    const QCommandLineOption autoStartOption(
        QStringLiteral("auto-start"),
        QStringLiteral("Automatically start recording when audio level exceeds threshold."));
    const QCommandLineOption autoStopOption(
        QStringLiteral("auto-stop"),
        QStringLiteral("Automatically stop recording when audio level drops below threshold."));
    const QCommandLineOption preRollOption(
        QStringLiteral("pre-roll"),
        QStringLiteral("Pre-roll buffer duration in milliseconds."),
        QStringLiteral("ms"), QStringLiteral("0"));
    const QCommandLineOption postRollOption(
        QStringLiteral("post-roll"),
        QStringLiteral("Post-roll duration in milliseconds."),
        QStringLiteral("ms"), QStringLiteral("0"));
    const QCommandLineOption exportOption(
        QStringList() << QStringLiteral("e") << QStringLiteral("export"),
        QStringLiteral("Comma-separated export formats to generate after capture (e.g. mp3,flac,ogg)."),
        QStringLiteral("formats"));

    parser.addOption(sourceOption);
    parser.addOption(outputOption);
    parser.addOption(formatOption);
    parser.addOption(durationOption);
    parser.addOption(autoStartOption);
    parser.addOption(autoStopOption);
    parser.addOption(preRollOption);
    parser.addOption(postRollOption);
    parser.addOption(exportOption);
    parser.addHelpOption();

    if (!parser.parse(recordArgs)) {
        std::cerr << "Argument error: " << parser.errorText().toStdString() << "\n";
        return 1;
    }

    if (parser.isSet(QStringLiteral("help"))) {
        parser.showHelp();
        return 0;
    }

    if (!parser.isSet(sourceOption)) {
        std::cerr << "Argument error: --source is required\n";
        return 1;
    }

    const QString sourceArg = parser.value(sourceOption).trimmed();
    if (sourceArg.isEmpty()) {
        std::cerr << "Argument error: --source cannot be empty\n";
        return 1;
    }

    const QString formatArg = parser.value(formatOption).trimmed().toLower();
    if (formatArg != QStringLiteral("wav") && formatArg != QStringLiteral("flac")) {
        std::cerr << "Argument error: --format must be 'wav' or 'flac'\n";
        return 1;
    }

    double durationSeconds = 0.0;
    if (parser.isSet(durationOption)) {
        bool ok = false;
        durationSeconds = parser.value(durationOption).toDouble(&ok);
        if (!ok || durationSeconds <= 0.0) {
            std::cerr << "Argument error: --duration must be a positive number\n";
            return 1;
        }
    }

    int preRollMs = 0;
    if (parser.isSet(preRollOption)) {
        bool ok = false;
        preRollMs = parser.value(preRollOption).toInt(&ok);
        if (!ok || preRollMs < 0) {
            std::cerr << "Argument error: --pre-roll must be a non-negative integer\n";
            return 1;
        }
    }

    int postRollMs = 0;
    if (parser.isSet(postRollOption)) {
        bool ok = false;
        postRollMs = parser.value(postRollOption).toInt(&ok);
        if (!ok || postRollMs < 0) {
            std::cerr << "Argument error: --post-roll must be a non-negative integer\n";
            return 1;
        }
    }

    const bool isAutoStart = parser.isSet(autoStartOption);
    const bool isAutoStop = parser.isSet(autoStopOption);

    PipeWireContext ctx;
    SourceCatalog catalog;
    catalog.start(ctx);
    if (!ctx.start()) {
        std::cerr << "Capture error: Failed to connect to PipeWire\n";
        return 3;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto& sources = catalog.sources();
    std::optional<SourceDescriptor> selectedSource;

    bool idParsed = false;
    const std::uint32_t targetId = sourceArg.toUInt(&idParsed);
    if (idParsed) {
        selectedSource = catalog.sourceById(targetId);
    }

    if (!selectedSource) {
        const std::string lowerTarget = toLowerString(sourceArg.toStdString());
        for (const auto& candidate : sources) {
            if (toLowerString(candidate.displayName) == lowerTarget ||
                toLowerString(candidate.appName) == lowerTarget ||
                toLowerString(candidate.nodeName) == lowerTarget) {
                selectedSource = candidate;
                break;
            }
        }
    }

    if (!selectedSource) {
        const std::string lowerTarget = toLowerString(sourceArg.toStdString());
        for (const auto& candidate : sources) {
            if (toLowerString(candidate.displayName).find(lowerTarget) != std::string::npos ||
                toLowerString(candidate.appName).find(lowerTarget) != std::string::npos ||
                toLowerString(candidate.nodeName).find(lowerTarget) != std::string::npos) {
                selectedSource = candidate;
                break;
            }
        }
    }

    if (!selectedSource) {
        std::cerr << "Source not found: " << sourceArg.toStdString() << "\n";
        catalog.stop();
        ctx.stop();
        return 2;
    }

    const auto sourceFormatDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!selectedSource->sourceFormat && std::chrono::steady_clock::now() < sourceFormatDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        selectedSource = catalog.sourceById(selectedSource->nodeId);
        if (!selectedSource) {
            break;
        }
    }
    if (!selectedSource || !selectedSource->sourceFormat) {
        std::cerr << "Capture error: selected source has no current PipeWire format\n";
        catalog.stop();
        ctx.stop();
        return 3;
    }

    std::cerr << "[abracapture] source selection: target=node=" << selectedSource->nodeId
              << " node.name=" << selectedSource->nodeName
              << " app=" << selectedSource->appName << "\n";

    const SampleFormat fileSampleFormat =
        (formatArg == QStringLiteral("flac")) ? SampleFormat::Int16 : SampleFormat::Float32;

    abracapture::recording::OutputPathPolicy pathPolicy;
    if (parser.isSet(outputOption)) {
        const std::filesystem::path specifiedPath =
            parser.value(outputOption).toStdString();
        if (std::filesystem::is_directory(specifiedPath)) {
            pathPolicy.setBaseDirectory(specifiedPath);
            pathPolicy.setFilenameTemplate("{source}_{date}_{time}");
            pathPolicy.setDefaultFormat(formatArg.toStdString());
        } else {
            if (specifiedPath.has_parent_path()) {
                pathPolicy.setBaseDirectory(specifiedPath.parent_path());
            } else {
                pathPolicy.setBaseDirectory(std::filesystem::current_path());
            }
            pathPolicy.setFilenameTemplate(specifiedPath.stem().string());
            pathPolicy.setDefaultFormat(formatArg.toStdString());
        }
    } else {
        pathPolicy.setBaseDirectory(std::filesystem::current_path());
        pathPolicy.setFilenameTemplate("{source}_{date}_{time}");
        pathPolicy.setDefaultFormat(formatArg.toStdString());
    }

    abracapture::recording::RecordingSink sink;

    pw_thread_loop* const captureLoop = pw_thread_loop_new("abracapture-cli-stream", nullptr);
    if (captureLoop == nullptr) {
        catalog.stop();
        ctx.stop();
        return 3;
    }

    pw_context* const captureContext = pw_context_new(pw_thread_loop_get_loop(captureLoop), nullptr, 0);
    if (captureContext == nullptr) {
        pw_thread_loop_destroy(captureLoop);
        catalog.stop();
        ctx.stop();
        return 3;
    }

    pw_core* const captureCore = pw_context_connect(captureContext, nullptr, 0);
    if (captureCore == nullptr) {
        pw_context_destroy(captureContext);
        pw_thread_loop_destroy(captureLoop);
        catalog.stop();
        ctx.stop();
        return 3;
    }

    if (pw_thread_loop_start(captureLoop) < 0) {
        pw_core_disconnect(captureCore);
        pw_context_destroy(captureContext);
        pw_thread_loop_destroy(captureLoop);
        catalog.stop();
        ctx.stop();
        return 3;
    }

    abracapture::audio::AudioRingBuffer ringBuffer(65536 * 4);
    abracapture::audio::PipeWireCaptureStream stream(
        captureLoop, captureCore, ringBuffer, selectedSource->nodeId,
        *selectedSource->sourceFormat, *selectedSource->sourceFormat);

    if (!stream.start()) {
        std::cerr << "Capture error: Failed to start PipeWire stream\n";
        pw_thread_loop_stop(captureLoop);
        pw_core_disconnect(captureCore);
        pw_context_destroy(captureContext);
        pw_thread_loop_destroy(captureLoop);
        catalog.stop();
        ctx.stop();
        return 3;
    }

    const auto negotiationDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((stream.negotiatedFormat() == SPA_AUDIO_FORMAT_UNKNOWN ||
            stream.negotiatedSampleRate() == 0 || stream.negotiatedChannels() == 0) &&
           std::chrono::steady_clock::now() < negotiationDeadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const spa_audio_format negotiatedSpaFormat = stream.negotiatedFormat();
    const std::uint32_t captureRate = stream.negotiatedSampleRate();
    const std::uint32_t captureChannels = stream.negotiatedChannels();
    if (negotiatedSpaFormat == SPA_AUDIO_FORMAT_UNKNOWN || captureRate == 0 ||
        captureChannels == 0) {
        std::cerr << "Capture error: PipeWire did not negotiate a capture format\n";
        stream.stop();
        stream.destroy();
        pw_thread_loop_stop(captureLoop);
        pw_core_disconnect(captureCore);
        pw_context_destroy(captureContext);
        pw_thread_loop_destroy(captureLoop);
        catalog.stop();
        ctx.stop();
        return 3;
    }

    const abracapture::recording::CaptureMetadata captureMeta{
        .source = selectedSource->displayName.empty() ? selectedSource->appName : selectedSource->displayName,
        .sampleRate = captureRate,
        .channels = static_cast<std::uint16_t>(captureChannels),
        .format = formatArg.toStdString(),
    };
    const AudioFormat targetAudioFormat{
        .sampleRate = captureRate,
        .channels = static_cast<std::uint16_t>(captureChannels),
        .sampleFormat = fileSampleFormat,
    };
    if (!sink.open(targetAudioFormat, captureMeta, pathPolicy)) {
        std::cerr << "Capture error: Unable to open recording sink\n";
        stream.stop();
        stream.destroy();
        pw_thread_loop_stop(captureLoop);
        pw_core_disconnect(captureCore);
        pw_context_destroy(captureContext);
        pw_thread_loop_destroy(captureLoop);
        catalog.stop();
        ctx.stop();
        return 3;
    }

    AutoRecordController autoController;
    autoController.setPreRollDurationMs(preRollMs);
    autoController.setPostRollDurationMs(postRollMs);

    std::atomic<bool> isCapturingActive{!isAutoStart};
    std::atomic<bool> shouldStop{false};

    if (isAutoStart || isAutoStop) {
        QObject::connect(&autoController, &AutoRecordController::startRequested, [&]() {
            isCapturingActive.store(true, std::memory_order_release);
        });
        QObject::connect(&autoController, &AutoRecordController::automaticStopRequested, [&]() {
            shouldStop.store(true, std::memory_order_release);
        });
    }

    g_interrupted.store(false, std::memory_order_release);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::vector<float> sampleBuffer(4096 * captureChannels);
    std::vector<std::int32_t> intBuffer(4096 * captureChannels);

    auto startTime = std::chrono::steady_clock::now();
    bool captureTimerStarted = !isAutoStart;

    while (!g_interrupted.load(std::memory_order_acquire) &&
           !shouldStop.load(std::memory_order_acquire)) {
        QCoreApplication::processEvents();

        const std::size_t available = ringBuffer.available();
        if (available == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (captureTimerStarted && durationSeconds > 0.0) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                    std::chrono::steady_clock::now() - startTime);
                if (elapsed.count() >= durationSeconds) {
                    break;
                }
            }
            continue;
        }

        const std::size_t framesToRead = std::min(sampleBuffer.size() / captureChannels, available / captureChannels);
        if (framesToRead == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        const std::size_t readCount = ringBuffer.read(sampleBuffer.data(), framesToRead * captureChannels);
        const std::size_t readFrames = readCount / captureChannels;

        if (readFrames > 0) {
            float peak = 0.0F;
            for (std::size_t i = 0; i < readCount; ++i) {
                peak = std::max(peak, std::fabs(sampleBuffer[i]));
            }
            if (isAutoStart || isAutoStop) {
                autoController.feedLevel(peak);
            }

            if (isCapturingActive.load(std::memory_order_acquire)) {
                if (!captureTimerStarted) {
                    captureTimerStarted = true;
                    startTime = std::chrono::steady_clock::now();
                }

                if (formatArg == QStringLiteral("flac")) {
                    for (std::size_t i = 0; i < readCount; ++i) {
                        const float clamped = std::clamp(sampleBuffer[i], -1.0F, 1.0F);
                        intBuffer[i] = static_cast<std::int32_t>(clamped * 32767.0F);
                    }
                    if (!sink.pushSamples(intBuffer.data(), readFrames)) {
                        std::cerr << "Capture error: Failed to push FLAC samples\n";
                        break;
                    }
                } else {
                    if (!sink.pushSamples(sampleBuffer.data(), readFrames)) {
                        std::cerr << "Capture error: Failed to push WAV samples\n";
                        break;
                    }
                }
            }
        }

        if (captureTimerStarted && durationSeconds > 0.0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::steady_clock::now() - startTime);
            if (elapsed.count() >= durationSeconds) {
                break;
            }
        }
    }

    stream.stop();
    stream.destroy();

    pw_thread_loop_stop(captureLoop);
    pw_core_disconnect(captureCore);
    pw_context_destroy(captureContext);
    pw_thread_loop_destroy(captureLoop);

    while (ringBuffer.available() >= captureChannels &&
           isCapturingActive.load(std::memory_order_acquire)) {
        const std::size_t framesToRead = std::min(sampleBuffer.size() / captureChannels,
                                                  ringBuffer.available() / captureChannels);
        if (framesToRead == 0) {
            break;
        }
        const std::size_t readCount = ringBuffer.read(sampleBuffer.data(), framesToRead * captureChannels);
        const std::size_t readFrames = readCount / captureChannels;
        if (formatArg == QStringLiteral("flac")) {
            for (std::size_t i = 0; i < readCount; ++i) {
                const float clamped = std::clamp(sampleBuffer[i], -1.0F, 1.0F);
                intBuffer[i] = static_cast<std::int32_t>(clamped * 32767.0F);
            }
            sink.pushSamples(intBuffer.data(), readFrames);
        } else {
            sink.pushSamples(sampleBuffer.data(), readFrames);
        }
    }

    const auto finalizeResult = sink.finalize();
    catalog.stop();
    ctx.stop();

    if (!finalizeResult.success) {
        std::cerr << "Capture error: " << finalizeResult.error << "\n";
        return 3;
    }

    abracapture::recording::CaptureIntegrity integrity;
    integrity.sourceStream = captureMeta.source;
    integrity.setNegotiatedFormat(
        sampleFormatFromSpa(negotiatedSpaFormat), captureRate);
    integrity.setFileFormat(finalizeResult.sampleFormat, finalizeResult.sampleRate);
    integrity.totalFrames = finalizeResult.totalFrames;
    integrity.durationSeconds = finalizeResult.duration.count();
    integrity.filePath = finalizeResult.finalPath;

    std::cout << integrity.summary() << "\n";

    if (parser.isSet(exportOption)) {
        const QString exportFormatsRaw = parser.value(exportOption).trimmed();
        const QStringList formatList = exportFormatsRaw.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (!formatList.isEmpty()) {
            abracapture::exporting::ExportManager exportManager;
            if (!exportManager.verifyMaster(finalizeResult.finalPath, integrity)) {
                std::cerr << "Export error: Master verification failed before export\n";
                return 4;
            }

            QList<abracapture::exporting::ExportSpec> exportSpecs;
            for (const QString& fmt : formatList) {
                const QString cleanFmt = fmt.trimmed().toLower();
                if (cleanFmt.isEmpty()) {
                    continue;
                }
                abracapture::exporting::ExportSpec spec;
                spec.format = cleanFmt;
                spec.sampleRate = finalizeResult.sampleRate;
                spec.bitDepth = 16;
                std::filesystem::path expPath = finalizeResult.finalPath;
                expPath.replace_extension("." + cleanFmt.toStdString());
                spec.outputPath = QString::fromStdString(expPath.string());
                exportSpecs.append(spec);
            }

            if (!exportSpecs.isEmpty()) {
                bool exportFailed = false;
                QString exportErrorMessage;

                QEventLoop loop;
                QObject::connect(&exportManager, &abracapture::exporting::ExportManager::exportFailed,
                                 [&](const QString& format, const QString& error) {
                                     exportFailed = true;
                                     exportErrorMessage = QStringLiteral("Export %1 failed: %2").arg(format, error);
                                     std::cerr << exportErrorMessage.toStdString() << "\n";
                                 });
                QObject::connect(&exportManager, &abracapture::exporting::ExportManager::allExportsFinished,
                                 &loop, &QEventLoop::quit);

                exportManager.startExports(finalizeResult.finalPath, integrity, exportSpecs);
                loop.exec();

                if (exportFailed) {
                    return 4;
                }
            }
        }
    }

    return 0;
}
