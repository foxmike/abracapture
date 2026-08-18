#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <variant>

namespace abracapture::exporting {

enum class ExportFormat {
    Mp3,
    Flac,
    Wav,
    OggVorbis,
};

[[nodiscard]] std::optional<ExportFormat> exportFormatFromString(const QString& format);
[[nodiscard]] QString exportFormatName(ExportFormat format);
[[nodiscard]] QString exportFileExtension(ExportFormat format);
[[nodiscard]] bool supportsExplicitSampleRate(ExportFormat format, std::uint32_t sampleRate);

enum class SampleRatePolicy {
    PreserveSource,
    Explicit,
};

struct SampleRateOption {
    SampleRatePolicy policy = SampleRatePolicy::PreserveSource;
    std::uint32_t sampleRate = 0;
};

enum class CollisionPolicy {
    GenerateUniqueName,
    FailIfExists,
    ReplaceExisting,
};

enum class Mp3BitrateMode {
    Constant,
    Variable,
};

struct Mp3Options {
    std::uint32_t bitrateKbps = 320;
    Mp3BitrateMode bitrateMode = Mp3BitrateMode::Constant;
};

struct FlacOptions {
    std::uint32_t bitDepth = 24;
    int compressionLevel = 5;
};

enum class WavSampleFormat {
    Unsigned8,
    Signed16,
    Signed24,
    Signed32,
    Float32,
};

struct WavOptions {
    WavSampleFormat sampleFormat = WavSampleFormat::Signed16;
};

struct OggVorbisOptions {
    int quality = 5;
};

using FormatSpecificOptions = std::variant<Mp3Options, FlacOptions, WavOptions, OggVorbisOptions>;

struct ExportProfile {
    ExportFormat format = ExportFormat::Mp3;
    SampleRateOption sampleRate;
    FormatSpecificOptions options = Mp3Options{};
    CollisionPolicy collisionPolicy = CollisionPolicy::GenerateUniqueName;

    [[nodiscard]] bool validate(QString* error = nullptr) const;
};

[[nodiscard]] ExportProfile defaultExportProfile(ExportFormat format);

enum class AutomaticExportDestinationPolicy {
    SameFolder,
    ExportsSubfolder,
    CustomFolder,
};

struct AutomaticExportProfile {
    bool enabled = false;
    ExportProfile profile = defaultExportProfile(ExportFormat::Mp3);
    bool keepMaster = true;
    AutomaticExportDestinationPolicy destinationPolicy =
        AutomaticExportDestinationPolicy::SameFolder;
    QString customDestinationDirectory;
};

struct ExportRequest {
    std::filesystem::path sourcePath;
    std::filesystem::path destinationPath;
    ExportProfile profile;
    QString captureSessionId;
    std::uint32_t sourceSampleRate = 0;
};

using ExportJobId = quint64;

enum class ExportJobState {
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled,
};

struct ExportJob {
    ExportJobId id = 0;
    ExportRequest request;
    ExportJobState state = ExportJobState::Queued;
    QDateTime queuedAt;
    QDateTime startedAt;
    QDateTime finishedAt;
    std::filesystem::path temporaryOutputPath;
    std::filesystem::path resultPath;
    QString error;
    bool destinationReserved = false;
};

} // namespace abracapture::exporting
