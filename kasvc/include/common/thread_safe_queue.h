#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

namespace kubearmor::common {

    template<typename T>
    class ThreadSafeQueue {
    public:
        explicit ThreadSafeQueue(size_t max_size = 10000)
            : max_size_(max_size), closed_(false) {
        }

        ~ThreadSafeQueue() {
            Close();
        }

        // Push item (blocks if full)
        bool Push(const T& item) {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_not_full_.wait(lock, [this] {
                return queue_.size() < max_size_ || closed_;
                });

            if (closed_) return false;

            queue_.push(item);
            cv_not_empty_.notify_one();
            return true;
        }

        bool Push(T&& item) {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_not_full_.wait(lock, [this] {
                return queue_.size() < max_size_ || closed_;
                });

            if (closed_) return false;

            queue_.push(std::move(item));
            cv_not_empty_.notify_one();
            return true;
        }

        // Try push with timeout
        template<typename Rep, typename Period>
        bool TryPush(const T& item, std::chrono::duration<Rep, Period> timeout) {
            std::unique_lock<std::mutex> lock(mutex_);

            if (!cv_not_full_.wait_for(lock, timeout, [this] {
                return queue_.size() < max_size_ || closed_;
                })) {
                return false;
            }

            if (closed_) return false;

            queue_.push(item);
            cv_not_empty_.notify_one();
            return true;
        }

        // Pop item (blocks if empty)
        std::optional<T> Pop() {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_not_empty_.wait(lock, [this] {
                return !queue_.empty() || closed_;
                });

            if (closed_ && queue_.empty()) {
                return std::nullopt;
            }

            T item = std::move(queue_.front());
            queue_.pop();
            cv_not_full_.notify_one();
            return item;
        }

        // Try pop with timeout
        template<typename Rep, typename Period>
        std::optional<T> TryPop(std::chrono::duration<Rep, Period> timeout) {
            std::unique_lock<std::mutex> lock(mutex_);

            if (!cv_not_empty_.wait_for(lock, timeout, [this] {
                return !queue_.empty() || closed_;
                })) {
                return std::nullopt; // Timeout
            }

            if (closed_ && queue_.empty()) {
                return std::nullopt;
            }

            T item = std::move(queue_.front());
            queue_.pop();
            cv_not_full_.notify_one();
            return item;
        }

        bool Empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        size_t Size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

        void Close() {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            cv_not_empty_.notify_all();
            cv_not_full_.notify_all();
        }

        bool IsClosed() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return closed_;
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            std::queue<T> empty;
            std::swap(queue_, empty);
            cv_not_full_.notify_all();
        }

    private:
        mutable std::mutex mutex_;
        std::condition_variable cv_not_empty_;
        std::condition_variable cv_not_full_;
        std::queue<T> queue_;
        size_t max_size_;
        bool closed_;
    };

} // namespace kubearmor::common