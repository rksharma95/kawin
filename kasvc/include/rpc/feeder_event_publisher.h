#pragma once

#include "app/interfaces/i_event_publisher.h"
#include "data/event_types.h"
#include "kubearmor.grpc.pb.h"  // From submodule
#include <grpcpp/grpcpp.h>
#include <map>
#include <shared_mutex>
#include <atomic>
#include <set>

namespace kubearmor::rpc {

    class FeederEventPublisher : public app::IEventPublisher {
    public:
        struct StreamFilter {
            std::set<std::string> types;        // "alert", "log"
            std::set<std::string> namespaces;
            std::set<uint32_t> process_ids;
            bool blocked_only;

            StreamFilter() : blocked_only(false) {}
        };

        FeederEventPublisher(const std::string& cluster_name,
            const std::string& host_name);
        ~FeederEventPublisher() override = default;

        void Publish(const data::Event& event) override;
        void PublishBatch(const std::vector<data::Event>& events) override;
        size_t GetSubscriberCount() const override;
        PublisherStatistics GetStatistics() const override;

        using AlertStreamId = uint64_t;
        AlertStreamId SubscribeAlerts(
            grpc::ServerWriter<feeder::Alert>* writer,
            const StreamFilter& filter);
        void UnsubscribeAlerts(AlertStreamId id);

        using LogStreamId = uint64_t;
        LogStreamId SubscribeLogs(
            grpc::ServerWriter<feeder::Log>* writer,
            const StreamFilter& filter);
        void UnsubscribeLogs(LogStreamId id);

    private:
        struct AlertSubscriber {
            grpc::ServerWriter<feeder::Alert>* writer;
            StreamFilter filter;
            std::atomic<bool> active;
            std::chrono::steady_clock::time_point last_activity;
            std::mutex write_mutex;
        };

        struct LogSubscriber {
            grpc::ServerWriter<feeder::Log>* writer;
            StreamFilter filter;
            std::atomic<bool> active;
            std::chrono::steady_clock::time_point last_activity;
            std::mutex write_mutex;
        };

        feeder::Alert ConvertToAlert(const data::Event& event);
        feeder::Log ConvertToLog(const data::Event& event);

        bool ShouldSendToSubscriber(const data::Event& event,
            const StreamFilter& filter);

        int64_t ToUnixTimestamp(std::chrono::system_clock::time_point tp);
        std::string ToFormattedTime(std::chrono::system_clock::time_point tp);

        std::string cluster_name_;
        std::string host_name_;

        mutable std::shared_mutex alert_subscribers_mutex_;
        std::map<AlertStreamId, std::unique_ptr<AlertSubscriber>> alert_subscribers_;
        std::atomic<AlertStreamId> next_alert_id_{ 1 };

        mutable std::shared_mutex log_subscribers_mutex_;
        std::map<LogStreamId, std::unique_ptr<LogSubscriber>> log_subscribers_;
        std::atomic<LogStreamId> next_log_id_{ 1 };

        std::atomic<uint64_t> alerts_published_{ 0 };
        std::atomic<uint64_t> logs_published_{ 0 };
        std::atomic<uint64_t> events_dropped_{ 0 };
    };

} // namespace kubearmor::rpc