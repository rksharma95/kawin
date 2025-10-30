#pragma once

#include <string>
#include <variant>
#include <stdexcept>

namespace kubearmor::common {

    template<typename T>
    class Result {
    public:
        static Result Success(T value) {
            return Result(std::move(value));
        }

        static Result Error(std::string error) {
            return Result(std::move(error));
        }

        bool IsSuccess() const {
            return std::holds_alternative<T>(data_);
        }

        bool IsError() const {
            return std::holds_alternative<std::string>(data_);
        }

        const T& Value() const {
            if (IsError()) {
                throw std::runtime_error("Attempted to get value from error result: " + ErrorMessage());
            }
            return std::get<T>(data_);
        }

        T& Value() {
            if (IsError()) {
                throw std::runtime_error("Attempted to get value from error result: " + ErrorMessage());
            }
            return std::get<T>(data_);
        }

        const std::string& ErrorMessage() const {
            if (IsSuccess()) {
                throw std::runtime_error("Attempted to get error from success result");
            }
            return std::get<std::string>(data_);
        }

        explicit operator bool() const {
            return IsSuccess();
        }

    private:
        explicit Result(T value) : data_(std::move(value)) {}
        explicit Result(std::string error) : data_(std::move(error)) {}

        std::variant<T, std::string> data_;
    };

    template<>
    class Result<void> {
    public:
        static Result Success() {
            return Result(true);
        }

        static Result Error(std::string error) {
            return Result(std::move(error));
        }

        bool IsSuccess() const { return success_; }
        bool IsError() const { return !success_; }

        const std::string& ErrorMessage() const {
            if (IsSuccess()) {
                throw std::runtime_error("Attempted to get error from success result");
            }
            return error_;
        }

        explicit operator bool() const { return IsSuccess(); }

    private:
        explicit Result(bool success) : success_(success) {}
        explicit Result(std::string error) : success_(false), error_(std::move(error)) {}

        bool success_;
        std::string error_;
    };

} // namespace kubearmor::common