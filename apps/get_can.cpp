#include <iostream>
#include <fstream>
#include <gflags/gflags.h>

#include "reader_client.h"
#include "asc_writer.h"
#include "streams_combiner.h"

DEFINE_string(addr, "", "Logger IP address or hostname.");
DEFINE_int32(port, 49998, "Logger's reader port.");
DEFINE_int32(day, 0, "Day in YYYYMMDD format.");
DEFINE_string(start, "", "Start time in HH:MM:SS format.");
DEFINE_string(end, "", "End time in HH:MM:SS format.");
DEFINE_string(out, "", "Output ASC file name.");

// Converts day (YYYYMMDD) and time (HH:MM:SS) into the epoch time.
uint64_t timeNs(int day, const std::string &time) {
    struct tm t = {};
    t.tm_year = day / 10000 - 1900;
    t.tm_mon = day / 100 - (day / 10000) * 10000;
    t.tm_mday = day - (day / 100) * 100;
    sscanf(time.data(), "%2d:%2d:%2d", &t.tm_hour, &t.tm_min, &t.tm_sec);
    unsigned long epoch = mktime(&t);
    return epoch;
}

int main(int argc, char *argv[]) {
    using namespace tracebox::logger;
    gflags::SetUsageMessage("get_can --addr <host> --day <YYYYMMDD> "
                            "--start <HH:MM:SS> --end <HH:MM:SS> "
                            "--out <file> [options]");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_addr.empty() || FLAGS_day == 0 || FLAGS_start.empty() ||
        FLAGS_end.empty() || FLAGS_out.empty()) {
        std::cerr << "Error: Insufficient arguments." << std::endl;
        std::cerr << gflags::ProgramUsage();
        return 1;
    }

    LogReadClient client;

    if (!client.connect(FLAGS_addr, FLAGS_port)) {
        std::cerr << "Error connecting to the logger" << std::endl;
        return 2;
    }

    // Determine time range
    uint64_t start_ns = timeNs(FLAGS_day, FLAGS_start) * 1E9;
    uint64_t end_ns = timeNs(FLAGS_day, FLAGS_end) * 1E9;

    // Open file for output
    std::ofstream file(FLAGS_out);
    if (!file.is_open()) {
        std::cerr << "Couldn't open file " << FLAGS_out << std::endl;
        return 3;
    }
    // Get list of available streams and print it
    auto list = client.getStreams(start_ns, end_ns);
    if (!list) {
        std::cerr << "Error getting response from the logger" << std::endl;
        return 4;
    }
    if (list->has_errors()) {
        std::cerr << list->errors() << std::endl;
    }
    if (list->data_size() == 0) {
        std::cerr << "There are no records for given time interval" << std::endl;
        return 5;
    }

    StreamsCombiner combiner(start_ns, end_ns);
    for (const auto & it : list->stream()) {
        auto data = client.getData(it.file(), 0, UINT64_MAX);
        combiner.addRecords(it.name(), data->data());
    }
    AscWriter writer(file, combiner.streams());
    auto count = combiner.save([&writer](const std::vector<tracebox::logger::DataPiece> & data){
        writer.write(data);
    });
    std::cout << "File " << FLAGS_out << " with " << count
              << " records was created successfully." << std::endl;
    file.close();
    return 0;
}
