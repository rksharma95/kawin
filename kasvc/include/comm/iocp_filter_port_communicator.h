#pragma once

#include "app/interfaces/i_event_receiver.h"  // Changed!
#include "comm/kernel_message.h"
#include "common/thread_safe_queue.h"
#include <Windows.h>
#include <fltUser.h>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace kubearmor::comm {

    class IOCPFilterPortCommunicator : public app::IEventReceiver {
    public:
        struct IOCPConfig {
            std::wstring port_name;
            size_t worker_thread_count;
            size_t concurrent_operations;
            size_t buffer_size;
            size_t buffer_pool_size;
        };

        explicit IOCPFilterPortCommunicator(const IOCPConfig& config);
        ~IOCPFilterPortCommunicator() override;

        // IEventReceiver implementation
        common::Result<void> Connect() override;
        void Disconnect() override;
        bool IsConnected() const override;

        std::optional<data::Event> ReceiveEvent(
            std::chrono::milliseconds timeout) override;

        PerformanceMetrics GetPerformanceMetrics() const override;

    private:
        // IOCP context structure
        struct IOContext {
            OVERLAPPED overlapped;
            uint8_t* message_buffer;
            size_t buffer_size;
            IOCPFilterPortCommunicator* communicator;
            bool in_use;
            std::chrono::steady_clock::time_point submit_time;
        };

        // Buffer management
        IOContext* AllocateContext(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
        void FreeContext(IOContext* context);
        bool SubmitReceive(IOContext* context);

        // IOCP worker threads
        void IOCPWorkerThread();

        // Event processing
        void ProcessCompletedIO(IOContext* context, DWORD bytes_transferred);

        // Reply sending
        bool SendReply(const FILTER_MESSAGE_HEADER* msg_header, HRESULT status);

        IOCPConfig config_;
        HANDLE filter_port_;
        HANDLE iocp_handle_;

        std::atomic<bool> running_;
        std::vector<std::thread> worker_threads_;

        // Buffer pool
        std::vector<std::unique_ptr<IOContext>> context_pool_;
        std::mutex context_pool_mutex_;
        std::condition_variable context_available_;

        // Event queue for dispatch
        common::ThreadSafeQueue<data::Event> event_queue_;

        // Performance
        std::atomic<uint64_t> total_messages_{ 0 };
        std::atomic<uint64_t> total_latency_us_{ 0 };
        std::atomic<uint64_t> dropped_messages_{ 0 };
        std::chrono::steady_clock::time_point last_stats_time_;
        uint64_t last_message_count_{ 0 };
    };

} // namespace kubearmor::comm