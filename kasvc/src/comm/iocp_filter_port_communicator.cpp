#include "comm/iocp_filter_port_communicator.h"
#include "comm/message_parser.h"
#include <algorithm>
#include "common/logger.h"
#include "common/constants.h"

namespace kubearmor::comm {

    IOCPFilterPortCommunicator::IOCPFilterPortCommunicator(const IOCPConfig& config)
        : config_(config)
        , filter_port_(INVALID_HANDLE_VALUE)
        , iocp_handle_(nullptr)
        , running_(false)
        , event_queue_(constants::MAX_EVENT_QUEUE_SIZE)
        , last_stats_time_(std::chrono::steady_clock::now()) {
    }

    IOCPFilterPortCommunicator::~IOCPFilterPortCommunicator() {
        Disconnect();
    }

    common::Result<void> IOCPFilterPortCommunicator::Connect() {
        if (IsConnected()) {
            return common::Result<void>::Success();
        }

        LOG_INFO("Connecting to filter port with IOCP: " +
            std::string(config_.port_name.begin(), config_.port_name.end()));

        // Connect to filter port
        HRESULT hr = FilterConnectCommunicationPort(
            config_.port_name.c_str(),
            0,
            nullptr,
            0,
            nullptr,
            &filter_port_
        );

        if (FAILED(hr)) {
            LOG_ERR("FilterConnectCommunicationPort failed: 0x" +
                std::to_string(hr));
            return common::Result<void>::Error(
                "Failed to connect to filter port: 0x" + std::to_string(hr));
        }

        // Create IOCP
        iocp_handle_ = CreateIoCompletionPort(
            filter_port_,
            nullptr,
            0,
            static_cast<DWORD>(config_.worker_thread_count)
        );

        if (!iocp_handle_) {
            DWORD error = GetLastError();
            CloseHandle(filter_port_);
            filter_port_ = INVALID_HANDLE_VALUE;

            LOG_ERR("CreateIoCompletionPort failed: " + std::to_string(error));
            return common::Result<void>::Error(
                "Failed to create IOCP: " + std::to_string(error));
        }

        // Allocate buffer pool
        LOG_INFO("Allocating " + std::to_string(config_.buffer_pool_size) +
            " buffers");

        for (size_t i = 0; i < config_.buffer_pool_size; ++i) {
            auto context = std::make_unique<IOContext>();
            context->message_buffer = static_cast<uint8_t*>(
                _aligned_malloc(config_.buffer_size, MEMORY_ALLOCATION_ALIGNMENT));

            if (!context->message_buffer) {
                LOG_ERR("Failed to allocate message buffer");
                Disconnect();
                return common::Result<void>::Error("Memory allocation failed");
            }

            context->buffer_size = config_.buffer_size;
            context->communicator = this;
            context->in_use = false;

            context_pool_.push_back(std::move(context));
        }

        running_ = true;

        // Start worker threads
        LOG_INFO("Starting " + std::to_string(config_.worker_thread_count) +
            " IOCP worker threads");

        for (size_t i = 0; i < config_.worker_thread_count; ++i) {
            worker_threads_.emplace_back([this] { IOCPWorkerThread(); });
        }

        // Submit initial receive operations
        LOG_INFO("Submitting " + std::to_string(config_.concurrent_operations) +
            " concurrent receive operations");

        for (size_t i = 0; i < config_.concurrent_operations; ++i) {
            IOContext* context = AllocateContext();
            if (context) {
                if (!SubmitReceive(context)) {
                    LOG_WARN("Failed to submit initial receive operation");
                    FreeContext(context);
                }
            }
        }

        LOG_INFO("Successfully connected to filter port with IOCP");

        return common::Result<void>::Success();
    }

