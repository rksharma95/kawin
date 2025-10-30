#pragma once

#include "data/event_types.h"
#include <vector>

namespace kubearmor::app {

    class IEventPublisher {
    public:
        virtual ~IEventPublisher() = default;

        virtual void Publish(const data::Event& event) = 0;
        virtual void PublishBatch(const std::vector<data::Event>& events) = 0;
        virtual size_t GetSubscriberCount() const = 0;

        struct PublisherStatistics {
            uint64_t events_published;
            uint64_t events_dropped;
            size_t active_subscribers;
            size_t queue_size;
        };

        virtual PublisherStatistics GetStatistics() const = 0;
    };

} // namespace kubearmor::app