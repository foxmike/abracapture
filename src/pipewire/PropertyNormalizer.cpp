#include "PropertyNormalizer.hpp"

#include <charconv>

#include <pipewire/keys.h>

namespace {

const char* lookup(const spa_dict* props, const char* key)
{
    return props == nullptr ? nullptr : spa_dict_lookup(props, key);
}

std::string sanitizedProperty(const spa_dict* props, const char* key)
{
    const char* value = lookup(props, key);
    return value == nullptr ? std::string{} : PropertyNormalizer::sanitizeUtf8(value);
}

void appendReplacement(std::string& output)
{
    output.append("\xEF\xBF\xBD");
}

bool isContinuation(unsigned char byte)
{
    return (byte & 0xC0u) == 0x80u;
}

} // namespace

std::string PropertyNormalizer::extractDisplayName(const spa_dict* props)
{
    constexpr const char* fallback = "Unknown";
    if (props == nullptr)
        return fallback;

    constexpr const char* keys[] = {
        PW_KEY_NODE_DESCRIPTION,
        PW_KEY_NODE_NICK,
        PW_KEY_NODE_NAME,
        PW_KEY_APP_NAME,
    };
    for (const char* key : keys) {
        if (const char* value = lookup(props, key); value != nullptr && *value != '\0')
            return sanitizeUtf8(value);
    }
    return fallback;
}

std::string PropertyNormalizer::extractAppName(const spa_dict* props)
{
    return sanitizedProperty(props, PW_KEY_APP_NAME);
}

std::string PropertyNormalizer::extractMediaClass(const spa_dict* props)
{
    return sanitizedProperty(props, PW_KEY_MEDIA_CLASS);
}

std::optional<std::uint32_t> PropertyNormalizer::extractDeviceId(const spa_dict* props)
{
    return extractUInt32(props, PW_KEY_DEVICE_ID);
}

std::optional<std::uint32_t> PropertyNormalizer::extractUInt32(const spa_dict* props,
                                                               const char* key)
{
    const char* value = lookup(props, key);
    if (value == nullptr || *value == '\0')
        return std::nullopt;

    std::uint32_t deviceId = 0;
    const char* end = value;
    while (*end != '\0')
        ++end;
    const auto result = std::from_chars(value, end, deviceId, 10);
    if (result.ec != std::errc{} || result.ptr != end)
        return std::nullopt;
    return deviceId;
}

std::string PropertyNormalizer::extractIconName(const spa_dict* props)
{
    return sanitizedProperty(props, PW_KEY_APP_ICON_NAME);
}

std::string PropertyNormalizer::sanitizeUtf8(std::string_view raw)
{
    std::string output;
    output.reserve(raw.size());

    for (std::size_t i = 0; i < raw.size();) {
        const auto first = static_cast<unsigned char>(raw[i]);
        std::uint32_t codePoint = 0;
        std::size_t length = 0;

        if (first <= 0x7Fu) {
            codePoint = first;
            length = 1;
        } else if (first >= 0xC2u && first <= 0xDFu) {
            codePoint = first & 0x1Fu;
            length = 2;
        } else if (first >= 0xE0u && first <= 0xEFu) {
            codePoint = first & 0x0Fu;
            length = 3;
        } else if (first >= 0xF0u && first <= 0xF4u) {
            codePoint = first & 0x07u;
            length = 4;
        } else {
            appendReplacement(output);
            ++i;
            continue;
        }

        if (i + length > raw.size()) {
            appendReplacement(output);
            ++i;
            continue;
        }
        bool valid = true;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto byte = static_cast<unsigned char>(raw[i + offset]);
            if (!isContinuation(byte)) {
                valid = false;
                break;
            }
            codePoint = (codePoint << 6u) | (byte & 0x3Fu);
        }
        if (!valid || codePoint > 0x10FFFFu ||
            (codePoint >= 0xD800u && codePoint <= 0xDFFFu) ||
            (length == 3 && codePoint < 0x800u) ||
            (length == 4 && codePoint < 0x10000u)) {
            appendReplacement(output);
            ++i;
            continue;
        }

        if (!((codePoint <= 0x1Fu) ||
              (codePoint >= 0x7Fu && codePoint <= 0x9Fu)))
            output.append(raw, i, length);
        i += length;
    }
    return output;
}