    void IOCPFilterPortCommunicator::Disconnect() {
        if (!IsConnected()) {
            return;
        }

        LOG_INFO("Disconnecting from filter port");

        running_ = false;

        // Cancel outstanding I/O
        if (filter_port_ != INVALID_HANDLE_VALUE) {
            LOG_INFO("Cancelling pending I/O...");
            CancelIoEx(filter_port_, nullptr);
        }

        // Post completions before closing the IOCP
        if (iocp_handle_) {
            LOG_DEBUG("Posting dummy completions to unblock threads...");
            for (size_t i = 0; i < worker_threads_.size(); ++i) {
                PostQueuedCompletionStatus(iocp_handle_, 0, 0, nullptr);
            }
        }

        // Wait for worker threads
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads_.clear();
        LOG_DEBUG("worker threads cleared");
        // Wait for dispatch thread

        // Close iocp handle
        if (iocp_handle_) {
            LOG_DEBUG("Now closing IOCP handle...");
            CloseHandle(iocp_handle_);
            iocp_handle_ = nullptr;
            LOG_DEBUG("iocp_handle closed");
        }

        // Close filter port
        if (filter_port_ != INVALID_HANDLE_VALUE) {
            CloseHandle(filter_port_);
            filter_port_ = INVALID_HANDLE_VALUE;
            LOG_DEBUG("filter_port closed");
        }
        // Free buffer pool
        for (auto& context : context_pool_) {
            if (context->message_buffer) {
                _aligned_free(context->message_buffer);
                context->message_buffer = nullptr;
            }
        }
        context_pool_.clear();

        event_queue_.Close();
        LOG_INFO("Disconnected from filter port");
    }

    bool IOCPFilterPortCommunicator::IsConnected() const {
        return filter_port_ != INVALID_HANDLE_VALUE &&
            iocp_handle_ != nullptr &&
            running_.load();
    }

    std::optional<data::Event> IOCPFilterPortCommunicator::ReceiveEvent(
        std::chrono::milliseconds timeout) {

        return event_queue_.TryPop(timeout);
    }

    IOCPFilterPortCommunicator::IOContext*
        IOCPFilterPortCommunicator::AllocateContext(
        std::chrono::milliseconds timeout) {

        std::unique_lock<std::mutex> lock(context_pool_mutex_);

        // Wait with timeout instead of blocking forever
        if (!context_available_.wait_for(lock, timeout, [this] {
            return !running_.load() ||
                std::any_of(context_pool_.begin(), context_pool_.end(),
                    [](const auto& ctx) { return !ctx->in_use; });
            })) {
            return nullptr;
        }

        if (!running_.load()) {
            return nullptr;
        }

        // Find available context
        for (auto& context : context_pool_) {
            if (!context->in_use) {
                context->in_use = true;
                return context.get();
            }
        }

        return nullptr;
    }

    void IOCPFilterPortCommunicator::FreeContext(IOContext* context) {
        if (!context) return;

        {
            std::lock_guard<std::mutex> lock(context_pool_mutex_);
            context->in_use = false;
        }
        context_available_.notify_one();
    }

    bool IOCPFilterPortCommunicator::SubmitReceive(IOContext* context) {
        if (!context || !IsConnected()) {
            return false;
        }

        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        context->submit_time = std::chrono::steady_clock::now();

        HRESULT hr = FilterGetMessage(
            filter_port_,
            reinterpret_cast<PFILTER_MESSAGE_HEADER>(context->message_buffer),
            static_cast<DWORD>(context->buffer_size),
            &context->overlapped
        );

        if (hr == HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
            // Operation pending - this is expected
            return true;
        }
        else if (SUCCEEDED(hr)) {
            // Completed synchronously - post manually
            PostQueuedCompletionStatus(
                iocp_handle_,
                0,
                reinterpret_cast<ULONG_PTR>(context),
                &context->overlapped
            );
            return true;
        }
        else {
            LOG_ERR("FilterGetMessage failed: 0x" + std::to_string(hr));
            return false;
        }
    }

