#include "app/monitoring_service.h"
#include "common/logger.h"

namespace kubearmor::app {

    MonitoringService::MonitoringService(
        std::shared_ptr<IEventReceiver> event_receiver,
        std::shared_ptr<IEventPublisher> publisher,
        std::shared_ptr<data::EventProcessor> processor,
        size_t worker_threads_count)
        : event_receiver_(std::move(event_receiver))
        , publisher_(std::move(publisher))
        , processor_(std::move(processor))
        , running_(false)
        , worker_threads_count_(worker_threads_count){
    }

    MonitoringService::~MonitoringService() {
        Stop();
    }

    common::Result<void> MonitoringService::Start() {
        if (running_.load()) {
            return common::Result<void>::Error("Service already running");
        }

        LOG_INFO("Starting monitoring service");

        // Connect to event_receiver
        auto connect_result = event_receiver_->Connect();
        if (!connect_result) {
            LOG_ERR("Failed to connect to event_receiver: " + connect_result.ErrorMessage());
            return connect_result;
        }

        running_ = true;
        start_time_ = std::chrono::steady_clock::now();

        // Start worker threads
        for (size_t i = 0; i < worker_threads_count_; ++i) {
            worker_threads_.emplace_back([this] { EventLoopThread(); });
        }

        LOG_INFO("Monitoring service started with " +
            std::to_string(worker_threads_count_) + " worker threads");

        return common::Result<void>::Success();
    }

    common::Result<void> MonitoringService::Stop() {
        if (!running_.load()) {
            return common::Result<void>::Success();
        }

        LOG_INFO("Stopping monitoring service");

        running_ = false;

        // Wait for threads to finish
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads_.clear();

        // Disconnect
        event_receiver_->Disconnect();

        LOG_INFO("Monitoring service stopped");

        return common::Result<void>::Success();
    }

    void MonitoringService::EventLoopThread() {
        LOG_DEBUG("Event loop thread started");

        while (running_.load()) {
            LOG_DEBUG("EventLoopThread()");
            auto event_opt = event_receiver_->ReceiveEvent(std::chrono::milliseconds(100));

            if (!event_opt) {
                LOG_DEBUG("EventLoopThread() !event_opt");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            events_received_++;

            try {
                ProcessEvent(*event_opt);
            }
            catch (const std::exception& e) {
                LOG_ERR(std::string("Error processing event: ") + e.what());
                processing_errors_++;
            }
        }

        LOG_DEBUG("Event loop thread stopped");
    }

    void MonitoringService::ProcessEvent(const data::Event& event) {
        // Enrich event with additional information
        auto enriched = processor_->Enrich(event);

        // Publish to subscribers
        publisher_->Publish(enriched);

        events_processed_++;
        events_published_++;
    }

    MonitoringService::MonitoringStatus MonitoringService::GetStatus() const {
        return MonitoringStatus{
            event_receiver_->IsConnected(),
            running_.load(),
            publisher_->GetSubscriberCount(),
            events_processed_.load(),
            start_time_
        };
    }

    MonitoringService::Statistics MonitoringService::GetStatistics() const {
        return Statistics{
            events_received_.load(),
            events_processed_.load(),
            events_published_.load(),
            processing_errors_.load(),
            start_time_
        };
    }

    void MonitoringService::ResetStatistics() {
        events_received_ = 0;
        events_processed_ = 0;
        events_published_ = 0;
        processing_errors_ = 0;
        start_time_ = std::chrono::steady_clock::now();
    }

} // namespace kubearmor::app