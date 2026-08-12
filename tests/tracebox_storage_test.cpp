#include <gtest/gtest.h>

#include <chrono>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "data_file_reader.h"
#include "file_io.h"
#include "folder.h"
#include "idx_file_reader.h"
#include "storage.h"
#include "storage_backend.h"
#include "storage_scanner.h"
#include "stream_reader.h"
#include "stream_writer.h"

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() /
                ("tracebox_test_" + suffix);
        if (!std::filesystem::create_directories(path_)) {
            throw std::runtime_error("failed to create test directory");
        }
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    const std::filesystem::path &path() const { return path_; }

   private:
    std::filesystem::path path_;
};

tracebox::logger::LogRequest request(uint64_t time_ns, uint32_t size,
                                   std::string data) {
    tracebox::logger::LogRequest result;
    result.set_channel("sensor");
    result.set_size(size);
    result.set_time_ns(time_ns);
    result.set_data(std::move(data));
    return result;
}

std::filesystem::path onlyIndexFile(const std::filesystem::path &dir) {
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".idx") {
            return entry.path();
        }
    }
    return {};
}

std::filesystem::path onlyDataFile(const std::filesystem::path &dir) {
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".data") {
            return entry.path();
        }
    }
    return {};
}

std::filesystem::path timestampedDay(uint64_t timestamp) {
    return std::to_string(tracebox::logger::Time2YYYYMMDD(timestamp));
}

void writeHeaderOnly(const std::filesystem::path &path, uint8_t type,
                     uint32_t record_size = 0) {
    tracebox::logger::LogFileHeader header;
    header.header_size_ = sizeof(header);
    header.record_size_ = record_size;
    header.file_type_ = type;
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(&header), sizeof(header));
}

struct MockFileState {
    bool fail_open = false;
    bool fail_flush = false;
    int fail_after_writes = -1;
    int writes = 0;
};

class MockFileIO final : public tracebox::logger::FileIO {
   public:
    explicit MockFileIO(std::shared_ptr<MockFileState> state)
        : state_(std::move(state)) {}

    bool open(const std::filesystem::path &path, std::ios::openmode) override {
        path_ = path;
        open_ = !state_->fail_open;
        return open_;
    }

    bool isOpen() const override { return open_; }

    bool write(const char *, std::size_t) override {
        if (state_->fail_after_writes >= 0 &&
            state_->writes++ >= state_->fail_after_writes) {
            return false;
        }
        return open_;
    }

    bool flush() override { return open_ && !state_->fail_flush; }

    void close() override { open_ = false; }

   private:
    std::shared_ptr<MockFileState> state_;
    std::filesystem::path path_;
    bool open_ = false;
};

tracebox::logger::StreamWriter::FileIOFactory mockFileFactory(
    const std::shared_ptr<MockFileState> &state) {
    return [state] { return std::make_unique<MockFileIO>(state); };
}

}  // namespace

TEST(StreamWriterTest, PowerLossBeforeIndexCommitLeavesRecordUnindexed) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;

    auto writer = std::make_unique<StreamWriter>(dir.path());
    auto log_request = request(timestamp, 0, "payload");
    ASSERT_GT(writer->openFile(log_request), 0);
    writer->setFaultInjector([](StreamWriter::WriteFaultPoint point) {
        ASSERT_EQ(point,
                  StreamWriter::WriteFaultPoint::kAfterDataWriteBeforeIndexWrite);
        throw StreamWriter::SimulatedPowerLoss();
    });

    EXPECT_THROW(writer->write(timestamp, "payload"),
                 StreamWriter::SimulatedPowerLoss);

    writer->setFaultInjector({});
    writer.reset();

    const auto index_path = onlyIndexFile(dir.path());
    ASSERT_FALSE(index_path.empty());
    IndexFileReader index(index_path);
    EXPECT_EQ(index.records_cnt(), 0U);

    const auto data_path = index_path.parent_path() /
                           (index_path.stem().string() + ".data");
    ASSERT_TRUE(std::filesystem::exists(data_path));
    EXPECT_GT(std::filesystem::file_size(data_path), sizeof(LogFileHeader));
}

