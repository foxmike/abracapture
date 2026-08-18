#include "PathSanitizer.hpp"

#include <cctype>

namespace abracapture::exporting {

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

} // namespace

std::string sanitizePathComponent(const std::string_view component) {
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

} // namespace abracapture::exporting
