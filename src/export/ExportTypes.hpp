#pragma once

#include "AudioDomainTypes.hpp"

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <variant>

namespace abracapture::history { struct DerivedExport; }

namespace abracapture::exporting {

// ---------------------------------------------------------------------------
// Legacy ExportFormat — retained for migration and string interop
// ---------------------------------------------------------------------------

enum class ExportFormat {
    Mp3,
    Flac,
    Wav,
    OggVorbis,
};

[[nodiscard]] std::optional<ExportFormat> exportFormatFromString(const QString& format);
[[nodiscard]] QString exportFormatName(ExportFormat format);
[[nodiscard]] QString exportFileExtension(ExportFormat format);

[[nodiscard]] AudioContainer containerForLegacyFormat(ExportFormat format);
[[nodiscard]] AudioCodec codecForLegacyFormat(ExportFormat format);
[[nodiscard]] std::optional<ExportFormat> legacyFormatFor(AudioContainer container, AudioCodec codec);

// ---------------------------------------------------------------------------
// Sample-rate policy
// ---------------------------------------------------------------------------

enum class SampleRatePolicy {
    PreserveSource,
    Explicit,
};

struct SampleRateOption {
    SampleRatePolicy policy = SampleRatePolicy::PreserveSource;
    std::uint32_t sampleRate = 0;
};

// ---------------------------------------------------------------------------
// Collision policy
// ---------------------------------------------------------------------------

enum class CollisionPolicy {
    GenerateUniqueName,
    FailIfExists,
    ReplaceExisting,
};

// ---------------------------------------------------------------------------
// Codec-specific options
// ---------------------------------------------------------------------------

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

struct PcmOptions {
    AudioSampleFormat sampleFormat = AudioSampleFormat::SignedInt16;
};

struct VorbisOptions {
    int quality = 5;
};

struct AacOptions {
    std::uint32_t bitrateKbps = 256;
};

struct AlacOptions {
    std::uint32_t bitDepth = 24;
};

struct WavPackOptions {
    std::uint32_t bitDepth = 24;
};

struct OpusOptions {
    std::uint32_t bitrateKbps = 160;
};

struct Ac3Options {
    std::uint32_t bitrateKbps = 640;
};

struct Eac3Options {
    std::uint32_t bitrateKbps = 768;
};

using CodecOptions = std::variant<
    Mp3Options,
    FlacOptions,
    PcmOptions,
    VorbisOptions,
    AacOptions,
    AlacOptions,
    WavPackOptions,
    OpusOptions,
    Ac3Options,
    Eac3Options>;

// ---------------------------------------------------------------------------
// AudioEncodingTarget — container + codec + codec-specific options
// ---------------------------------------------------------------------------

struct AudioEncodingTarget {
    AudioContainer container = AudioContainer::Mp3;
    AudioCodec codec = AudioCodec::Mp3;
    SampleRateOption sampleRate;
    CodecOptions codecOptions = Mp3Options{};

    [[nodiscard]] bool validate(QString* error = nullptr) const;
    [[nodiscard]] QString fileExtension() const;
    [[nodiscard]] bool isLossless() const noexcept;
};

[[nodiscard]] AudioEncodingTarget defaultEncodingTarget(AudioContainer container, AudioCodec codec);
[[nodiscard]] bool supportsExplicitSampleRate(AudioContainer container, AudioCodec codec,
                                              std::uint32_t sampleRate);

// ---------------------------------------------------------------------------
// ExportProfile — encoding target + collision policy
// ---------------------------------------------------------------------------

struct ExportProfile {
    AudioEncodingTarget target;
    CollisionPolicy collisionPolicy = CollisionPolicy::GenerateUniqueName;

    [[nodiscard]] bool validate(QString* error = nullptr) const;
};

[[nodiscard]] ExportProfile defaultExportProfile(ExportFormat format);
[[nodiscard]] ExportProfile defaultExportProfile(AudioContainer container, AudioCodec codec);

// ---------------------------------------------------------------------------
// Automatic post-capture policy (Abracapture-specific)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Export request / job
// ---------------------------------------------------------------------------

struct ExportRequest {
    std::filesystem::path sourcePath;
    std::filesystem::path destinationPath;
    ExportProfile profile;
    QString captureSessionId;
    AudioStreamFormat sourceFormat;
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

// ---------------------------------------------------------------------------
// Legacy migration helpers
// ---------------------------------------------------------------------------

struct LegacyExportProfile {
    ExportFormat format = ExportFormat::Mp3;
    SampleRateOption sampleRate;
    std::variant<Mp3Options, FlacOptions, PcmOptions, VorbisOptions> options = Mp3Options{};
    CollisionPolicy collisionPolicy = CollisionPolicy::GenerateUniqueName;
};

[[nodiscard]] ExportProfile migrateProfile(const LegacyExportProfile& legacy);

[[nodiscard]] abracapture::history::DerivedExport createDerivedExportRecord(
    ExportJobId jobId,
    const ExportRequest& request,
    const QString& state,
    const QString& outputPath = {},
    const QString& error = {});

} // namespace abracapture::exporting
