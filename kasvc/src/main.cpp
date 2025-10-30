#include "common/logger.h"
#include "common/constants.h"
#include "data/event_processor.h"
#include "app/monitoring_service.h"
#include "comm/iocp_filter_port_communicator.h"
#include "comm/json_config_store.h"
#include "rpc/feeder_event_publisher.h"
#include "rpc/feeder_service.h"
#include <grpcpp/grpcpp.h>
#include <csignal>
#include <memory>
#include <iostream>

namespace {
    std::unique_ptr<grpc::Server> g_server;
    std::shared_ptr<kubearmor::app::MonitoringService> g_monitoring_service;

    void SignalHandler(int signal) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;

        if (g_monitoring_service) {
            g_monitoring_service->Stop();
        }

        if (g_server) {
            g_server->Shutdown();
        }
    }

    kubearmor::common::LogLevel ParseLogLevel(const std::string& level) {
        if (level == "TRACE") return kubearmor::common::LogLevel::TRACE;
        if (level == "DEBUG") return kubearmor::common::LogLevel::DEBUG;
        if (level == "INFO") return kubearmor::common::LogLevel::INFO;
        if (level == "WARN") return kubearmor::common::LogLevel::WARN;
        if (level == "ERR") return kubearmor::common::LogLevel::ERR;
        if (level == "FATAL") return kubearmor::common::LogLevel::FATAL;
        return kubearmor::common::LogLevel::INFO;
    }
}

int main(int argc, char** argv) {
    using namespace kubearmor;

    // Setup signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // Basic console logging initially
    common::Logger::GetInstance().SetLevel(common::LogLevel::INFO);
    common::Logger::GetInstance().EnableConsoleOutput(true);

    LOG_INFO("========================================");
    LOG_INFO("KubeArmor User Service Starting");
    LOG_INFO("========================================");

    try {
        // Determine config file path
        std::string config_file = "config.json";
        if (argc > 1) {
            config_file = argv[1];
            LOG_INFO("Using config file: " + config_file);
        }

        // Load configuration
        auto config_store = std::make_shared<comm::JsonConfigStore>(config_file);
        auto config_result = config_store->Load();

        if (!config_result) {
            LOG_FATAL("Failed to load configuration: " + config_result.ErrorMessage());
            return 1;
        }

        auto config = config_result.Value();

        LOG_INFO("Configuration loaded successfully");
        LOG_INFO("  Service: " + config.service_name);
        LOG_INFO("  gRPC: " + config.grpc_address + ":" +
            std::to_string(config.grpc_port));
        LOG_INFO("  Worker threads: " + std::to_string(config.worker_threads));

        // Update logger
        common::Logger::GetInstance().SetLevel(ParseLogLevel(config.log_level));
        common::Logger::GetInstance().SetOutputFile(config.log_file);
        LOG_INFO("Logging: " + config.log_file + " [" + config.log_level + "]");

        // Watch config
        config_store->Watch([](const app::Configuration& new_config) {
            LOG_INFO("Configuration changed");
            });

        // Create data services
        auto event_processor = std::make_shared<data::EventProcessor>();

        // Configure IOCP
        comm::IOCPFilterPortCommunicator::IOCPConfig iocp_config;
        iocp_config.port_name = std::wstring(config.filter_port_name.begin(), config.filter_port_name.end());
        iocp_config.worker_thread_count = config.worker_threads;
        iocp_config.concurrent_operations = 2* config.worker_threads;
        iocp_config.buffer_size = 4096;
        iocp_config.buffer_pool_size = 4 * config.worker_threads;

        LOG_INFO("IOCP Configuration:");
        LOG_INFO("  Worker threads: " + std::to_string(iocp_config.worker_thread_count));
        LOG_INFO("  Concurrent ops: " + std::to_string(iocp_config.concurrent_operations));
        LOG_INFO("  Buffer pool: " + std::to_string(iocp_config.buffer_pool_size));

        // Create comm components
        auto event_receiver =
            std::make_shared<comm::IOCPFilterPortCommunicator>(iocp_config);

        auto feeder_publisher = std::make_shared<kubearmor::rpc::FeederEventPublisher>(
            config.cluster_name,
            config.host_name);

        // Create monitoring service
        auto monitoring_service = std::make_shared<app::MonitoringService>(
            event_receiver,
            feeder_publisher,
            event_processor,
            config.service_worker_threads);

        g_monitoring_service = monitoring_service;

        auto monitoring_result = g_monitoring_service->Start();

        if (!monitoring_result) {
            LOG_FATAL("Failed to start monitoring service: " + monitoring_result.ErrorMessage());
            return 1;
        }
        LOG_INFO("returned from monitoring service startup");

        // Create gRPC service
        auto grpc_service = std::make_unique<kubearmor::rpc::LogService>(
            feeder_publisher);

        // Build gRPC server
        std::string server_address = config.grpc_address + ":" +
            std::to_string(config.grpc_port);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(grpc_service.get());

        LOG_INFO("build and start grpc server");
        g_server = builder.BuildAndStart();

        if (!g_server) {
            LOG_FATAL("Failed to start gRPC server");
            return 1;
        }

        LOG_INFO("========================================");
        LOG_INFO("Service Startup Complete");
        LOG_INFO("========================================");
        LOG_INFO("gRPC: " + server_address);
        LOG_INFO("Config: " + config_file);
        LOG_INFO("Log: " + config.log_file);
        LOG_INFO("Supported Events: File, Process, Network");
        LOG_INFO("Press Ctrl+C to stop");
        LOG_INFO("========================================");

        // Performance monitoring thread
        std::thread perf_thread([&event_receiver, &monitoring_service, &feeder_publisher]() {
            while (true) {
                LOG_DEBUG("perf_thread");
                std::this_thread::sleep_for(std::chrono::seconds(60));

                try {
                    auto iocp_metrics = event_receiver->GetPerformanceMetrics();
                    auto mon_stats = monitoring_service->GetStatistics();
                    auto pub_stats = feeder_publisher->GetStatistics();

                    LOG_INFO("=== Performance Report ===");
                    LOG_INFO("  Messages recv: " +
                        std::to_string(iocp_metrics.total_messages_received));
                    LOG_INFO("  Messages/sec: " +
                        std::to_string(iocp_metrics.messages_per_second));
                    LOG_INFO("  Avg latency: " +
                        std::to_string(iocp_metrics.average_latency_us) + " ?s");
                    LOG_INFO("  Buffers: " +
                        std::to_string(iocp_metrics.buffers_in_use) + "/" +
                        std::to_string(iocp_metrics.buffers_in_use +
                            iocp_metrics.buffers_available));
                    LOG_INFO("  Events processed: " +
                        std::to_string(mon_stats.events_processed));
                    LOG_INFO("  Events published: " +
                        std::to_string(pub_stats.events_published));
                    LOG_INFO("  Active streams: " +
                        std::to_string(pub_stats.active_subscribers));
                    LOG_INFO("  Processing errors: " +
                        std::to_string(mon_stats.processing_errors));
                }
                catch (const std::exception& e) {
                    LOG_ERR("Perf monitoring error: " + std::string(e.what()));
                }
            }
            });
        perf_thread.detach();

        // Wait for shutdown
        g_server->Wait();

        g_monitoring_service->Stop();

        // Cleanup
        config_store->StopWatching();

    }
    catch (const std::exception& e) {
        LOG_FATAL("Fatal error: " + std::string(e.what()));
        return 1;
    }

    LOG_INFO("========================================");
    LOG_INFO("KubeArmor User Service Stopped");
    LOG_INFO("========================================");

    return 0;
}