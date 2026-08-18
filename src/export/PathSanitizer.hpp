#pragma once

#include <string>
#include <string_view>

namespace abracapture::exporting {

[[nodiscard]] std::string sanitizePathComponent(std::string_view component);

} // namespace abracapture::exporting
