#include "data/event_processor.h"
#include <algorithm>
#include <iterator>

namespace kubearmor::data {

    std::vector<Event> EventProcessor::Filter(
        const std::vector<Event>& events,
        FilterPredicate predicate) const {

        std::vector<Event> filtered;
        std::copy_if(events.begin(), events.end(),
            std::back_inserter(filtered), predicate);
        return filtered;
    }

    Event EventProcessor::Enrich(const Event& event) const {
        /*
        TODO:
        This is where we can enrich the received event by updating it with
        any information that is not available in kernel space i.e. namespace, pod,
        etc
        */
        return event;
    }

    EventProcessor::AggregatedStats EventProcessor::Aggregate(
        const std::vector<Event>& events) const {

        AggregatedStats stats;
        stats.total_events = events.size();
        stats.blocked_events = 0;

        for (const auto& event : events) {
            if (event.blocked) stats.blocked_events++;
            stats.events_by_type[event.operation_type]++;
        }

        return stats;
    }

} // namespace kubearmor::data