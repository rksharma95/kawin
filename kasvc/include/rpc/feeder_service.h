#pragma once

#include "kubearmor.grpc.pb.h"
#include "feeder_event_publisher.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace kubearmor::rpc {

    class LogService final : public feeder::LogService::Service {
    public:
        explicit LogService(
            std::shared_ptr<FeederEventPublisher> publisher);

        grpc::Status HealthCheck(
            grpc::ServerContext* context,
            const feeder::NonceMessage* request,
            feeder::ReplyMessage* response) override;

        grpc::Status WatchAlerts(
            grpc::ServerContext* context,
            const feeder::RequestMessage* request,
            grpc::ServerWriter<feeder::Alert>* writer) override;

        grpc::Status WatchLogs(
            grpc::ServerContext* context,
            const feeder::RequestMessage* request,
            grpc::ServerWriter<feeder::Log>* writer) override;

        // WatchMessages not implemented for now
        grpc::Status WatchMessages(
            grpc::ServerContext* context,
            const feeder::RequestMessage* request,
            grpc::ServerWriter<feeder::Message>* writer) override;

    private:
        FeederEventPublisher::StreamFilter ParseFilter(
            const std::string& filter_str);

        std::shared_ptr<FeederEventPublisher> event_publisher_;
    };

} // namespace kubearmor::rpc