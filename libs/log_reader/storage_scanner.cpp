#include "storage_scanner.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "data_types.h"
#include "stream_reader.h"

namespace tracebox {
namespace logger {

namespace {

bool isWithinRoot(const std::filesystem::path& root,
                 const std::filesystem::path& candidate) {
    auto root_it = root.begin();
    auto candidate_it = candidate.begin();
    for (; root_it != root.end() && candidate_it != candidate.end();
         ++root_it, ++candidate_it) {
        if (*root_it != *candidate_it) {
            return false;
        }
    }
    return root_it == root.end();
}

bool hasRejectedComponent(const std::filesystem::path& path) {
    for (const auto& component : path) {
        if (component == "..") {
            return true;
        }
    }
    return false;
}

}  // namespace

std::optional<std::filesystem::path> StorageScanner::resolveIndexPath(
    const std::string& file) const {
    // The discovery field is a relative identifier, not a filesystem path.
    // Reject both POSIX and Windows-style separators so the same policy holds
    // when requests are produced on another platform.
    if (file.empty() || file.find('\0') != std::string::npos ||
        file.find('\\') != std::string::npos) {
        return std::nullopt;
    }

    const std::filesystem::path requested(file);
    if (requested.empty() || requested.is_absolute() ||
        requested.has_root_name() || requested.has_root_directory() ||
        hasRejectedComponent(requested) || requested.extension() != ".idx") {
        return std::nullopt;
    }

    std::error_code error;
    const auto candidate = std::filesystem::weakly_canonical(
        folder_ / requested, error);
    if (error || !isWithinRoot(folder_, candidate) ||
        !std::filesystem::is_regular_file(candidate, error) || error) {
        return std::nullopt;
    }

    IndexFileReader index(candidate);
    if (!index.validIndex()) {
        return std::nullopt;
    }
    return candidate;
}

std::string StorageScanner::streamIdentifier(
    const std::filesystem::path& path) const {
    return std::filesystem::relative(path, folder_).generic_string();
}

std::shared_ptr<DataStreamsResponse> StorageScanner::getStreams(uint64_t start,
                                                                uint64_t end) {
    auto result = std::make_shared<DataStreamsResponse>();

    auto start_day = Time2YYYYMMDD(start);
    auto end_day = Time2YYYYMMDD(end);

    // Iterate through all subdirectories in the storage folder and process
    // every one which match the day range
    for (const auto& entry : std::filesystem::directory_iterator(folder_)) {
        if (entry.is_directory()) {
            try {
                auto path = entry.path().filename().string();
                auto day = std::stoul(path);
                if (day >= start_day && day <= end_day) {
                    extractStreams(entry.path(), start, end, day, result);
                }
            } catch (...) {
            }
        }
    }
    return result;
}

void StorageScanner::extractStreams(
    const std::filesystem::path& path, uint64_t start, uint64_t end, uint32_t day,
    std::shared_ptr<DataStreamsResponse> result) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_directory() && entry.path().extension() == ".idx") {
            const auto identifier = streamIdentifier(entry.path());
            const auto index_path = resolveIndexPath(identifier);
            if (!index_path) {
                continue;
            }
            StreamReader reader(*index_path);
            if (reader.matchTime(start, end)) {
                auto stream = result->add_stream();
                stream->set_day(day);
                stream->set_name(reader.streamName());
                stream->set_status(reader.status());
                stream->set_file(identifier);
                stream->set_records_cnt(reader.records_cnt());
                stream->set_start_time_ns(reader.start_ns());
                stream->set_end_time_ns(reader.end_ns());
            }
        }
    }
}

std::shared_ptr<DataStreamsResponse> StorageScanner::getData(
    const std::string& file, uint64_t start_time, uint64_t end_time,
    uint32_t start_idx, uint32_t max_size) {
    auto result = std::make_shared<DataStreamsResponse>();
    const auto index_path = resolveIndexPath(file);
    if (!index_path) {
        result->set_errors("invalid reader path");
        return result;
    }
    StreamReader reader(*index_path);
    if (!reader.matchTime(start_time, end_time)) {
        return result;
    }
    if (start_idx >= reader.records_cnt()) {
        return result;
    }
    uint32_t returned = 0;
    for (size_t idx = start_idx; idx < reader.records_cnt() &&
                                  returned < max_size; idx++) {
        DataPiece data;
        if (!reader.read(idx, &data)) {
            result->set_errors(reader.last_error());
        } else if (data.time_ns() >= start_time &&
                   data.time_ns() <= end_time) {
            auto data_piece = result->add_data();
            data_piece->CopyFrom(data);
            ++returned;
        }
    }
    return result;
}

}  // namespace logger
}  // namespace tracebox
