# Tracebox future improvements

This document separates technically justified follow-up work from the current
architecture review. Items marked implemented are no longer pending; the
remaining items are intentionally outside the focused lifecycle change.

## Short-term improvements

These items can be addressed without changing the storage or network format if
the behavior is specified first.

### Make shutdown ownership explicit (implemented)

`Server` now owns a joinable thread, synchronized lifecycle state, and an
eventfd wakeup. Destruction stops and joins before callback targets are
destroyed; active clients are closed by the service thread. `LogWriter` stops
admission, drains accepted requests, and joins its worker. Bounded storage
cancellation and transport resource limits remain separate follow-up work.

### Bound ingress and response memory

Add configured limits for framed payload length, queued requests, individual
record size, and read response size. Reject or report an over-limit request
before allocating its complete payload. This is justified by the current
unbounded allocations and is especially important for a network-facing service
and embedded deployments.

### Define and report write outcomes

Decide whether the write protocol needs an acknowledgement. If it does, return
an explicit request result or sequence identifier and propagate `Storage::write`
failures out of `LogWriter`. If it does not, document that send success means
transport acceptance only. This is justified because storage failures are
currently invisible to producers.

### Harden file validation

Add explicit checks for header size, file type, record alignment, offset bounds,
payload bounds, and trailing partial records. Return structured corruption
status rather than relying on zero counts or generic read errors. This can be
implemented behind the current reader APIs and is justified by the current
partial corruption detection.

### Make test and coverage configuration reproducible

Keep the Docker commands as the reference environment and add a documented
coverage target/configuration that excludes generated files and build-system
compiler probes. This is justified by the need to distinguish production
coverage from generated protobuf and local build artifacts.

### Clarify protocol defaults

Specify the behavior of omitted `start_idx`, `max_count`, file paths, empty
responses, and malformed protobufs. In particular, align server defaults with
client defaults so an omitted maximum does not unexpectedly produce zero data.

## Medium-term improvements

These changes affect internals and should be designed with compatibility tests
before implementation.

### Introduce a storage metadata/index layer

Maintain day and stream metadata incrementally instead of reopening every index
file for each discovery query. A small sidecar or in-memory-on-startup index
could preserve the current files while reducing directory and file-header scan
cost. This is justified by the O(number of files) discovery path and linear
record retrieval.

### Add explicit durability policy

Separate C++ stream flush from media synchronization and expose a configurable
durability mode. Document the latency tradeoff between queued, OS-buffered,
flushed, and synchronized writes. This is justified because `flush()` alone
does not establish power-loss durability.

### Make retention accounting transactional

Centralize file-size accounting and deletion decisions, account for actual
bytes written, and define behavior when a write exceeds the complete capacity.
Consider deleting or reclaiming orphaned variable data during startup. This is
justified by day-granular deletion, conservative estimates, and orphaned data
after interrupted variable writes.

### Separate time partitioning from local process time

Define the timezone policy and use a thread-safe, explicit conversion API. Add
tests around daylight-saving transitions and timestamps near midnight. This is
justified because local timezone state controls directory placement and can
vary between processes or deployments.

### Improve network framing robustness

Use a specified byte order, loop on partial sends, define a maximum frame size,
and add a protocol/version envelope if independent clients are expected. This
is justified by the current native-endian framing and single-call send behavior.

### Add service-level observability

Expose queue depth, dropped/failed writes, scan latency, records returned, and
retention deletions through logs or counters. This is justified because the
write path currently hides storage outcomes and the reader has potentially
expensive scans.

## Long-term redesign opportunities

These are larger changes and should not be started until the open questions in
the architecture document are resolved.

### Define a versioned, portable storage format

Replace direct packed-struct persistence with an explicitly specified byte
order, widths, checksums, record framing, and compatibility/version rules. Add
crash-recovery metadata or a journal if the durability target requires it. The
technical justification is portability and reliable corruption/recovery
behavior across architectures and interrupted writes.

### Use a bounded, backpressured ingestion pipeline

Replace the unbounded queue with explicit capacity, admission policy, and
possibly multiple storage workers or batching. The design must preserve per-
stream ordering if that is required. This is justified by the current ability
for producers to exhaust RAM when storage is slower.

### Reconsider the query/index architecture

For large stores, evaluate immutable segments, sparse time indexes, and
background compaction or a purpose-built embedded index. The goal would be to
avoid full directory scans and linear record walks while preserving predictable
flash behavior. This is justified only if measured workloads show the current
O(files + records scanned) behavior is insufficient.

### Introduce a platform abstraction for networking and storage

Split the POSIX/epoll transport from the service protocol and split the file
backend from storage policy. This would allow a smaller embedded transport,
RTOS filesystem, or alternate persistence backend without carrying Linux-only
assumptions into the core. The current `FileIO` seam is a useful first boundary,
but the network layer has no equivalent abstraction.

### Establish a security and compatibility contract

If the services leave a trusted local network, add authentication, encryption,
authorization, replay/size protections, and negotiated protocol versions. This
is justified by the current unauthenticated TCP protocol and absolute-path data
returned in read responses; it is not necessary for a strictly isolated local
deployment unless that deployment model changes.
