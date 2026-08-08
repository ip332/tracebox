# Tracebox durability and corruption recovery

## Scope

This document defines what the current implementation does and does not
guarantee when writes are interrupted or persisted files are damaged. It does
not introduce a new storage format, recovery journal, alternate backend, or
public durability API.

The current format and behavior remain the compatibility reference. The only
reader change made with this review is defensive: malformed variable-record
lengths/offsets and truncated payloads now fail the read instead of being
reported as successful reads.

## Write path and persistence points

### Asynchronous public writer path

For `tracebox::Writer::add(record)`:

1. The public façade copies `Record` fields into a protobuf `LogRequest`.
2. `logger::LogWriter::add()` copies that request into an unbounded
   `std::queue` under a mutex and notifies its worker condition variable.
3. `LogWriter`'s worker copies one queued request into a local object and calls
   `logger::Storage::write()`.
4. `Storage` checks the conservative byte estimate and may synchronously delete
   the oldest day directory.
5. `FilesystemStorageBackend` selects/creates the day `Folder`.
6. `Folder` finds or creates a `StreamWriter` for the channel.
7. `StreamWriter` opens files and writes the record through `FileIO`.

`Writer::add()` returns after queue insertion. It does not mean that the worker
has processed the request or that any bytes reached the kernel.

For direct `tracebox::Storage::write(record)`, steps 1 and 3 are omitted: the
call maps the public record to `LogRequest` and executes the storage path on the
caller’s thread. It returns the existing positive byte count, zero, or negative
error result.

For `tracebox::Recorder::record(data, timestamp)`, the client serializes a
`LogRequest`, prefixes it with a native-endian `uint32_t` payload length, and
calls `send()`. The service parses and queues it. There is no application-level
acknowledgement.

### File creation and day rollover

When a channel is first opened for a day:

1. The day directory is created if needed.
2. An `.idx` file is opened in append mode.
3. Its packed `LogFileHeader` is written and the C++ stream is flushed.
4. For variable-size records, a paired `.data` file is opened.
5. Its header is written and the C++ stream is flushed.
6. The variable data offset is initialized to the data header size.

When a request crosses a local calendar day, the filesystem backend replaces
the active `Folder`. Destruction of the old folder destroys its stream writers,
which flush and close their file handles before the new day’s writer is used.

Repeated opens at the same timestamp choose a suffix (`_1`, `_2`, ...), so an
existing file is not appended as a new logical stream by the rollover path.

### Fixed-size record writes

Fixed-size records are stored entirely in `.idx`:

```text
write uint64_t timestamp
write record payload
optional C++ stream flush
```

The payload length is checked before the two writes. A partial write can leave
a partial final fixed record in the index file.

### Variable-size record writes

Variable records use the current data-before-index ordering:

```text
.data: write uint64_t timestamp
.data: write uint32_t payload length
.data: write payload bytes
.data: optional C++ stream flush
fault-injection point
.idx:  write uint64_t timestamp
.idx:  write uint32_t data offset
.idx:  optional C++ stream flush
advance in-memory data offset
```

The index entry is intended to make the data record reachable only after the
data writes have completed. This prevents a normal injected power-loss point
from exposing an index entry for data that was never attempted. It does not
make the ordering durable across a power failure because neither stream uses
an explicit filesystem synchronization barrier.

### Retention deletion

Before a write, `Storage` compares `maxRecordSize()` with available bytes. If
there is not enough space, the backend repeatedly identifies the numerically
oldest day, sums its regular files, and calls `remove_all()` on that day tree.
The available-byte counter is increased by the measured bytes. Deletion is
synchronous and directory-granular. An interrupted deletion can leave a
partially removed day; there is no deletion journal or startup repair.

## Current durability guarantees

The implementation has several different completion points:

| Event | What is established | What is not established |
| --- | --- | --- |
| `Writer::add()` returns | Request copied into the in-process queue | Worker processing, file write, kernel page-cache acceptance, or persistence |
| `Recorder::record()` returns true | One framed message was accepted by the client `send()` call | Server parse, queue acceptance, storage write, or persistence |
| `Storage::write()` returns positive | File stream write calls reported success; some bytes may have been buffered | `fsync`, filesystem metadata persistence, device persistence, or power-fail safety |
| Periodic `flush()` completes | C++ stream buffers were flushed toward the OS | Filesystem metadata/device cache synchronization |
| `StreamWriter` closes normally | C++ streams were flushed and closed | Stable media persistence without an explicit sync primitive |
| `Writer` destruction completes | Worker shutdown path waited for the worker thread | If the shared `Storage` remains alive, its active file handles may remain open; no media sync |
| Normal process exit | C++ destructors may run if normal stack/static destruction occurs | Not applicable to abnormal termination; no explicit durability barrier |
| Graceful SIGTERM | No special Tracebox handler exists; default process behavior applies | Guaranteed writer drain or destructor execution |
| SIGKILL | Kernel terminates the process and closes descriptors as part of process death | C++ flush/destructor execution or ordered durable records |
| Kernel crash | Depends on OS/filesystem/page-cache recovery | Tracebox-level consistency or media persistence |
| Sudden power loss | No Tracebox power-fail guarantee | Complete records, synchronized metadata, or recoverable tails |

