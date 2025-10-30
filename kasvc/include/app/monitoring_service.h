#pragma once

#include "app/interfaces/i_event_publisher.h"
#include "app/interfaces/i_event_receiver.h"
#include "data/event_processor.h"
#include "common/result.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

namespace kubearmor::app {

    class MonitoringService {
    public:
        struct MonitoringStatus {
            bool connected;
            bool running;
            size_t subscriber_count;
            uint64_t events_processed;
            std::chrono::steady_clock::time_point start_time;
        };

        MonitoringService(
            std::shared_ptr<IEventReceiver> event_receiver,
            std::shared_ptr<IEventPublisher> publisher,
            std::shared_ptr<data::EventProcessor> processor,
            size_t worker_threads_count);

        ~MonitoringService();

        common::Result<void> Start();
        common::Result<void> Stop();
        bool IsRunning() const { return running_.load(); }

        MonitoringStatus GetStatus() const;

        struct Statistics {
            uint64_t events_received;
            uint64_t events_processed;
            uint64_t events_published;
            uint64_t processing_errors;
            std::chrono::steady_clock::time_point start_time;
        };

        Statistics GetStatistics() const;
        void ResetStatistics();

    private:
        void EventLoopThread();
        void ProcessEvent(const data::Event& event);

        std::shared_ptr<IEventReceiver> event_receiver_;
        std::shared_ptr<IEventPublisher> publisher_;
        std::shared_ptr<data::EventProcessor> processor_;
        size_t worker_threads_count_;

        std::atomic<bool> running_;
        std::vector<std::thread> worker_threads_;

        std::atomic<uint64_t> events_received_{ 0 };
        std::atomic<uint64_t> events_processed_{ 0 };
        std::atomic<uint64_t> events_published_{ 0 };
        std::atomic<uint64_t> processing_errors_{ 0 };
        std::chrono::steady_clock::time_point start_time_;
    };

} // namespace kubearmor::app