TEST(StreamWriterTest, ReopeningAtSameTimestampCreatesNewFiles) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    auto log_request = request(timestamp, 4, "abcd");

    {
        StreamWriter writer(dir.path());
        ASSERT_GT(writer.openFile(log_request), 0);
        ASSERT_GT(writer.write(timestamp, "abcd"), 0);
    }
    {
        StreamWriter writer(dir.path());
        ASSERT_GT(writer.openFile(log_request), 0);
        ASSERT_GT(writer.write(timestamp + 1, "efgh"), 0);
    }

    size_t index_count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(dir.path())) {
        if (entry.path().extension() == ".idx") {
            ++index_count;
            IndexFileReader index(entry.path());
            EXPECT_EQ(index.records_cnt(), 1U);
        }
    }
    EXPECT_EQ(index_count, 2U);
}

TEST(StreamWriterTest, RejectsWritesBeforeOpenAndWrongFixedPayloads) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    StreamWriter writer(dir.path());

    EXPECT_EQ(writer.write(1, "x"), -EINVAL);
    auto log_request = request(1704067200000000000ULL, 2, "ab");
    ASSERT_GT(writer.openFile(log_request), 0);
    EXPECT_EQ(writer.write(log_request.time_ns(), "x"), -EINVAL);
    EXPECT_EQ(writer.write(log_request.time_ns(), "ab"),
              static_cast<int>(sizeof(uint64_t) + 2));
}

TEST(StreamWriterTest, WritesAndReadsFixedLengthRecords) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    {
        StreamWriter writer(dir.path());
        ASSERT_GT(writer.openFile(request(timestamp, 3, "abc")), 0);
        ASSERT_GT(writer.write(timestamp, "abc"), 0);
    }
    IndexFileReader index(onlyIndexFile(dir.path()));
    ASSERT_EQ(index.records_cnt(), 1U);
    std::string payload;
    EXPECT_EQ(index.readPayload(0, &payload), 3U);
    EXPECT_EQ(payload, "abc");
    EXPECT_EQ(index.readOffset(0), 0U);
    EXPECT_EQ(index.readPayload(0, nullptr), 0U);
}

TEST(StreamWriterTest, PropagatesFileOpenAndHeaderWriteFailures) {
    using namespace tracebox::logger;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    TemporaryDirectory dir;

    auto open_state = std::make_shared<MockFileState>();
    open_state->fail_open = true;
    StreamWriter open_failure(dir.path(), mockFileFactory(open_state));
    EXPECT_EQ(open_failure.openFile(request(timestamp, 3, "abc")), -EINVAL);

    auto write_state = std::make_shared<MockFileState>();
    write_state->fail_after_writes = 0;
    StreamWriter header_failure(dir.path(), mockFileFactory(write_state));
    EXPECT_EQ(header_failure.openFile(request(timestamp, 3, "abc")), -EIO);
}

TEST(StreamWriterTest, PropagatesRecordWriteAndFlushFailures) {
    using namespace tracebox::logger;
    constexpr uint64_t timestamp = 1704067200000000000ULL;

    auto write_state = std::make_shared<MockFileState>();
    TemporaryDirectory write_dir;
    StreamWriter writer(write_dir.path(), mockFileFactory(write_state));
    ASSERT_GT(writer.openFile(request(timestamp, 3, "abc")), 0);
    write_state->fail_after_writes = write_state->writes;
    EXPECT_EQ(writer.write(timestamp, "abc"), -EIO);

    auto flush_state = std::make_shared<MockFileState>();
    flush_state->fail_flush = true;
    TemporaryDirectory flush_dir;
    StreamWriter flush_failure(flush_dir.path(), mockFileFactory(flush_state));
    EXPECT_EQ(flush_failure.openFile(request(timestamp, 3, "abc")), -EIO);
}

