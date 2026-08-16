#include "recording/OutputPathPolicy.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

class TemporaryOutputDirectory {
public:
    explicit TemporaryOutputDirectory(const char* name)
        : path_(std::filesystem::temp_directory_path() /
                (std::string("abracapture_output_path_policy_") + name)) {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    ~TemporaryOutputDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

std::chrono::system_clock::time_point fixedTimestamp() {
    return std::chrono::system_clock::time_point{std::chrono::seconds{1704067200}};
}

std::string localTimestampPart(const std::chrono::system_clock::time_point timestamp,
                               const char* format) {
    const std::time_t timeT = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tmBuffer{};
#if defined(_WIN32)
    localtime_s(&tmBuffer, &timeT);
#else
    localtime_r(&timeT, &tmBuffer);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tmBuffer, format);
    return stream.str();
}

abracapture::recording::CaptureMetadata metadataForTest() {
    abracapture::recording::CaptureMetadata metadata;
    metadata.source = "Studio Mic";
    metadata.sampleRate = 48000;
    metadata.channels = 2;
    metadata.counter = 7;
    metadata.timestamp = fixedTimestamp();
    return metadata;
}

} // namespace

TEST(OutputPathPolicy, ExpandsDateTimeSourceAndSequenceVariables) {
    TemporaryOutputDirectory temporary("expansion");
    const auto metadata = metadataForTest();
    abracapture::recording::OutputPathPolicy policy(
        temporary.path(), "{source}_{date}_{time}_{counter}");

    const auto result = policy.resolveOutputPath(metadata);
    const auto expectedName = metadata.source + "_" +
                              localTimestampPart(metadata.timestamp, "%Y-%m-%d") + "_" +
                              localTimestampPart(metadata.timestamp, "%H-%M-%S") + "_007.wav";

    EXPECT_EQ(result.filename().string(), expectedName);
    EXPECT_EQ(result.parent_path(), std::filesystem::absolute(temporary.path()));
}

TEST(OutputPathPolicy, CreatesAndUsesNestedBaseDirectory) {
    TemporaryOutputDirectory temporary("directory_assembly");
    const auto base = temporary.path() / "records" / "2024";
    auto metadata = metadataForTest();
    metadata.source = "Room A";
    abracapture::recording::OutputPathPolicy policy(base, "{source}", "flac");

    const auto result = policy.resolveOutputPath(metadata);

    EXPECT_TRUE(std::filesystem::is_directory(base));
    EXPECT_EQ(result.parent_path(), std::filesystem::absolute(base));
    EXPECT_EQ(result.filename().string(), "Room A.flac");
}

TEST(OutputPathPolicy, AvoidsExistingFilenameWithNumericSuffix) {
    TemporaryOutputDirectory temporary("collision");
    const auto metadata = metadataForTest();
    abracapture::recording::OutputPathPolicy policy(temporary.path(), "take");
    const auto first = policy.resolveOutputPath(metadata);

    {
        std::ofstream existing(first);
        existing << "occupied";
    }

    const auto second = policy.resolveOutputPath(metadata);
    EXPECT_EQ(second.filename().string(), "take_001.wav");
    EXPECT_NE(second, first);
}

TEST(OutputPathPolicy, RejectsUnknownTemplateVariables) {
    TemporaryOutputDirectory temporary("invalid_template");
    abracapture::recording::OutputPathPolicy policy(temporary.path(), "{not_a_variable}");

    EXPECT_THROW(policy.resolveOutputPath(metadataForTest()), std::invalid_argument);
}

TEST(OutputPathPolicy, ExpandsFormatRateAndChannelVariables) {
    TemporaryOutputDirectory temporary("metadata_variables");
    auto metadata = metadataForTest();
    metadata.format = ".FLAC";
    metadata.counter = 12;
    abracapture::recording::OutputPathPolicy policy(
        temporary.path(), "{samplerate}_{channels}_{format}_{sequence}", "wav");

    const auto result = policy.resolveOutputPath(metadata);

    EXPECT_EQ(result.filename().string(), "48000_2_flac_012.flac");
}

TEST(OutputPathPolicy, ValidateAcceptsEmptyAndExistingTemporaryDirectories) {
    TemporaryOutputDirectory temporary("validation");
    std::filesystem::create_directories(temporary.path());
    abracapture::recording::OutputPathPolicy emptyPolicy;
    abracapture::recording::OutputPathPolicy validPolicy(temporary.path());

    EXPECT_FALSE(emptyPolicy.validate());
    EXPECT_TRUE(validPolicy.validate());
}
