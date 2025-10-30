#pragma once

#include "data/event_types.h"  // Changed
#include <vector>
#include <functional>
#include <map>

namespace kubearmor::data {

    class EventProcessor {
    public:
        using FilterPredicate = std::function<bool(const Event&)>;

        std::vector<Event> Filter(
            const std::vector<Event>& events,
            FilterPredicate predicate) const;

        Event Enrich(const Event& event) const;

        struct AggregatedStats {
            size_t total_events;
            size_t blocked_events;
            std::map<EventOperationType, size_t> events_by_type;
        };

        AggregatedStats Aggregate(const std::vector<Event>& events) const;
    };

} // namespace kubearmor::data