TEST(StorageScannerTest, PaginationAndOverlappingTimeRangeAreCorrect) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t first = 1704067200000000000ULL;
    const auto day_dir = dir.path() / std::to_string(Time2YYYYMMDD(first));
    ASSERT_TRUE(std::filesystem::create_directories(day_dir));

    {
        StreamWriter writer(day_dir);
        auto log_request = request(first, 1, "a");
        ASSERT_GT(writer.openFile(log_request), 0);
        ASSERT_GT(writer.write(first, "a"), 0);
        ASSERT_GT(writer.write(first + 1, "b"), 0);
        ASSERT_GT(writer.write(first + 2, "c"), 0);
    }

    const auto index_path = onlyIndexFile(day_dir);
    ASSERT_FALSE(index_path.empty());
    StorageScanner scanner(dir.path().string());

    auto streams = scanner.getStreams(first + 1, first + 1);
    ASSERT_EQ(streams->stream_size(), 1);
    EXPECT_EQ(streams->stream(0).records_cnt(), 3U);

    EXPECT_FALSE(std::filesystem::path(streams->stream(0).file()).is_absolute());
    auto data = scanner.getData(streams->stream(0).file(), first, first + 2, 1, 2);
    ASSERT_EQ(data->errors(), "");
    ASSERT_EQ(data->data_size(), 2);
    EXPECT_EQ(data->data(0).time_ns(), first + 1);
    EXPECT_EQ(data->data(1).time_ns(), first + 2);
}

TEST(FolderTest, CreatesFolderAndReusesAnExistingChannel) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    auto first = request(timestamp, 3, "abc");
    {
        Folder folder(dir.path().string());
        ASSERT_GT(folder.write(first), 0);
        auto second = request(timestamp + 1, 3, "def");
        ASSERT_GT(folder.write(second), 0);
    }

    const auto day_dir = dir.path() / timestampedDay(timestamp);
    EXPECT_TRUE(std::filesystem::is_directory(day_dir));
    auto index = onlyIndexFile(day_dir);
    ASSERT_FALSE(index.empty());
    IndexFileReader reader(index);
    EXPECT_EQ(reader.records_cnt(), 2U);
}

TEST(FolderTest, RejectsARecordWithTheWrongFixedSize) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    Folder folder(dir.path().string());
    ASSERT_GT(folder.write(request(timestamp, 3, "abc")), 0);
    EXPECT_EQ(folder.write(request(timestamp + 1, 3, "x")), -EINVAL);
}

TEST(StorageTest, WritesAcrossDaysAndEvictsTheOldestDay) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t first = 1704067200000000000ULL;
    constexpr uint64_t second = first + 3 * 24 * 60 * 60 * 1000000000ULL;

    Storage storage(dir.path().string(), maxRecordSize(1));
    ASSERT_GT(storage.write(request(first, 1, "a")), 0);
    ASSERT_GT(storage.write(request(second, 1, "b")), 0);

    EXPECT_FALSE(std::filesystem::exists(dir.path() / timestampedDay(first)));
    EXPECT_TRUE(std::filesystem::exists(dir.path() / timestampedDay(second)));
}

TEST(StorageTest, RejectsWritesWhenNoFolderCanBeRemoved) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    Storage storage(dir.path().string(), 1);
    EXPECT_EQ(storage.write(request(timestamp, 1, "a")), 0);
}

TEST(StorageBackendTest, FilesystemBackendOwnsDayAndRecordPersistence) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;

    FilesystemStorageBackend backend(dir.path().string());
    EXPECT_EQ(backend.usedBytes(), 0U);
    ASSERT_GT(backend.write(request(timestamp, 1, "a")), 0);
    EXPECT_TRUE(std::filesystem::exists(dir.path() / timestampedDay(timestamp)));
    EXPECT_GT(backend.usedBytes(), 0U);
}

