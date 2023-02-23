#include "streams_combiner.h"

size_t OneSecondData::save(data_writer_t writer) const {
    writer(data_);
    return data_.size();
}

void StreamsCombiner::storeData(uint64_t second, const std::vector<embark::logger::DataPiece> &data) {
    if (second == 0 || data.empty()) {
        return;
    }
    auto it = seconds_.find(second);
    if (it == seconds_.end()) {
        seconds_.emplace(second, OneSecondData(second));
        it = seconds_.find(second);
    }
    it->second.add(data);
}

void StreamsCombiner::addRecords(const std::string &stream,
                                 const google::protobuf::RepeatedPtrField<embark::logger::DataPiece> &data) {
    auto stream_index = indices_.find(stream);
    if (stream_index == indices_.end()) {
        indices_.emplace(stream, indices_.size());
        stream_index = indices_.find(stream);
    }
    std::vector<embark::logger::DataPiece> tmp;
    uint64_t last = 0;
    for (const auto & it : data) {
        uint64_t second = it.time_ns() / 1E9;
        if (second < first_second_) {
            continue;
        }
        if (second > last_second_) {
            break;
        }

        if (second != last) {
            storeData(last, tmp);
            tmp.clear();
            last = second;
        }
        embark::logger::DataPiece piece;
        piece.set_time_ns(it.time_ns());
        piece.set_data(it.data().data(), it.data().size());
        piece.set_stream_idx(stream_index->second);
        tmp.push_back(std::move(piece));
        continue;
    }
    storeData(last, tmp);
}

size_t StreamsCombiner::save(data_writer_t writer) const {
    std::map<uint64_t, OneSecondData>::const_iterator it = seconds_.begin();
    size_t size = 0;
    for (; it != seconds_.end(); it++) {
        size += it->second.save(writer);
    }
    return size;
}