The relevant layers are:

1. **Userspace queue:** `Writer` may hold a request only in the C++ queue.
2. **C++ stream buffer:** `StandardFileIO::write()` writes through
   `std::ofstream`; without `flush`, bytes can remain in the stream buffer.
3. **Kernel page cache:** `flush()` sends the stream buffer to the kernel, but
   does not call `fsync()` or an equivalent data/metadata barrier.
4. **Filesystem metadata:** file length, directory entries, and renames/deletes
   are subject to filesystem journaling and mount policy, not Tracebox policy.
5. **Device cache/media:** SD/eMMC/controller/flash persistence is outside the
   current implementation.

Thus the current level is best described as **best-effort buffered append**. It
is not crash-consistent or power-fail-safe.

## Data/index consistency matrix

| State | How it can arise | Current detection | Reader behavior | Recovery today | Silent risk |
| --- | --- | --- | --- | --- | --- |
| Data record written, index missing | Power loss/process death after variable data write and before index write | Not directly; orphan bytes are not indexed | Index reports fewer records; data is ignored | None | Space leak, not normally a false record |
| Index entry written, data missing | Device/filesystem reordering or external deletion; not normal logical order | Missing data header gives header mismatch; missing tail gives read error | Stream rejected for header mismatch or record read fails | None | A valid-looking index can reference unavailable data |
| Partial data header | Interrupted data write | Bounded read/short read error | The affected read fails | Later indexed records may still be attempted | No checksum; damage boundary is not authenticated |
| Partial data payload | Truncation/torn write | Length exceeds remaining file bytes | Affected read fails; scanner can continue to later records | None | Previously this could be reported as success; now fails closed |
| Partial index entry | Interrupted index write | Record count uses complete-record floor | Trailing partial entry is ignored | None | Last record can disappear without a corruption status |
| Duplicate index entry | Repeated/replayed index write or manual corruption | No duplicate detection | Same data may be returned twice | None | Duplicate samples are silently accepted |
| Stale index offset | Old index paired with newer/changed data | Only offset/file bounds are checked | In-range stale offsets can read the wrong bytes | None | Silent misinterpretation is possible |
| Truncated data file | Manual truncation, retention interruption, power loss | Header and payload bounds/short reads | Affected record fails; stream may otherwise remain healthy | None | No automatic tail truncation or repair |
| Truncated index file | Manual truncation/power loss | Complete-record floor | Valid prefix is readable; partial tail is ignored; no explicit damaged status | None | Loss of final records can look like a normal shorter stream |
| Corrupted payload bytes | Torn write or media bit error | No checksum | Bytes are returned as if valid | None | Silent corruption is possible |
| Corrupted timestamp | Bit flip/manual edit | No timestamp checksum/range validation | Query ordering/range selection can be wrong | None | Records can be hidden or misordered |
| Corrupted length/offset | Bit flip/manual edit | Bounds checks reject many invalid values | Affected read fails; an in-range wrong value may misinterpret bytes | None | In-range corruption can be silent |

## Failure model

| Failure | Current behavior |
| --- | --- |
| Partial/torn write | Depends on which stream operation reached the OS; no atomic-record guarantee |
| Truncated file | Header/read checks may reject it; complete prefix can remain readable |
| Missing `.idx` | Stream discovery does not find the data; no index rebuild |
| Missing `.data` | Variable stream normally becomes `kHeadersDifferent` and is not readable |
| Corrupted index | Header/type/seek/read checks are partial; malformed tails may be ignored |
| Corrupted payload | No checksum; accepted bytes are returned |
| Directory partially created | Empty/incomplete day may be ignored; missing root can make construction fail |
| Retention deletion interrupted | Partial tree may remain; startup only recounts files, never repairs |
| Disk full | File operations can return errors; queue path does not report storage failures to producer |
| Permission failure | Directory/file operations fail and return negative/empty results depending on path |
| I/O error after a successful write call | No durable error is available unless the stream operation reports it immediately |

## Reader behavior and fail-open/closed choices

`FileReader` reads a fixed-size header only when `file_size()` is nonzero. A
short header resets the parsed header and makes the reader unavailable. Header
version, name, size, and file type are not comprehensively validated.

`IndexFileReader` derives record count by integer division of the bytes after the
header by the expected record width. Trailing bytes that do not complete a
record are ignored. It reads the first and last complete timestamps to cache a
range; it does not scan or checksum every entry.

`DataFileReader` now checks that the offset is inside the data file, that a full
data header exists, and that the declared payload fits in the remaining file.
Short reads set an error. `StreamReader` propagates those variable-data errors
instead of returning success. Invalid offsets and malformed lengths therefore
fail closed for the affected read.

The reader still fails open for some content corruption: an in-range offset,
timestamp, or payload can be wrong but structurally readable. There is no
checksum or cross-file transaction marker. One corrupt variable record causes
that record read to fail, while `StorageScanner::getData()` continues scanning
later index entries and returns an error string alongside any later records it
can read. A truncated final index record is treated as a shorter valid prefix,
not as a damaged stream.

