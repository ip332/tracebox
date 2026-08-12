# Tracebox public API

## Purpose

This document defines the supported embedding surface introduced by PR-2. The
public headers live under `include/tracebox/` and expose only value types and
small PImpl-backed façades. They do not expose protobuf messages, POSIX socket
types, filesystem types, mutexes, file streams, or the internal storage
classes.

The implementation remains in `src/tracebox_api.cpp` and the existing
`libs/` targets. The public façade delegates to the existing implementation;
it does not change the storage format, TCP framing, protobuf schema, retention
policy, or write/read behavior.

## Public headers

| Header | Public symbols | Purpose |
| --- | --- | --- |
| `tracebox/timestamp.h` | `Timestamp`, `day()` | Timestamp alias and local `YYYYMMDD` conversion |
| `tracebox/stream.h` | `StreamStatus`, `Sample`, `Stream`, `ReadResult` | Protocol-independent read-side value types |
| `tracebox/recorder.h` | `Recorder` | Client for sending records to the logging service |
| `tracebox/reader.h` | `Reader` | Client for discovering streams and reading records |
| `tracebox/storage.h` | `Record`, `Storage` | Synchronous local storage façade |
| `tracebox/writer.h` | `Writer` | Asynchronous local storage façade |

All public symbols are directly under `namespace tracebox`.

### `Recorder`

```cpp
#include <tracebox/recorder.h>

tracebox::Recorder recorder("imu", 16);  // zero means variable-size records
if (recorder.connect("127.0.0.1", 49999)) {
    recorder.record(payload, timestamp_ns);
}
```

`Recorder` owns one client connection through its private implementation. A
successful `record()` means that the framed request was accepted by the TCP
client; the current protocol has no persistence acknowledgement. The object
is move-only and disconnects on destruction through the internal client.

### `Reader`

```cpp
#include <tracebox/reader.h>

tracebox::Reader reader;
reader.connect("127.0.0.1", 49998);
auto streams = reader.streams(start_ns, end_ns);
if (!streams.empty()) {
    auto result = reader.read(streams.front().file, start_ns, end_ns);
}
```

`Reader` owns one read-service connection. `streams()` returns a vector of
public `Stream` values. `read()` returns `ReadResult`; transport or server
errors may leave the vector empty, and storage/read errors are reported in its
`error` string when the current implementation provides one. The object is
move-only and disconnects on destruction.

`StreamStatus` preserves the current protobuf status values without requiring a
protobuf include in user code. The numeric values are intentionally aligned
with the current protocol enum; changing them would require a compatibility
review.

### `Storage` and `Record`

```cpp
#include <tracebox/storage.h>

tracebox::Storage storage("/var/lib/tracebox", 64 * 1024 * 1024);
tracebox::Record record{"imu", 0, payload, timestamp_ns};
int result = storage.write(record);
```

`Storage` is the synchronous local persistence façade. It owns its private
implementation and is non-copyable but movable. The folder path is accepted as
a string so the public header does not expose `std::filesystem`.

`Record::record_size` follows the existing convention: zero denotes a
variable-size channel; nonzero denotes a fixed-size channel. For fixed-size
records, the payload length must match `record_size` or the existing writer
returns its negative invalid-argument result.

### `Writer`

```cpp
#include <tracebox/writer.h>

auto storage = std::make_shared<tracebox::Storage>(folder, max_bytes);
tracebox::Writer writer(storage);
writer.add(record);
```

`Writer` owns the asynchronous worker associated with the supplied storage
object. `add()` copies the record into the existing queue. The worker is joined
when the `Writer` is destroyed. The supplied `Storage` must outlive the writer
or be retained by the shared pointer passed to its constructor; a null pointer
is invalid input.

### Value types

* `Timestamp` is an unsigned 64-bit Unix-epoch nanosecond value.
* `Sample` contains a timestamp, payload, and the stream index assigned by the
  read client/combiner.
* `Stream` describes one index file and its cached time range.
* `ReadResult` contains returned samples and an optional textual error.
* `day()` uses the local timezone, matching the current on-disk day-directory
  behavior.

## Ownership and lifetime

Public façade classes use `std::unique_ptr` PImpl storage. This keeps internal
headers out of the public include graph and makes the public object layout
independent of sockets, protobuf-generated types, file streams, and worker
threads.

| Object | Ownership | Destruction behavior |
| --- | --- | --- |
| `Recorder` | Owns its client implementation and socket | Disconnects the client |
| `Reader` | Owns its client implementation and socket | Disconnects the client |
| `Storage` | Owns one internal storage object | Closes active writers through normal destruction |
| `Writer` | Owns one internal asynchronous writer; shares internal storage ownership | Signals and joins the writer worker |
| `Record`, `Sample`, `Stream`, `ReadResult` | Value ownership of strings/vectors | Normal value destruction |

