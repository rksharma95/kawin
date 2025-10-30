#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <memory>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace kubearmor::common {

    enum class LogLevel {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERR,
        FATAL
    };

    class Logger {
    public:
        static Logger& GetInstance() {
            static Logger instance;
            return instance;
        }

        void SetLevel(LogLevel level) {
            level_ = level;
        }

        void SetOutputFile(const std::string& path) {
            std::lock_guard<std::mutex> lock(mutex_);
            file_.close();
            file_.open(path, std::ios::app);
        }

        void EnableConsoleOutput(bool enable) {
            console_enabled_ = enable;
        }

        void Log(LogLevel level, const std::string& message,
            const char* file = nullptr, int line = 0) {
            if (level < level_) return;

            std::lock_guard<std::mutex> lock(mutex_);

            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::ostringstream oss;
            oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << ms.count()
                << " [" << LevelToString(level) << "] ";

            if (file && line > 0) {
                oss << file << ":" << line << " - ";
            }

            oss << message << std::endl;

            std::string log_line = oss.str();

            if (console_enabled_) {
                std::cout << log_line;
            }

            if (file_.is_open()) {
                file_ << log_line;
                file_.flush();
            }
        }

    private:
        Logger() : level_(LogLevel::INFO), console_enabled_(true) {}

        const char* LevelToString(LogLevel level) {
            switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO ";
            case LogLevel::WARN: return "WARN ";
            case LogLevel::ERR: return "ERR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKNOWN";
            }
        }

        std::mutex mutex_;
        std::ofstream file_;
        LogLevel level_;
        bool console_enabled_;
    };

#define LOG_TRACE(msg) \
    kubearmor::common::Logger::GetInstance().Log(\
        kubearmor::common::LogLevel::TRACE, msg, __FILE__, __LINE__)

#define LOG_DEBUG(msg) \
    kubearmor::common::Logger::GetInstance().Log(\
        kubearmor::common::LogLevel::DEBUG, msg, __FILE__, __LINE__)

#define LOG_INFO(msg) \
    kubearmor::common::Logger::GetInstance().Log(\
        kubearmor::common::LogLevel::INFO, msg)

#define LOG_WARN(msg) \
    kubearmor::common::Logger::GetInstance().Log(\
        kubearmor::common::LogLevel::WARN, msg, __FILE__, __LINE__)

#define LOG_ERR(msg) \
    kubearmor::common::Logger::GetInstance().Log(\
        kubearmor::common::LogLevel::ERR, msg, __FILE__, __LINE__)

#define LOG_FATAL(msg) \
    kubearmor::common::Logger::GetInstance().Log(\
        kubearmor::common::LogLevel::FATAL, msg, __FILE__, __LINE__)

} // namespace kubearmor::common