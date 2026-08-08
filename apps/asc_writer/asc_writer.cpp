#include <ctime>
#include <iomanip>
#include "asc_writer.h"

static const char *day[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

static const char *month[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

bool::AscWriter::initStream(uint64_t time_ns) {
    // Expected format:
    // date Fri Feb 17 15:06:56 2023
    // base hex  timestamps absolute
    // no internal events logged

    // First, get number of seconds with minutes  accuracy.
    uint64_t sec = time_ns / 1E9;
    start_time_ns_ = (sec / 60) * 60;
    time_t start(start_time_ns_);
    std::tm tm = *std::localtime(&start);
    out_ << "date " << day[tm.tm_wday] << " " << month[tm.tm_mon] << " " << tm.tm_mday;
    out_ << " " << tm.tm_hour << ":" << tm.tm_min << ":" << tm.tm_sec  << " ";
    out_ << (1900 + tm.tm_year) << std::endl;
    out_ << "base hex  timestamps absolute" << std::endl;
    out_ << "no internal events logged" << std::endl;
    auto it = streams_.begin();
    out_ << "# Channel  Name" << std::endl;
    for (; it != streams_.end(); it++) {
        out_ << "# " << it->second << "\t\t\t" << it->first << std::endl;
    }
    return true;
}

void AscWriter::write(const std::vector<tracebox::logger::DataPiece> &data) {
    if (data.empty()) {
        // TODO: error logging.
        return;
    }
    if(start_time_ns_ == 0) {
        initStream(data[0].time_ns());
    }
    for (const auto & it : data) {
        if (it.data().length() < 4) {
            // TODO: error logging - invalid CAN ID
            continue;
        }
        uint32_t can_id = *(reinterpret_cast<const uint32_t *>(it.data().c_str()));
        size_t dlc = it.data().length() - 4;
        // Expected format:
        //    0.000123 1  34B             Rx   d 8 A8 C3 50 4E DB FD 3C 59
        char buffer[50];
        sprintf(buffer, "%11.6f %d %8X", (it.time_ns() - start_time_ns_) / 1E9, it.stream_idx(), can_id);
        out_ << buffer;
        if (can_id & 0x80000000) {
            out_ << 'x';
        }
        out_ << " Rx   d " << dlc << ' ';
        for (size_t i = 4; i < it.data().length(); i++) {
            out_ << std::hex << std::setfill('0') << std::setw(2) << it.data()[i];
        }
        out_ << std::endl;
    }
}