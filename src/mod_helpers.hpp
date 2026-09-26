#pragma once

#include <stdexcept>
#include <string_view>

#include "mods/api.h"

#include "fmt/format.h"

namespace slugcat::mod_helpers {

constexpr std::string_view ResultMessage(ModResult const result) {
    using namespace std::string_view_literals;

    switch (result) {
    case MOD_OK:
        return "OK"sv;
    case MOD_ERROR:
        return "ERROR"sv;
    case MOD_UNAVAILABLE:
        return "UNAVAILABLE"sv;
    case MOD_UNSUPPORTED:
        return "UNSUPPORTED"sv;
    case MOD_CONFLICT:
        return "CONFLICT"sv;
    case MOD_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT"sv;
    default:
        return "???"sv;
    }
}

inline void CheckResult(ModResult result, std::string_view operation) {
    if (result != MOD_OK) {
        throw std::runtime_error(fmt::format("{} failed: {}", operation, ResultMessage(result)));
    }
}

} // namespace slugcat::mod_helpers