TEST(DataFileReaderTest, ReadsValidDataAndReportsMalformedFiles) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;

    {
        StreamWriter writer(dir.path());
        ASSERT_GT(writer.openFile(request(timestamp, 0, "payload")), 0);
        ASSERT_GT(writer.write(timestamp, "payload"), 0);
    }
    const auto data_path = onlyDataFile(dir.path());
    ASSERT_FALSE(data_path.empty());

    DataFileReader reader(data_path);
    DataPiece piece;
    ASSERT_EQ(reader.read(sizeof(LogFileHeader), &piece), 7U);
    EXPECT_EQ(piece.time_ns(), timestamp);
    EXPECT_EQ(piece.data(), "payload");
    EXPECT_EQ(reader.read(0, &piece), 0U);

    DataFileReader missing(dir.path() / "missing.data");
    EXPECT_EQ(missing.read(sizeof(LogFileHeader), &piece), 0U);
    EXPECT_FALSE(missing.last_error().empty());

    const auto truncated = dir.path() / "truncated.data";
    writeHeaderOnly(truncated, kDataFile);
    DataFileReader truncated_reader(truncated);
    EXPECT_EQ(truncated_reader.read(sizeof(LogFileHeader), &piece), 0U);

    const auto wrong_type = dir.path() / "wrong_type.data";
    writeHeaderOnly(wrong_type, kIndexFile);
    DataFileReader wrong_type_reader(wrong_type);
    EXPECT_EQ(wrong_type_reader.read(sizeof(LogFileHeader), &piece), 0U);

    const auto truncated_record = dir.path() / "truncated_record.data";
    {
        std::ofstream output(truncated_record, std::ios::binary);
        LogFileHeader header;
        header.header_size_ = sizeof(header);
        header.file_type_ = kDataFile;
        output.write(reinterpret_cast<const char *>(&header), sizeof(header));
        VariableRecordDataHeader record;
        record.size_ = 10;
        output.write(reinterpret_cast<const char *>(&record), sizeof(record));
        output.put('x');
    }
    DataFileReader truncated_record_reader(truncated_record);
    EXPECT_EQ(truncated_record_reader.read(sizeof(LogFileHeader), &piece), 0U);
}

TEST(DataFileReaderTest, RejectsARecordWhosePayloadExceedsTheFile) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    const auto path = dir.path() / "short_payload.data";
    {
        std::ofstream output(path, std::ios::binary);
        LogFileHeader header;
        header.header_size_ = sizeof(header);
        header.file_type_ = kDataFile;
        output.write(reinterpret_cast<const char *>(&header), sizeof(header));
        VariableRecordDataHeader record;
        record.time_ns_ = timestamp;
        record.size_ = 16;
        output.write(reinterpret_cast<const char *>(&record), sizeof(record));
        output.write("x", 1);
    }

    DataFileReader reader(path);
    DataPiece piece;
    EXPECT_EQ(reader.read(sizeof(LogFileHeader), &piece), 0U);
    EXPECT_FALSE(reader.last_error().empty());
}

TEST(IndexFileReaderTest, ReportsInvalidReadsAndHandlesMalformedHeaders) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    const auto empty = dir.path() / "empty.idx";
    std::ofstream(empty).close();
    IndexFileReader empty_reader(empty);
    EXPECT_EQ(empty_reader.records_cnt(), 0U);
    EXPECT_EQ(empty_reader.timeOf(0), 0U);
    EXPECT_EQ(empty_reader.readOffset(0), 0U);

    const auto fixed_header = dir.path() / "fixed.idx";
    writeHeaderOnly(fixed_header, kIndexFile, 3);
    IndexFileReader fixed(fixed_header);
    EXPECT_EQ(fixed.readOffset(0), 0U);
    EXPECT_EQ(fixed.readPayload(0, nullptr), 0U);

    const auto variable_header = dir.path() / "variable.idx";
    writeHeaderOnly(variable_header, kIndexFile);
    IndexFileReader variable(variable_header);
    EXPECT_EQ(variable.readPayload(0, nullptr), 0U);
}

