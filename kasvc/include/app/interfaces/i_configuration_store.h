#pragma once

#include "common/result.h"
#include <string>
#include <vector>
#include <functional>

namespace kubearmor::app {

    struct Configuration {
        std::string cluster_name;
        std::string host_name;
        std::string service_name;
        std::string filter_port_name;
        std::string device_path;
        std::string grpc_address;
        uint16_t grpc_port;

        size_t event_queue_size;
        size_t worker_threads;
        size_t service_worker_threads;
        std::string log_file;
        std::string log_level;
    };

    class IConfigurationStore {
    public:
        virtual ~IConfigurationStore() = default;

        // Load/Save configuration
        virtual common::Result<Configuration> Load() = 0;
        virtual common::Result<void> Save(const Configuration& config) = 0;

        // Watch for changes
        using ConfigChangeCallback = std::function<void(const Configuration&)>;
        virtual void Watch(ConfigChangeCallback callback) = 0;
        virtual void StopWatching() = 0;
    };

} // namespace kubearmor::app