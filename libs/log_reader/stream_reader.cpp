#include "stream_reader.h"

namespace embark {
namespace logger {

StreamReader::StreamReader(const std::filesystem::path &path) : status_(StreamStatus::kUnknown) {
    if (!is_regular_file(path)) {
        std::cerr << "File " << path << " is not a regular file and cannot be handled" << std::endl;
        return;
    }
    // Get basename by ignoring ".idx" extension
    auto const ext_pos = path.filename().string().find(".idx");
    if (ext_pos == std::string::npos) {
        std::cerr << "File " << path << " doesn't have '.idx' extension" << std::endl;
        return;
    }
    // Initialize the stream.
    index_ = std::make_unique<IndexFileReader>(path);

    if (index_->records_cnt() > 0) {
        status_ = StreamStatus::kHealthy;
    }
    if (index_->fixedSize()) {
        auto basename = path.filename().string().substr(0, ext_pos);
        data_ = std::make_unique<DataFileReader>(basename + ".data");
        if (data_->name() != index_->name()) {
            status_ = StreamStatus::kHeadersDifferent;
        }
    }
}

bool StreamReader::read(size_t index, DataPiece *payload) {
    auto time_ns = index_->timeOf(index);
    if (!time_ns) {
        return false;
    }
    if (index_->fixedSize()) {
        std::string data;
        if (!index_->readPayload(&data)) {
            return false;
        }
        payload->set_time_ns(time_ns);
        payload->set_data(data);
    } else {
        auto offset = index_->readOffset();
        if (!offset) {
            return false;
        }
        data_->read(offset, payload);
    }
    return true;
}

}}