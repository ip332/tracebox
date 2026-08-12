# Tracebox roadmap

This roadmap reflects the architecture review. It distinguishes completed,
partial, and planned work; the current public API and wire/storage formats
remain unchanged.

## Completed

* Tracebox public API
* Core recorder, reader, writer, and storage implementations
* Storage backend isolation
* Docker development image
* GCC/Clang CI validation
* Architecture, public API, storage, and durability documentation

## Partial / in progress

* Crash recovery: durability and recovery semantics are documented, and
  corruption/recovery tests cover current behavior; startup recovery is not yet
  implemented.
* Storage abstraction: the internal backend boundary exists, but additional
  backends and hardware-specific optimization are not implemented.
* Reliability hardening: validation and fault-injection coverage exists, while
  lifecycle, bounds, and observability work remains.

## Planned: reliability and hardware integration

* Transport abstraction
* Flash optimization
* Full crash recovery and durable metadata recovery

## Planned: advanced data services

* Compression
* Encryption
* Snapshots
* Triggers
* Replication
* Streaming

## Planned: ecosystem integrations

* Multiple storage backends
* Web UI
* REST/gRPC
* Cloud synchronization

## Cross-phase engineering priorities

* Make service shutdown owned and joinable; remove detached-thread lifetime and
  shutdown data races.
* Bound TCP frame size, record size, queue depth, and read response size.
* Define whether write success means queued, flushed, or durable.
* Strengthen file validation for headers, record boundaries, offsets, and
  truncated records.
* Define timezone behavior and timestamp ordering.
* Add observability for queue depth, failed writes, scan latency, and retention
  activity.

## Decision gates

Before starting medium- or long-term work, resolve required durability,
resource limits, portability beyond Linux/POSIX, timestamp policy, storage
compatibility, and whether the network protocol is private or externally
supported.
