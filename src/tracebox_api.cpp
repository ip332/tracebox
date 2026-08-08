#include "tracebox/reader.h"
#include "tracebox/recorder.h"
#include "tracebox/storage.h"
#include "tracebox/timestamp.h"
#include "tracebox/writer.h"

#include <limits>
#include <utility>

#include "data_types.h"
#include "log_client.h"
#include "log_writer.h"
#include "reader_client.h"

namespace tracebox {

std::uint32_t day(Timestamp timestamp) {
    return logger::Time2YYYYMMDD(timestamp);
}

class Recorder::Impl {
   public:
    Impl(std::string stream_name, std::uint32_t record_size)
        : client(std::move(stream_name), record_size) {}

    logger::LogClient client;
};

Recorder::Recorder(std::string stream_name, std::uint32_t record_size)
    : impl_(std::make_unique<Impl>(std::move(stream_name), record_size)) {}

Recorder::~Recorder() = default;
Recorder::Recorder(Recorder&&) noexcept = default;
Recorder& Recorder::operator=(Recorder&&) noexcept = default;

bool Recorder::connect(const std::string& address, std::uint32_t port) {
    return impl_->client.connect(address, port);
}

bool Recorder::connected() const { return impl_->client.is_connected(); }

void Recorder::disconnect() { impl_->client.disconnect(); }

bool Recorder::record(const std::string& data, Timestamp timestamp) {
    return impl_->client.logData(data, timestamp);
}

class Reader::Impl {
   public:
    logger::LogReadClient client;
};

Reader::Reader() : impl_(std::make_unique<Impl>()) {}
Reader::~Reader() = default;
Reader::Reader(Reader&&) noexcept = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

bool Reader::connect(const std::string& address, std::uint32_t port) {
    return impl_->client.connect(address, port);
}

bool Reader::connected() const { return impl_->client.is_connected(); }

void Reader::disconnect() { impl_->client.disconnect(); }

std::vector<Stream> Reader::streams(Timestamp start, Timestamp end,
                                    std::uint32_t start_index,
                                    std::uint32_t max_count) {
    auto response = impl_->client.getStreams(start, end, start_index, max_count);
    std::vector<Stream> result;
    if (!response) return result;
    result.reserve(response->stream_size());
    for (const auto& source : response->stream()) {
        Stream target;
        target.name = source.name();
        target.day = source.day();
        target.file = source.file();
        target.records_count = source.records_cnt();
        target.status = static_cast<StreamStatus>(source.status());
        target.start_time_ns = source.start_time_ns();
        target.end_time_ns = source.end_time_ns();
        result.push_back(std::move(target));
    }
    return result;
}

ReadResult Reader::read(const std::string& file, Timestamp start,
                        Timestamp end, std::uint32_t start_index,
                        std::uint32_t max_count) {
    auto response =
        impl_->client.getData(file, start, end, start_index, max_count);
    ReadResult result;
    if (!response) return result;
    result.error = response->errors();
    result.records.reserve(response->data_size());
    for (const auto& source : response->data()) {
        Sample target;
        target.time_ns = source.time_ns();
        target.data = source.data();
        target.stream_index = source.stream_idx();
        result.records.push_back(std::move(target));
    }
    return result;
}

class Storage::Impl {
   public:
    Impl(std::string folder, std::size_t max_size_bytes)
        : storage(std::make_shared<logger::Storage>(
              std::move(folder), static_cast<int>(max_size_bytes))) {}

    std::shared_ptr<logger::Storage> storage;
};

Storage::Storage(std::string folder, std::size_t max_size_bytes)
    : impl_(std::make_unique<Impl>(std::move(folder), max_size_bytes)) {}

Storage::~Storage() = default;
Storage::Storage(Storage&&) noexcept = default;
Storage& Storage::operator=(Storage&&) noexcept = default;

int Storage::write(const Record& record) {
    logger::LogRequest request;
    request.set_channel(record.channel);
    request.set_size(record.record_size);
    request.set_data(record.data);
    request.set_time_ns(record.timestamp);
    return impl_->storage->write(request);
}

class Writer::Impl {
   public:
    explicit Impl(std::shared_ptr<logger::Storage> storage)
        : writer(std::make_shared<logger::LogWriter>(std::move(storage))) {}

    std::shared_ptr<logger::LogWriter> writer;
};

Writer::Writer(std::shared_ptr<Storage> storage)
    : impl_(std::make_unique<Impl>(storage->impl_->storage)) {}

Writer::~Writer() = default;
Writer::Writer(Writer&&) noexcept = default;
Writer& Writer::operator=(Writer&&) noexcept = default;

void Writer::add(const Record& record) {
    logger::LogRequest request;
    request.set_channel(record.channel);
    request.set_size(record.record_size);
    request.set_data(record.data);
    request.set_time_ns(record.timestamp);
    impl_->writer->add(request);
}

}  // namespace tracebox
