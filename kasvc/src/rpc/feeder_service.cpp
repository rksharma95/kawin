#include "rpc/feeder_service.h"
#include "common/logger.h"

namespace kubearmor::rpc {

    LogService::LogService(
        std::shared_ptr<FeederEventPublisher> publisher)
        : event_publisher_(std::move(publisher)) {
    }

    grpc::Status LogService::HealthCheck(
        grpc::ServerContext* context,
        const feeder::NonceMessage* request,
        feeder::ReplyMessage* response) {

        response->set_retval(request->nonce());
        return grpc::Status::OK;
    }

    grpc::Status LogService::WatchAlerts(
        grpc::ServerContext* context,
        const feeder::RequestMessage* request,
        grpc::ServerWriter<feeder::Alert>* writer) {

        LOG_INFO("gRPC: WatchAlerts started");

        // Parse filter
        auto filter = ParseFilter(request->filter());

        // Subscribe to alert stream
        auto subscription_id = event_publisher_->SubscribeAlerts(writer, filter);

        // Keep stream alive until client disconnects
        while (!context->IsCancelled()) {
            LOG_DEBUG("WatchAlerts");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Cleanup
        event_publisher_->UnsubscribeAlerts(subscription_id);

        LOG_INFO("gRPC: WatchAlerts ended");

        return grpc::Status::OK;
    }

    grpc::Status LogService::WatchLogs(
        grpc::ServerContext* context,
        const feeder::RequestMessage* request,
        grpc::ServerWriter<feeder::Log>* writer) {

        LOG_INFO("gRPC: WatchLogs started");

        // Parse filter
        auto filter = ParseFilter(request->filter());

        // Subscribe to log stream
        auto subscription_id = event_publisher_->SubscribeLogs(writer, filter);

        // Keep stream alive until client disconnects
        while (!context->IsCancelled()) {
            LOG_DEBUG("WatchLogs");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Cleanup
        event_publisher_->UnsubscribeLogs(subscription_id);

        LOG_INFO("gRPC: WatchLogs ended");

        return grpc::Status::OK;
    }

    grpc::Status LogService::WatchMessages(
        grpc::ServerContext* context,
        const feeder::RequestMessage* request,
        grpc::ServerWriter<feeder::Message>* writer) {

        // Not implemented
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "WatchMessages not implemented");
    }

    FeederEventPublisher::StreamFilter
        LogService::ParseFilter(const std::string& filter_str) {

        FeederEventPublisher::StreamFilter filter;

        return filter;
    }

} // namespace kubearmor::rpc