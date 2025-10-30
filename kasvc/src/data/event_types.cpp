#include "data/event_types.h"
#include <sstream>

namespace kubearmor::data {

    std::string FileEventData::ToString() const {
        std::ostringstream oss;
        oss << "File[op=";
        switch (operation) {
        case FileOperation::F_CREATE: oss << "CREATE"; break;
        case FileOperation::F_READ: oss << "READ"; break;
        case FileOperation::F_WRITE: oss << "WRITE"; break;
        case FileOperation::F_DELETE: oss << "DELETE"; break;
        case FileOperation::F_RENAME: oss << "RENAME"; break;
        default: oss << "UNKNOWN"; break;
        }
        oss << ", path=" << file_path;
        return oss.str();
    }

    std::string ProcessEventData::ToString() const {
        std::ostringstream oss;
        oss << "Process[op=";
        switch (operation) {
        case ProcessOperation::P_CREATE: oss << "CREATE"; break;
        case ProcessOperation::P_TERMINATE: oss << "TERMINATE"; break;
        case ProcessOperation::P_OPEN_HANDLE: oss << "OPEN_HANDLE"; break;
        default: oss << "UNKNOWN"; break;
        }
        oss << ", pid=" << process_id << ", path=" << process_path;
        if (!command_line.empty()) oss << ", cmd=" << command_line;
        oss << "]";
        return oss.str();
    }

    std::string NetworkEventData::ToString() const {
        std::ostringstream oss;
        oss << "Network[op=";
        switch (operation) {
        case NetworkOperation::TCP_CONNECT: oss << "TCP_CONNECT"; break;
        case NetworkOperation::TCP_ACCEPT: oss << "TCP_ACCEPT"; break;
        case NetworkOperation::TCP_SEND: oss << "TCP_SEND"; break;
        default: oss << "UNKNOWN"; break;
        }
        oss << ", " << local_address << ":" << local_port
            << " <-> " << remote_address << ":" << remote_port
            << ", bytes=" << data_length << "]";
        return oss.str();
    }

    std::string Event::ToString() const {
        std::ostringstream oss;
        oss << "Event[id=" << event_id << ", operation_type=";
        switch (operation_type) {
        case EventOperationType::FILE_EVENT: oss << "FILE"; break;
        case EventOperationType::PROCESS_EVENT: oss << "PROCESS"; break;
        case EventOperationType::NETWORK_EVENT: oss << "NETWORK"; break;
        }
        switch (type) {
        case EventType::HOST_LOG: oss << "HOST_LOG"; break;
        case EventType::MATCH_HOST_POLICY: oss << "MATCH_HOST_POLICY"; break;
        }
        oss << ", blocked=" << (blocked ? "YES" : "NO");
        if (auto* fd = GetFileData()) oss << fd->ToString();
        else if (auto* pd = GetProcessData()) oss << pd->ToString();
        else if (auto* nd = GetNetworkData()) oss << nd->ToString();

        oss << "]";
        return oss.str();
    }

    bool Event::IsHighSeverity() const {
        if (blocked) return true;
        if (auto* fd = GetFileData()) {
            return fd->operation == FileOperation::F_DELETE ||
                fd->operation == FileOperation::F_WRITE;
        }
        if (auto* pd = GetProcessData()) {
            return pd->operation == ProcessOperation::P_CREATE ||
                pd->operation == ProcessOperation::P_TERMINATE;
        }
        if (auto* nd = GetNetworkData()) {
            return nd->operation == NetworkOperation::TCP_CONNECT;
        }
        return false;
    }

} // namespace kubearmor::data