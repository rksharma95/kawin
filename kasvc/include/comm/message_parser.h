#pragma once

#include "comm/kernel_message.h"
#include "data/event_types.h"
#include "common/result.h"

namespace kubearmor::comm {

    class MessageParser {
    public:
        static common::Result<data::Event> Parse(const KernelMessage* kernel_msg, size_t buffer_size);

    private:
        static data::FileEventData ParseFileEvent(const KernelMessage* km, size_t buffer_size);
        static data::ProcessEventData ParseProcessEvent(const KernelMessage* km, size_t buffer_size);
        static data::NetworkEventData ParseNetworkEvent(const KernelMessage* km, size_t buffer_size);
        static std::string WStringToString(const wchar_t* wstr, size_t length);
        static std::string FormatIPAddress(const uint8_t* addr, uint8_t family);
    };

} // namespace kubearmor::comm