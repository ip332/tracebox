#pragma once

#include "logger.pb.h"

// A callback to store a single data record into the output stream.
using data_writer_t = std::function<void(const std::vector<embark::logger::DataPiece> & data)>;

// This container holds data pieces from different streams which belongs to the same second.
class OneSecondData {
    // The second this container represents.
    uint64_t second_;
    // Actual data storage
    std::vector<embark::logger::DataPiece> data_;
    // Keep sorting status
    bool sorted_ = false;

public:
    explicit OneSecondData(uint64_t second) : second_(second) {}

    // Moves the given data into the vector
    void add(const std::vector<embark::logger::DataPiece> &data) {
        sorted_ = false;
        data_.insert(data_.end(), data.begin(), data.end());
    }

    // Sorts the container by time_ns
    void sort() {
        if (sorted_) {
            return;
        }
        std::sort(data_.begin(),
                  data_.end(),
                  [](const embark::logger::DataPiece& a, const embark::logger::DataPiece& b){
                      return a.time_ns() < b.time_ns();
                  });
        sorted_ = true;
    }

    size_t save(data_writer_t writer) const;
};

// This class combines several input streams into a single stream of DataPieces
// ordered by their timestamps.
class StreamsCombiner {
    // Data organized by one-second chunks ordered by the actual seconds.
    std::map<uint64_t, OneSecondData> seconds_;
    // First and last seconds we are looking for.
    uint64_t first_second_;
    uint64_t last_second_;
    // Maps the channel name into the channel index
    std::map<std::string, int> indices_;

    // Find the appropriate second to add the accumulated records.
    void storeData(uint64_t second, const std::vector<embark::logger::DataPiece> &data);

public:
    // Request serv
    StreamsCombiner(uint64_t start_time, uint64_t end_time)
        : first_second_(start_time / 1E9), last_second_(end_time / 1E9) {}

    // Adds a data stream to the storage
    void addRecords(const std::string & stream,
                    const google::protobuf::RepeatedPtrField<embark::logger::DataPiece> & data);

    // Returns mapping between the name and the index.
    const std::map<std::string, int> & streams() const { return indices_; }

    // Outputs accumulated data using provided writer and returns number of data records
    size_t save(data_writer_t writer) const;
};