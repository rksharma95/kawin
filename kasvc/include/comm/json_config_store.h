#pragma once

#include "app/interfaces/i_configuration_store.h"
#include "common/logger.h"
#include <filesystem>
#include <mutex>

using namespace kubearmor::app;

namespace kubearmor::comm {

    class JsonConfigStore : public app::IConfigurationStore {
    public:
        explicit JsonConfigStore(const std::filesystem::path& config_path);
        ~JsonConfigStore() override = default;

        // IConfigurationStore implementation
        common::Result<app::Configuration> Load() override;
        common::Result<void> Save(const app::Configuration& config) override;

        void Watch(ConfigChangeCallback callback) override;
        void StopWatching() override;

    private:
        std::filesystem::path config_path_;
        ConfigChangeCallback callback_;

        std::thread watch_thread_;
        std::atomic<bool> watching_;
        std::filesystem::file_time_type last_write_time_;

        mutable std::mutex mutex_;

        void WatchThreadFunc();

        // JSON parsing helpers
        common::Result<app::Configuration> ParseJson(const std::string& json);
        std::string ToJson(const app::Configuration& config);

        common::LogLevel ParseLogLevel(const std::string& level);
    };

} // namespace kubearmor::comm