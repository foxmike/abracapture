#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <spa/utils/dict.h>

struct PropertyNormalizer final {
    PropertyNormalizer() = delete;

    static std::string extractDisplayName(const spa_dict* props);
    static std::string extractAppName(const spa_dict* props);
    static std::string extractMediaClass(const spa_dict* props);
    static std::string extractNodeName(const spa_dict* props);
    static std::optional<std::uint32_t> extractDeviceId(const spa_dict* props);
    static std::optional<std::uint32_t> extractUInt32(const spa_dict* props,
                                                      const char* key);
    static std::string extractIconName(const spa_dict* props);
    static std::string sanitizeUtf8(std::string_view raw);
};
