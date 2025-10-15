# kawin Design Document


## Purpose

This document serves as a record of design decisions, open questions, and architectural direction for the `kawin` project.

## Goals

- Intercept and monitor process,file,network,(maybe)registry system operations
- Policy based enforcement for monitored events.
- Logging the telemetry events for the monitored events.
- User-mode communication for configuration and policy management.

## :question: Open Questions

:gear: Logging

- What is the final format for logs (e.g., JSON, binary, protobuf)?

- How should the user-mode service forward logs to external systems?

	- HTTP? gRPC? any other alternative?

:lock: Security

- How do we secure communication between:

	- Minifilter :left_right_arrow: user-mode service (port access control)?

	- User-mode service :left_right_arrow: external endpoint i.e. TLS?

:wrench: Testing

- What critical paths and edge cases must be tested to ensure the minifilter never causes a BSOD?
  - ensure are we correctly handling all IRQL levels, memory allocations, and synchronization?

:package: Packaging & Signing

- How should we package the system for production?

- How will the driver be signed for production?

:chart_with_upwards_trend: Performance

- What is the expected overhead of the minifilter under normal and high I/O load?

## :triangular_ruler: Potential Architecture (Very Early Sketch)

- `DriverEntry` sets up communication port
- Register pre- and post-operation callbacks
- manage rules in non-paged memory with minifilter driver
- User-mode interface using `FltSendMessage`

![High-level-architecture](./assets/kawin-arch.svg)

---

## :speaking_head: Notes

Feel free to add your thoughts, questions, or comments directly in this doc, or start a GitHub Discussion.
