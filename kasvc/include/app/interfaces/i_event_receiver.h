#pragma once

#include "common/result.h"
#include "data/event_types.h"
#include <chrono>
#include <optional>

namespace kubearmor::app {

    class IEventReceiver {
    public:
        virtual ~IEventReceiver() = default;

        // Connection management
        virtual common::Result<void> Connect() = 0;
        virtual void Disconnect() = 0;
        virtual bool IsConnected() const = 0;

        virtual std::optional<data::Event> ReceiveEvent(
            std::chrono::milliseconds timeout) = 0;

        struct PerformanceMetrics {
            uint64_t total_messages_received;
            uint64_t messages_per_second;
            uint64_t average_latency_us;
            uint64_t buffers_in_use;
            uint64_t buffers_available;
            uint64_t dropped_messages;
        };

        virtual PerformanceMetrics GetPerformanceMetrics() const = 0;
    };

} // namespace kubearmor::app