## Current recovery behavior

Tracebox performs no startup recovery. `Storage` startup:

1. scans regular files under immediate day directories;
2. computes used bytes;
3. deletes oldest day directories if the configured budget is exceeded;
4. starts normal writing.

It does not truncate tails, rebuild indexes, validate all records, compare
index/data timestamps, verify checksums, quarantine damaged segments, or report
a recovery result.

### Can an index be rebuilt from data?

Not by the current implementation. For a variable-size stream, the `.data`
file contains enough sequential information in principle—timestamps, lengths,
and record boundaries—to reconstruct offsets and index timestamps. A missing or
corrupt index would still require policy for naming, duplicate handling,
partial-tail treatment, and replacement safety. For fixed-size streams, the
payload is stored in the index itself, so a missing index cannot be rebuilt
from a separate data file.

### Are partial tails recoverable?

They are readable only as a valid prefix when the index tail is incomplete. A
partial variable data tail referenced by an index entry is now detectable during
read, but it is not truncated or repaired. Orphan data written without an index
is not discoverable by the reader and is not reclaimed automatically.

## Candidate recovery strategies

These are alternatives for a future design, not decisions in PR-4:

| Strategy | Benefit | Cost/risk |
| --- | --- | --- |
| Ignore invalid final record | Simple startup and preserves valid prefix | Does not repair space or expose damage clearly |
| Truncate incomplete tail | Reclaims obvious partial bytes | Requires trusted record-boundary rules and safe metadata update |
| Rebuild variable index from data | Can recover a missing variable index | Cannot recover fixed indexes; needs replacement/duplicate policy |
| Mark damaged segment and continue | Makes corruption operationally visible | Requires status/catalog semantics and client behavior |
| Checksums per record/segment | Detects silent payload/metadata corruption | Format change, CPU/storage overhead, migration policy |
| Generation/commit markers | Makes completed segments identifiable | Format and rollover protocol change |
| Journal or copy-on-write metadata | Stronger crash consistency | Write amplification, recovery time, and embedded storage design |

No strategy should be selected solely because it is easy to implement. The
required guarantee, recovery-time bound, available RAM, and media behavior must
be decided first.

## Embedded implications

### Linux filesystems

The current design assumes seekable files, directory creation/enumeration,
append mode, file sizes, and a filesystem that gives useful ordering for normal
close/flush behavior. `fsync()`/`fdatasync()` are available in principle but are
not used. Filesystem journaling and mount options determine additional behavior.

### SD and eMMC

Controllers and flash translation layers hide erase/write granularity and may
have volatile caches. A C++ flush is not a power-fail guarantee. Sudden removal
can affect both data and directory/index metadata. Retention deletion can also
create high metadata and erase traffic.

### Raw NOR and SPI/QSPI flash

Raw flash normally requires erase-before-program constraints, page/program
granularity, wear management, and power-fail-safe metadata updates. The current
path assumes filesystem directories, filenames, seekable streams, and
`remove_all()`, none of which map directly to a raw device. NOR’s relatively
small program units do not make multi-field record updates atomic.

### Raw NAND flash

Bad blocks, larger pages, ECC, out-of-band metadata, wear leveling, and
log-structured allocation are device concerns. The current filesystem format
does not define how to recover from torn pages or bad-block remapping.

### RTOS block devices

An RTOS may provide neither POSIX `fsync()` nor the same filesystem atomicity,
threading, directory, or signal semantics. Bounded recovery time and RAM use
would need explicit limits; the current unbounded queue and query responses are
not sufficient for a deterministic embedded profile.

## Candidate target guarantees

The following levels are useful design vocabulary and are not currently exposed
by the public API:

1. **Buffered:** the record has entered a userspace queue or stream buffer.
2. **Flushed:** userspace stream buffers have been sent to the OS.
3. **Filesystem-synchronized:** data and required metadata have passed an
   explicit filesystem synchronization operation.
4. **Crash-consistent:** after process or kernel failure, every acknowledged
   record is either fully readable or explicitly classified as incomplete; no
   valid index points to unavailable data.
5. **Power-fail-safe:** the crash-consistency guarantee holds across the target
   device’s power-loss model, including controller cache and write/erase
   granularity.

Tradeoffs increase from level 1 to level 5: latency, write amplification,
metadata overhead, recovery work, and device-specific implementation all grow.
The current implementation reaches level 1 for asynchronous enqueue and
best-effort level 2 at selected flush/close points. It does not claim levels 3,
4, or 5.

## Open design questions

* Which event should a future acknowledgement represent?
* Is process-crash consistency sufficient, or is sudden power loss a product
  requirement?
* Which filesystems and flash controllers are in scope?
* What is the maximum acceptable recovery time and RAM usage?
* Should a damaged stream be skipped, quarantined, truncated, or exposed with
  a status/error response?
* Should variable data be authoritative for index rebuilding?
* What checksum coverage and migration mechanism are acceptable?
* How should retention account for orphan data and damaged segments?
* Does the future public API need an explicit durability result, or can a
  separate policy/configuration layer report it?
