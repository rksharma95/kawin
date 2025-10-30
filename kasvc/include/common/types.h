#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <optional>
#include <variant>

namespace kubearmor::common {

    // Action types for rules
    enum class ActionType : uint32_t {
        ALLOW = 0,
        BLOCK = 1,
        AUDIT = 2
    };

    inline const char* ToString(ActionType action) {
        switch (action) {
        case ActionType::ALLOW: return "ALLOW";
        case ActionType::BLOCK: return "BLOCK";
        case ActionType::AUDIT: return "AUDIT";
        default: return "UNKNOWN";
        }
    }

} // namespace kubearmor::common