All public classes are non-copyable and movable. A moved-from object is valid
only for destruction or reassignment, as usual for a PImpl-backed move-only
type.

## Thread-safety expectations

The public API does not make a single object generally thread-safe.

* A `Recorder` or `Reader` should be used by one caller at a time because each
  object owns one socket and request/response sequence.
* `Storage` is a synchronous façade over stateful storage management and should
  be externally serialized.
* `Writer::add()` delegates to the existing queue, whose enqueue operation is
  mutex-protected. Concurrent calls are therefore serialized at enqueue time,
  but destruction, move assignment, and `add()` must not overlap.
* Returned values are independent copies and may be processed by other threads
  after the API call returns.

The internal network service threads and storage worker are not exposed through
the public object model. Their lifecycle remains governed by the existing
implementation classes.

## Error model

The current implementation has no public `Error` enum and this PR does not
invent one. Callers should interpret results as follows:

* `connect()` returns `false` when socket creation, address parsing, or
  connection fails.
* `Recorder::record()` returns `false` for invalid fixed-size payloads or a
  transport send failure. It does not confirm persistence.
* `Storage::write()` returns a positive byte count on acceptance, `0` when
  retention cannot free enough space, and a negative errno-style result for
  writer errors.
* `Reader::streams()` returns an empty vector for no matching streams or a
  failed request; the current façade does not separately expose a transport
  error object.
* `Reader::read()` returns `ReadResult::error` when the underlying response
  contains an error. An empty result can also mean no records matched.
* Allocation failures and unexpected implementation exceptions are not
  converted into status values.

These inconsistencies are documented rather than redesigned in this PR. A
future error API must define compatibility and ownership rules before replacing
the current return conventions.

## Intended public versus internal classes

### Intended public classes

* `tracebox::Recorder` for network recording clients.
* `tracebox::Reader` for network read clients.
* `tracebox::Storage` for synchronous local persistence.
* `tracebox::Writer` for asynchronous local persistence.
* The value types in `tracebox/stream.h` and timestamp utility in
  `tracebox/timestamp.h`.

### Internal implementation classes

The following remain implementation details and should not be included by
embedding applications:

* `tracebox::logger::TraceboxLogger`, `LogReader`, and `LogWriter` service
  composition classes.
* `tracebox::logger::Storage`, `Folder`, and `StreamWriter` storage machinery.
* `tracebox::logger::FileIO` and `StandardFileIO` file backends.
* `tracebox::logger::StorageScanner`, `StreamReader`, `FileReader`,
  `IndexFileReader`, and `DataFileReader` readers.
* `tracebox::logger::Server`, `TcpSocket`, and `TcpClient` transport helpers.
* `tracebox::logger::LogClient`, `LogReadClient`, and `StreamsCombiner` protocol
  adapters.
* `AscWriter` and application-specific demo adapters.

The `tracebox::logger` namespace is retained for source compatibility inside
the repository, but it is not the supported embedding namespace. Public code
should include only `tracebox/...` headers and use `namespace tracebox` symbols.

## Header and link dependencies

Public headers require only C++ standard-library headers and the public
`tracebox` headers. They do not include:

* `logger.pb.h` or any protobuf-generated header;
* POSIX socket, epoll, or filesystem headers;
* C++ threading, queue, or stream implementation headers.

The implementation target still links the existing protobuf, networking,
thread, and storage libraries. This is a build-time dependency of the library,
not a type dependency of the public headers. Applications should link the
`Tracebox` target when consuming this façade.

## Extension points

The public API intentionally exposes a small extension surface:

* Callers can provide their own scheduling, queueing, or retry policy around
  `Recorder`, `Reader`, `Storage`, and `Writer`.
* `Record`, `Sample`, and `Stream` are ordinary value types suitable for
  adapters to application-specific formats.
* The internal `FileIO` abstraction remains available to repository tests and
  platform implementation work, but is not a public extension contract yet.
* Protobuf and TCP framing remain internal protocol details; applications that
  need direct protocol access are outside this stable API.

## Naming notes

The public façade uses root-level names and readable forms such as
`records_count`, `connected`, and `timestamp`. Existing implementation names
such as `records_cnt`, `is_connected`, and `time_ns` remain unchanged internally
to avoid unnecessary API/behavior changes. The distinction is intentional for
this PR; a later compatibility review can decide whether legacy names should be
deprecated or aliased.
