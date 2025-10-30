#include "comm/json_config_store.h"
#include "common/logger.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kubearmor::comm {

    JsonConfigStore::JsonConfigStore(const std::filesystem::path& config_path)
        : config_path_(config_path)
        , watching_(false) {
    }

    common::Result<app::Configuration> JsonConfigStore::Load() {
        std::lock_guard<std::mutex> lock(mutex_);

        LOG_INFO("Loading configuration from: " + config_path_.string());

        if (!std::filesystem::exists(config_path_)) {
            return common::Result<app::Configuration>::Error(
                "Configuration file not found: " + config_path_.string());
            // TODO: if configuration file not found, don't fail and retrun
            // instead use default configurations
        }

        try {
            std::ifstream file(config_path_);
            if (!file.is_open()) {
                return common::Result<app::Configuration>::Error(
                    "Failed to open configuration file");
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string json_str = buffer.str();

            auto result = ParseJson(json_str);

            if (result) {
                last_write_time_ = std::filesystem::last_write_time(config_path_);
                LOG_INFO("Configuration loaded successfully");
            }

            return result;

        }
        catch (const std::exception& e) {
            return common::Result<app::Configuration>::Error(
                "Failed to load configuration: " + std::string(e.what()));
        }
    }

    common::Result<void> JsonConfigStore::Save(
        const app::Configuration& config) {

        std::lock_guard<std::mutex> lock(mutex_);

        LOG_INFO("Saving configuration to: " + config_path_.string());

        try {
            std::string json_str = ToJson(config);

            // Write to temporary file first
            std::filesystem::path temp_path = config_path_;
            temp_path += ".tmp";

            std::ofstream file(temp_path);
            if (!file.is_open()) {
                return common::Result<void>::Error(
                    "Failed to open file for writing");
            }

            file << json_str;
            file.close();

            std::filesystem::rename(temp_path, config_path_);

            last_write_time_ = std::filesystem::last_write_time(config_path_);

            LOG_INFO("Configuration saved successfully");

            return common::Result<void>::Success();

        }
        catch (const std::exception& e) {
            return common::Result<void>::Error(
                "Failed to save configuration: " + std::string(e.what()));
        }
    }

    void JsonConfigStore::Watch(ConfigChangeCallback callback) {
        callback_ = std::move(callback);

        if (!watching_.load()) {
            watching_ = true;
            watch_thread_ = std::thread([this] { WatchThreadFunc(); });

            LOG_INFO("Started watching configuration file for changes");
        }
    }

    void JsonConfigStore::StopWatching() {
        if (watching_.load()) {
            watching_ = false;

            if (watch_thread_.joinable()) {
                watch_thread_.join();
            }

            LOG_INFO("Stopped watching configuration file");
        }
    }

    void JsonConfigStore::WatchThreadFunc() {
        while (watching_.load()) {
            LOG_DEBUG("WatchThreadFunc()");
            std::this_thread::sleep_for(std::chrono::seconds(5));

            try {
                if (!std::filesystem::exists(config_path_)) {
                    continue;
                }

                auto current_write_time = std::filesystem::last_write_time(config_path_);

                if (current_write_time != last_write_time_) {
                    LOG_INFO("Configuration file changed, reloading");

                    auto config = Load();
                    if (config && callback_) {
                        callback_(config.Value());
                    }
                }

            }
            catch (const std::exception& e) {
                LOG_ERR("Error watching config file: " + std::string(e.what()));
            }
        }
    }

    common::Result<app::Configuration> JsonConfigStore::ParseJson(
        const std::string& json_str) {

        try {
            json j = json::parse(json_str);

            app::Configuration config;
            
            // Cluster Name
            config.cluster_name = j.value("cluster_name","default");

            // Host Name
            config.host_name = j.value("host_name", "winows_host");

            // Service settings
            if (j.contains("service")) {
                config.service_name = j["service"].value("name", "KubeArmorUserService");
                // Worker threads
                if (j["service"].contains("worker_threads")) {
                    std::string threads = j["service"]["worker_threads"];
                    if (threads == "auto") {
                        config.service_worker_threads = std::thread::hardware_concurrency();
                    }
                    else {
                        config.service_worker_threads = std::stoul(threads);
                    }
                }
                else {
                    config.service_worker_threads = std::thread::hardware_concurrency();
                }
            }

            // Driver settings
            if (j.contains("driver")) {
                auto& driver = j["driver"];

                std::string port_name = driver.value("filter_port_name", "\\\\ScannerPort");
                config.filter_port_name = port_name;

                std::string device_path = driver.value("device_path", "\\\\??\\\\Karmor");
                config.device_path = device_path;

                // Worker threads
                if (j["driver"].contains("worker_threads")) {
                    std::string threads = j["driver"]["worker_threads"];
                    if (threads == "auto") {
                        config.worker_threads = std::thread::hardware_concurrency();
                    }
                    else {
                        config.worker_threads = std::stoul(threads);
                    }
                }
                else {
                    config.worker_threads = std::thread::hardware_concurrency();
                }
            }

            // gRPC settings
            if (j.contains("grpc")) {
                auto& grpc = j["grpc"];
                config.grpc_address = grpc.value("address", "0.0.0.0");
                config.grpc_port = grpc.value("port", static_cast<uint16_t>(32767));
            }

            // Event queue settings
            if (j.contains("event_streaming")) {
                auto& streaming = j["event_streaming"];
                config.event_queue_size = streaming.value("max_queue_size", 10000);
            }

            // Logging
            if (j.contains("logging")) {
                auto& logging = j["logging"];
                config.log_file = logging.value("file", "kubearmor_service.log");
                config.log_level = logging.value("level", "INFO");
            }

            return common::Result<app::Configuration>::Success(config);

        }
        catch (const json::exception& e) {
            return common::Result<app::Configuration>::Error(
                "JSON parse error: " + std::string(e.what()));
        }
    }

    std::string JsonConfigStore::ToJson(const app::Configuration& config) {
        json j;

        // Cluster Name
        j["cluster_name"] = config.cluster_name;

        // Host Name
        j["host_name"] = config.host_name;
        
        // Service
        j["service"]["name"] = config.service_name;

        // Driver
        std::string port_name(config.filter_port_name.begin(),
            config.filter_port_name.end());
        std::string device_path(config.device_path.begin(),
            config.device_path.end());

        j["driver"]["filter_port_name"] = port_name;
        j["driver"]["device_path"] = device_path;
        j["driver"]["worker_threads"] = config.worker_threads;

        // gRPC
        j["grpc"]["address"] = config.grpc_address;
        j["grpc"]["port"] = config.grpc_port;

        // Event streaming
        j["event_streaming"]["max_queue_size"] = config.event_queue_size;

        // Logging
        j["logging"]["file"] = config.log_file;
        j["logging"]["level"] = config.log_level;

        return j.dump(2);
    }

} // namespace kubearmor::comm