    void IOCPFilterPortCommunicator::IOCPWorkerThread() {

        LOG_DEBUG("IOCP worker thread started");

        while (running_.load()) {
            LOG_DEBUG("IOCPWorkerThread()");
            DWORD bytes_transferred = 0;
            ULONG_PTR completion_key = 0;
            LPOVERLAPPED overlapped = nullptr;

            BOOL result = GetQueuedCompletionStatus(
                iocp_handle_,
                &bytes_transferred,
                &completion_key,
                &overlapped,
                1000  // 1 second timeout
            );

            if (!result) {
                DWORD error = GetLastError();

                if (error == WAIT_TIMEOUT) {
                    LOG_DEBUG("IOCPWorkerThread() WAIT_TIMEOUT");
                    continue;
                }

                if (error == ERROR_ABANDONED_WAIT_0) {
                    // IOCP closed
                    LOG_DEBUG("IOCPWorkerThread() IOCP closed");
                    break;
                }

                if (overlapped) {
                    LOG_DEBUG("IOCPWorkerThread() overlapped");
                    // Get context from overlapped
                    IOContext* context = CONTAINING_RECORD(
                        overlapped, IOContext, overlapped);

                    LOG_ERR("GetQueuedCompletionStatus failed: " +
                        std::to_string(error));

                    // Resubmit
                    if (running_.load() && !SubmitReceive(context)) {
                        FreeContext(context);
                    }
                }

                continue;
            }

            if (!overlapped) {
                LOG_DEBUG("IOCPWorkerThread() Spurious wakeup");

                if (!running_.load()) {
                    LOG_DEBUG("IOCPWorkerThread() dummy wakeup -> exit");
                    break;
                }
                continue;
            }

            // Get context
            IOContext* context = CONTAINING_RECORD(overlapped, IOContext, overlapped);

            if (bytes_transferred > 0) {
                ProcessCompletedIO(context, bytes_transferred);
            }

            // Resubmit for next message
            if (running_.load() && !SubmitReceive(context)) {
                FreeContext(context);
            }
        }

        LOG_DEBUG("IOCP worker thread stopped");
    }

    void IOCPFilterPortCommunicator::ProcessCompletedIO(
        IOContext* context, DWORD bytes_transferred) {

        LOG_DEBUG("ProcessCompletedIO()");
        // Calculate latency
        auto now = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            now - context->submit_time);

        total_messages_++;
        total_latency_us_ += latency.count();
        LOG_DEBUG("parsing message");
        // Parse message
        auto* kernel_msg = reinterpret_cast<KernelMessage*>(context->message_buffer);

        // Convert to event data
        data::Event event;
        LOG_DEBUG("executing parser!!");
        auto e = MessageParser::Parse(kernel_msg, bytes_transferred);
        LOG_DEBUG("parser executed!!!");
        if (e.IsSuccess()) {
            event = e.Value();
        }
        else {
            LOG_WARN("Unable to parse kernel message: " + e.ErrorMessage());
        }
        LOG_DEBUG("message parsed, dispatching now!!");
        // Queue for dispatch
        if (!event_queue_.TryPush(event, std::chrono::milliseconds(10))) {
            LOG_WARN("Event queue full, dropping event");
        }
        // Send reply to driver
        // current we're sending this ack to kernel driver we'll need to revisit it
        // once we're done with complemte kernel filter design implementation
        SendReply(reinterpret_cast<FILTER_MESSAGE_HEADER*>(context->message_buffer), S_OK);
        LOG_DEBUG("ProcessCompletedIO() completed");
    }

    bool IOCPFilterPortCommunicator::SendReply(
        const FILTER_MESSAGE_HEADER* msg_header, HRESULT status) {

        struct FilterReply {
            FILTER_REPLY_HEADER header;
            HRESULT result;
        } reply = {};

        reply.header.Status = status;
        reply.header.MessageId = msg_header->MessageId;
        reply.result = status;

        HRESULT hr = FilterReplyMessage(
            filter_port_,
            &reply.header,
            sizeof(reply)
        );

        return SUCCEEDED(hr);
    }

    IOCPFilterPortCommunicator::PerformanceMetrics
        IOCPFilterPortCommunicator::GetPerformanceMetrics() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_stats_time_).count();

        uint64_t current_count = total_messages_.load();
        uint64_t messages_delta = current_count - last_message_count_;

        uint64_t messages_per_sec = elapsed > 0 ? messages_delta / elapsed : 0;

        uint64_t avg_latency = current_count > 0 ?
            total_latency_us_.load() / current_count : 0;

        size_t buffers_in_use = 0;
        {
            std::lock_guard<std::mutex> lock(
                const_cast<std::mutex&>(context_pool_mutex_));
            buffers_in_use = std::count_if(
                context_pool_.begin(), context_pool_.end(),
                [](const auto& ctx) { return ctx->in_use; });
        }

        return PerformanceMetrics{
            current_count,
            messages_per_sec,
            avg_latency,
            buffers_in_use,
            config_.buffer_pool_size - buffers_in_use
        };
    }

} // namespace kubearmor::comm