TEST(StreamReaderTest, HandlesEmptyInvalidAndMismatchedStreams) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;

    const auto not_index = dir.path() / "not-an-index.txt";
    std::ofstream(not_index) << "not an index";
    StreamReader invalid(not_index);
    EXPECT_EQ(invalid.status(), StreamStatus::kUnknown);
    EXPECT_FALSE(invalid.read(0, nullptr));

    const auto empty_dir = dir.path() / "empty";
    ASSERT_TRUE(std::filesystem::create_directories(empty_dir));
    {
        StreamWriter writer(empty_dir);
        ASSERT_GT(writer.openFile(request(timestamp, 3, "abc")), 0);
    }
    const auto empty_index = onlyIndexFile(empty_dir);
    StreamReader empty(empty_index);
    EXPECT_EQ(empty.status(), StreamStatus::kEmpty);
    EXPECT_FALSE(empty.read(0, nullptr));

    const auto variable_dir = dir.path() / "variable";
    ASSERT_TRUE(std::filesystem::create_directories(variable_dir));
    {
        StreamWriter writer(variable_dir);
        ASSERT_GT(writer.openFile(request(timestamp, 0, "payload")), 0);
        ASSERT_GT(writer.write(timestamp, "payload"), 0);
    }
    const auto variable_index = onlyIndexFile(variable_dir);
    const auto variable_data = onlyDataFile(variable_dir);
    ASSERT_FALSE(variable_index.empty());
    ASSERT_FALSE(variable_data.empty());
    StreamReader variable(variable_index);
    DataPiece piece;
    ASSERT_TRUE(variable.read(0, &piece));
    EXPECT_EQ(piece.data(), "payload");

    const auto fixed_dir = dir.path() / "fixed";
    ASSERT_TRUE(std::filesystem::create_directories(fixed_dir));
    {
        StreamWriter writer(fixed_dir);
        ASSERT_GT(writer.openFile(request(timestamp, 3, "abc")), 0);
        ASSERT_GT(writer.write(timestamp, "abc"), 0);
    }
    StreamReader fixed(onlyIndexFile(fixed_dir));
    ASSERT_EQ(fixed.status(), StreamStatus::kHealthy);
    ASSERT_TRUE(fixed.read(0, &piece));
    EXPECT_EQ(piece.time_ns(), timestamp);
    EXPECT_EQ(piece.data(), "abc");

    std::filesystem::remove(variable_data);
    StreamReader missing_data(variable_index);
    EXPECT_EQ(missing_data.status(), StreamStatus::kHeadersDifferent);
    EXPECT_FALSE(missing_data.read(0, &piece));
}

TEST(StreamReaderTest, RejectsTruncatedVariableDataAndInvalidOffsets) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    const auto variable_dir = dir.path() / "variable";
    ASSERT_TRUE(std::filesystem::create_directories(variable_dir));
    {
        StreamWriter writer(variable_dir);
        ASSERT_GT(writer.openFile(request(timestamp, 0, "payload")), 0);
        ASSERT_GT(writer.write(timestamp, "payload"), 0);
    }

    const auto index_path = onlyIndexFile(variable_dir);
    const auto data_path = onlyDataFile(variable_dir);
    ASSERT_FALSE(index_path.empty());
    ASSERT_FALSE(data_path.empty());

    std::filesystem::resize_file(
        data_path, sizeof(LogFileHeader) + sizeof(VariableRecordDataHeader) + 1);
    StreamReader truncated(index_path);
    DataPiece piece;
    EXPECT_EQ(truncated.status(), StreamStatus::kHealthy);
    EXPECT_FALSE(truncated.read(0, &piece));
    EXPECT_FALSE(truncated.last_error().empty());

    {
        std::fstream index(index_path,
                           std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(index.is_open());
        const auto offset_position = sizeof(LogFileHeader) + sizeof(uint64_t);
        index.seekp(offset_position);
        const uint32_t invalid_offset = UINT32_MAX;
        index.write(reinterpret_cast<const char *>(&invalid_offset),
                    sizeof(invalid_offset));
    }
    StreamReader invalid_offset(index_path);
    EXPECT_EQ(invalid_offset.status(), StreamStatus::kHealthy);
    EXPECT_FALSE(invalid_offset.read(0, &piece));
    EXPECT_FALSE(invalid_offset.last_error().empty());
}

TEST(StreamReaderTest, TreatsAPartialFinalIndexRecordAsAnEmptyStream) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    {
        StreamWriter writer(dir.path());
        ASSERT_GT(writer.openFile(request(timestamp, 3, "abc")), 0);
        ASSERT_GT(writer.write(timestamp, "abc"), 0);
    }

    const auto index_path = onlyIndexFile(dir.path());
    ASSERT_FALSE(index_path.empty());
    const auto complete_size = std::filesystem::file_size(index_path);
    ASSERT_GT(complete_size, sizeof(LogFileHeader) + sizeof(FixedRecordIdx));
    std::filesystem::resize_file(index_path, complete_size - 1);

    StreamReader reader(index_path);
    EXPECT_EQ(reader.status(), StreamStatus::kEmpty);
    EXPECT_FALSE(reader.read(0, nullptr));
}

