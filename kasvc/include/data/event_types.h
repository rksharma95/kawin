#pragma once

#include <string>
#include <chrono>
#include <variant>

namespace kubearmor::data {

    enum class EventType : uint32_t {
        HOST_LOG = 1,
        MATCH_HOST_POLICY = 2
    };

    enum class EventOperationType : uint32_t {
        FILE_EVENT = 1,
        PROCESS_EVENT = 2,
        NETWORK_EVENT = 3
    };

    enum class FileOperation : uint32_t {
        F_CREATE = 0, 
        F_READ = 1,
        F_WRITE = 2,
        F_DELETE = 3,
        F_RENAME = 4,
        F_SETINFO = 5,
        F_CLEANUP = 6,
        F_CLOSE = 7
    };

    enum class ProcessOperation : uint32_t {
        P_CREATE = 0, P_TERMINATE = 1, P_OPEN_HANDLE = 2, P_DUPLICATE_HANDLE = 3
    };

    enum class NetworkOperation : uint32_t {
        TCP_CONNECT = 0, 
        TCP_ACCEPT = 1, 
        TCP_SEND = 2, 
        TCP_RECEIVE = 3,
        UDP_SEND = 4, 
        UDP_RECEIVE = 5
    };

    struct FileEventData {
        FileOperation operation;
        uint32_t process_id;
        std::string process_path;
        std::string file_path;

        FileEventData() : operation(FileOperation::F_CREATE) {
        }
        std::string ToString() const;
    };

    struct ProcessEventData {
        ProcessOperation operation;
        uint32_t process_id;
        uint32_t parent_process_id;
        std::string process_path;
        std::string command_line;
        std::string parent_process_path;

        ProcessEventData() : operation(ProcessOperation::P_CREATE) {
        }
        std::string ToString() const;
    };

    struct NetworkEventData {
        NetworkOperation operation;
        uint32_t protocol;
        uint16_t local_port;
        uint16_t remote_port;
        std::string local_address;
        std::string remote_address;
        uint32_t data_length;

        NetworkEventData() : operation(NetworkOperation::TCP_CONNECT), protocol(0),
            local_port(0), remote_port(0), data_length(0) {
        }
        std::string ToString() const;
    };

    struct Event {
        EventType type;
        EventOperationType operation_type;
        uint64_t event_id;
        std::chrono::system_clock::time_point timestamp;
        bool blocked;

        std::variant<FileEventData, ProcessEventData, NetworkEventData> data;

        Event() : operation_type(EventOperationType::FILE_EVENT), event_id(0),
            timestamp(std::chrono::system_clock::now()),blocked(false), data(FileEventData{}) {
        }

        bool IsFileEvent() const { return operation_type == EventOperationType::FILE_EVENT; }
        bool IsProcessEvent() const { return operation_type == EventOperationType::PROCESS_EVENT; }
        bool IsNetworkEvent() const { return operation_type == EventOperationType::NETWORK_EVENT; }

        bool IsAlert() const { return type == EventType::MATCH_HOST_POLICY; }

        const FileEventData* GetFileData() const {
            return std::get_if<FileEventData>(&data);
        }
        const ProcessEventData* GetProcessData() const {
            return std::get_if<ProcessEventData>(&data);
        }
        const NetworkEventData* GetNetworkData() const {
            return std::get_if<NetworkEventData>(&data);
        }

        std::string ToString() const;
        bool IsHighSeverity() const;
    };

} // namespace kubearmor::data