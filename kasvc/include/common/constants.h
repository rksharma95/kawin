#pragma once

#include <cstddef>
#include <chrono>

namespace kubearmor::constants {

	// Queue sizes
	constexpr size_t MAX_EVENT_QUEUE_SIZE = 10000;

	// Thread counts
	constexpr size_t FILTER_PORT_WORKER_THREADS = 4;

	// Buffer sizes
	constexpr size_t FILTER_MESSAGE_BUFFER_SIZE = 4096;

} // namespace kubearmor::constants