TEST(StorageScannerTest, IgnoresInvalidDirectoriesAndRejectsInvalidQueries) {
    using namespace tracebox::logger;
    TemporaryDirectory dir;
    ASSERT_TRUE(std::filesystem::create_directories(dir.path() / "not-a-day"));
    std::ofstream(dir.path() / "not-a-day" / "ignored.txt").close();
    StorageScanner scanner(dir.path().string());
    auto streams = scanner.getStreams(2, 1);
    EXPECT_EQ(streams->stream_size(), 0);
    auto missing = scanner.getData("missing.idx", 0, 1, 0, 1);
    EXPECT_EQ(missing->data_size(), 0);
    EXPECT_EQ(missing->errors(), "invalid reader path");
}

TEST(StorageScannerTest, ConfinesReaderPathsToDiscoveredIndexFiles) {
    using namespace tracebox::logger;
    TemporaryDirectory root;
    TemporaryDirectory outside;
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    const auto day = root.path() / timestampedDay(timestamp);
    ASSERT_TRUE(std::filesystem::create_directories(day));

    {
        StreamWriter writer(day);
        ASSERT_GT(writer.openFile(request(timestamp, 3, "abc")), 0);
        ASSERT_GT(writer.write(timestamp, "abc"), 0);
    }
    const auto index = onlyIndexFile(day);
    ASSERT_FALSE(index.empty());
    const auto data = day / (index.stem().string() + ".data");
    std::ofstream(data).put('x');
    ASSERT_TRUE(std::filesystem::is_regular_file(data));

    const auto outside_index = outside.path() / "outside.idx";
    writeHeaderOnly(outside_index, kIndexFile, 3);
    const auto outside_parent = outside.path() / "parent";
    ASSERT_TRUE(std::filesystem::create_directories(outside_parent));
    const auto parent_index = outside_parent / "parent.idx";
    writeHeaderOnly(parent_index, kIndexFile, 3);
    const auto prefix_sibling = root.path().string() + "_sibling";
    ASSERT_TRUE(std::filesystem::create_directories(prefix_sibling));
    writeHeaderOnly(std::filesystem::path(prefix_sibling) / "sibling.idx",
                    kIndexFile, 3);

    std::filesystem::create_symlink(outside_index, root.path() / "link.idx");
    std::filesystem::create_symlink(outside_parent,
                                    root.path() / "linked-parent");

    StorageScanner scanner(root.path().string());
    const auto streams = scanner.getStreams(timestamp, timestamp);
    ASSERT_EQ(streams->stream_size(), 1);
    const std::string legitimate = streams->stream(0).file();
    EXPECT_EQ(scanner.getData(legitimate, timestamp, timestamp, 0, 1)
                  ->errors(),
              "");

    const std::vector<std::string> rejected = {
        outside_index.string(),
        "../outside.idx",
        timestampedDay(timestamp).string() + "/../" +
            std::filesystem::path(legitimate).filename().string(),
        "missing.idx",
        timestampedDay(timestamp),
        "not-an-index.txt",
        std::filesystem::path(legitimate).replace_extension(".data").string(),
        "link.idx",
        "linked-parent/parent.idx",
        (std::filesystem::path(prefix_sibling) / "sibling.idx").string(),
        "",
        "bad\\path.idx",
    };

    std::ofstream(root.path() / "not-an-index.txt").close();
    for (const auto& path : rejected) {
        const auto response = scanner.getData(path, timestamp, timestamp, 0, 1);
        EXPECT_EQ(response->errors(), "invalid reader path") << path;
        EXPECT_EQ(response->data_size(), 0) << path;
    }

    const std::string embedded_nul("bad\0.idx", 8);
    const auto malformed = scanner.getData(embedded_nul, timestamp, timestamp,
                                           0, 1);
    EXPECT_EQ(malformed->errors(), "invalid reader path");
    EXPECT_EQ(malformed->data_size(), 0);
    std::filesystem::remove_all(prefix_sibling);
}
