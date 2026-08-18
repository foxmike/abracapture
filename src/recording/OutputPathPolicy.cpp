#include "OutputPathPolicy.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace abracapture::recording {

namespace {

std::string trimAsciiWhitespace(std::string value) {
    const auto isWhitespace = [](const unsigned char character) {
        return std::isspace(character) != 0;
    };
    while (!value.empty() && isWhitespace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isWhitespace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool isPlaceholderName(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char character : value) {
        normalized.push_back(static_cast<char>(std::tolower(character)));
    }
    return normalized.empty() || normalized == "." || normalized == ".." ||
           normalized == "unknown" || normalized == "unknown album" ||
           normalized == "untitled" || normalized == "null" || normalized == "n/a";
}

std::string sanitizeSource(std::string_view source) {
    const std::string sanitized = OutputPathPolicy::sanitizePathComponent(source);
    return sanitized.empty() ? "capture" : sanitized;
}

std::string usableMetadataComponent(std::string_view value) {
    const std::string sanitized = OutputPathPolicy::sanitizePathComponent(value);
    return isPlaceholderName(sanitized) ? std::string{} : sanitized;
}

std::string metadataTrackName(const CaptureMetadata& metadata) {
    if (const std::string title = usableMetadataComponent(metadata.mediaTitle); !title.empty()) {
        return title;
    }
    return usableMetadataComponent(metadata.mediaName);
}

std::string metadataAlbumName(const CaptureMetadata& metadata) {
    return usableMetadataComponent(metadata.mediaAlbum);
}

std::string normalizeFormat(std::string_view format, std::string_view defaultFormat) {
    std::string_view fmt = !format.empty() ? format : defaultFormat;
    if (fmt.starts_with('.')) {
        fmt.remove_prefix(1);
    }
    if (fmt.empty()) {
        return "wav";
    }
    std::string result;
    result.assign(fmt);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

} // namespace

std::string OutputPathPolicy::sanitizePathComponent(std::string_view component) {
    std::string result;
    result.reserve(component.size());
    for (const char character : component) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (character == '/' || character == '\\' || character == ':' || character == '*' ||
            character == '?' || character == '"' || character == '<' || character == '>' ||
            character == '|' || byte < 32 || byte == 127) {
            result.push_back('_');
        } else {
            result.push_back(character);
        }
    }
    result = trimAsciiWhitespace(std::move(result));
    while (!result.empty() && (result.back() == '.' || result.back() == ' ')) {
        result.pop_back();
    }
    if (result == "." || result == "..") {
        return {};
    }

    constexpr std::size_t kMaximumComponentBytes = 120;
    if (result.size() > kMaximumComponentBytes) {
        std::size_t limit = kMaximumComponentBytes;
        while (limit > 0 && (static_cast<unsigned char>(result[limit]) & 0xC0U) == 0x80U) {
            --limit;
        }
        result.resize(limit);
        while (!result.empty() && (result.back() == '.' || result.back() == ' ')) {
            result.pop_back();
        }
    }
    return result;
}

OutputPathPolicy::OutputPathPolicy(std::filesystem::path baseDirectory,
                                   std::string filenameTemplate,
                                   std::string defaultFormat)
    : baseDirectory_(std::move(baseDirectory)),
      filenameTemplate_(std::move(filenameTemplate)),
      defaultFormat_(std::move(defaultFormat)) {}

const std::filesystem::path& OutputPathPolicy::baseDirectory() const noexcept {
    return baseDirectory_;
}

void OutputPathPolicy::setBaseDirectory(std::filesystem::path baseDirectory) {
    baseDirectory_ = std::move(baseDirectory);
}

const std::string& OutputPathPolicy::filenameTemplate() const noexcept {
    return filenameTemplate_;
}

void OutputPathPolicy::setFilenameTemplate(std::string filenameTemplate) {
    filenameTemplate_ = std::move(filenameTemplate);
}

const std::string& OutputPathPolicy::defaultFormat() const noexcept {
    return defaultFormat_;
}

void OutputPathPolicy::setDefaultFormat(std::string defaultFormat) {
    defaultFormat_ = std::move(defaultFormat);
}

bool OutputPathPolicy::validate() const {
    if (baseDirectory_.empty()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::path target = baseDirectory_;

    if (std::filesystem::exists(target, ec)) {
        if (!std::filesystem::is_directory(target, ec)) {
            return false;
        }

        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        const auto probe = target / (".abracapture_probe_" + std::to_string(now));
        {
            std::ofstream ofs(probe);
            if (!ofs.is_open()) {
                return false;
            }
            ofs << "probe";
        }
        std::filesystem::remove(probe, ec);
        return true;
    }

    std::filesystem::path ancestor = target.parent_path();
    while (!ancestor.empty() && !std::filesystem::exists(ancestor, ec)) {
        const auto next = ancestor.parent_path();
        if (next == ancestor) {
            break;
        }
        ancestor = next;
    }

    if (ancestor.empty() || !std::filesystem::exists(ancestor, ec)) {
        ancestor = std::filesystem::current_path();
    }

    if (!std::filesystem::is_directory(ancestor, ec)) {
        return false;
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    const auto probe = ancestor / (".abracapture_probe_" + std::to_string(now));
    {
        std::ofstream ofs(probe);
        if (!ofs.is_open()) {
            return false;
        }
        ofs << "probe";
    }
    std::filesystem::remove(probe, ec);
    return true;
}

std::filesystem::path OutputPathPolicy::resolveOutputPath(const CaptureMetadata& metadata) const {
    const std::string fmt = normalizeFormat(metadata.format, defaultFormat_);
    const std::string sourceStr = sanitizeSource(metadata.source);
    const bool useMetadataNames = metadata.automatedCapture && metadata.useMediaMetadataNames;
    const std::string trackName = useMetadataNames ? metadataTrackName(metadata) : std::string{};
    const std::string albumName = useMetadataNames ? metadataAlbumName(metadata) : std::string{};

    const std::time_t timeT = std::chrono::system_clock::to_time_t(metadata.timestamp);
    std::tm tmBuffer{};
#if defined(_WIN32)
    localtime_s(&tmBuffer, &timeT);
#else
    localtime_r(&timeT, &tmBuffer);
#endif

    std::ostringstream dateStream;
    dateStream << std::put_time(&tmBuffer, "%Y-%m-%d");
    const std::string dateStr = dateStream.str();

    std::ostringstream timeStream;
    timeStream << std::put_time(&tmBuffer, "%H-%M-%S");
    const std::string timeStr = timeStream.str();

    const std::string datetimeStr = dateStr + "_" + timeStr;

    const std::string sampleRateStr = std::to_string(metadata.sampleRate);
    const std::string channelsStr = std::to_string(metadata.channels);

    const std::uint64_t count = metadata.counter != 0 ? metadata.counter : 1;
    std::ostringstream counterStream;
    counterStream << std::setw(3) << std::setfill('0') << count;
    const std::string counterStr = counterStream.str();

    // Placeholder replacement
    std::string expanded;
    expanded.reserve(filenameTemplate_.size() + 64);

    for (std::size_t i = 0; i < filenameTemplate_.size();) {
        if (filenameTemplate_[i] == '{') {
            const auto closePos = filenameTemplate_.find('}', i + 1);
            if (closePos == std::string::npos) {
                expanded.push_back(filenameTemplate_[i]);
                ++i;
                continue;
            }

            const std::string_view tag(filenameTemplate_.data() + i + 1, closePos - (i + 1));
            if (tag == "date") {
                expanded += dateStr;
            } else if (tag == "time") {
                expanded += timeStr;
            } else if (tag == "datetime") {
                expanded += datetimeStr;
            } else if (tag == "source") {
                expanded += sourceStr;
            } else if (tag == "format") {
                expanded += fmt;
            } else if (tag == "samplerate" || tag == "sample_rate" || tag == "sampleRate") {
                expanded += sampleRateStr;
            } else if (tag == "channels" || tag == "channel_count" || tag == "channelCount") {
                expanded += channelsStr;
            } else if (tag == "counter" || tag == "sequence") {
                expanded += counterStr;
            } else {
                throw std::invalid_argument("Unknown filename template placeholder: {" + std::string(tag) + "}");
            }
            i = closePos + 1;
        } else {
            expanded.push_back(filenameTemplate_[i]);
            ++i;
        }
    }

    if (!trackName.empty()) {
        expanded = trackName;
    } else if (expanded.empty()) {
        expanded = sourceStr + "_" + datetimeStr;
    }

    std::filesystem::path filenamePath(expanded);
    if (!trackName.empty()) {
        expanded += "." + fmt;
        filenamePath = std::filesystem::path(expanded);
    } else {
        std::string extension = filenamePath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (extension != ".wav" && extension != ".wave" && extension != ".rf64" &&
            extension != ".flac") {
            expanded += "." + fmt;
            filenamePath = std::filesystem::path(expanded);
        }
    }

    std::filesystem::path outDir =
        baseDirectory_.empty() ? std::filesystem::current_path() : baseDirectory_;
    if (!albumName.empty()) {
        outDir /= albumName;
    }

    std::error_code ec;
    if (metadata.createDirectories) {
        std::filesystem::create_directories(outDir, ec);
        if (ec) {
            throw std::filesystem::filesystem_error("Failed to create output directory", outDir, ec);
        }
    }

    std::filesystem::path fullPath = std::filesystem::absolute(outDir / filenamePath);

    if (std::filesystem::exists(fullPath, ec)) {
        const auto parent = fullPath.parent_path();
        const auto stem = fullPath.stem().string();
        const auto ext = fullPath.extension().string();

        bool collisionResolved = false;
        constexpr int kMaxCollisionAttempts = 9999;
        for (int suffix = 1; suffix <= kMaxCollisionAttempts; ++suffix) {
            std::ostringstream ss;
            ss << stem << "_" << std::setw(3) << std::setfill('0') << suffix << ext;
            const auto candidate = parent / ss.str();
            if (!std::filesystem::exists(candidate, ec)) {
                fullPath = candidate;
                collisionResolved = true;
                break;
            }
        }

        if (!collisionResolved) {
            throw std::runtime_error("Exceeded maximum collision retry attempts for path: " + fullPath.string());
        }
    }

    return fullPath;
}

} // namespace abracapture::recording
