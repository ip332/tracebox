#include "storage_scanner.h"

#include <filesystem>

#include "data_types.h"
#include "stream_reader.h"

namespace embark {
namespace logger {

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
                    extractStreams(entry.path(), start, end, result);
                }
            } catch (...) {
            }
        }
    }
    return result;
}

void StorageScanner::extractStreams(
    const std::filesystem::path& path, uint64_t start, uint64_t end,
    std::shared_ptr<DataStreamsResponse> result) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_directory() && entry.path().extension() == ".idx") {
            // Create a temporary object
            StreamReader reader(entry);
            if (reader.matchTime(start, end)) {
                auto stream = result->add_stream();
                stream->set_name(reader.streamName());
                stream->set_status(reader.status());
                stream->set_file(entry.path());
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
    StreamReader reader(file);
    if (!reader.matchTime(start_time, end_time)) {
        return result;
    }
    if (max_size > (reader.records_cnt() - start_idx)) {
        max_size = reader.records_cnt() - start_idx;
    }
    for (size_t idx = start_idx; idx < max_size; idx++) {
        DataPiece data;
        if (!reader.read(idx, &data)) {
            result->set_errors(reader.last_error());
        } else {
            auto data_piece = result->add_data();
            data_piece->CopyFrom(data);
        }
    }
    return result;
}

}  // namespace logger
}  // namespace embark