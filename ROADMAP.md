# Tracebox roadmap

This roadmap reflects the architecture review. It describes future work only;
the current implementation and wire/storage formats remain unchanged.

## Phase 1 — Core platform

* Core recorder
* Storage
* Reader
* Writer
* Docker
* CI

## Phase 2 — Reliability and hardware integration

* Storage abstraction
* Transport abstraction
* Crash recovery
* Flash optimization

## Phase 3 — Advanced data services

* Compression
* Encryption
* Snapshots
* Triggers
* Replication
* Streaming

## Phase 4 — Ecosystem integrations

* Multiple storage backends
* Web UI
* REST/gRPC
* Cloud synchronization

## Cross-phase engineering priorities

These concerns should be addressed at the phase where they become necessary:

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

Before starting medium- or long-term work, resolve:

* required durability level;
* maximum record, queue, response, and client counts;
* portability requirements beyond Linux/POSIX;
* timestamp ordering and timezone policy;
* storage compatibility requirements;
* whether the network protocol is private or externally supported.
