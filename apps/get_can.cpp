#include <iostream>
#include <fstream>

#include "reader_client.h"
#include "options.h"
#include "asc_writer.h"
#include "streams_combiner.h"

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
    using namespace embark::logger;
    OptionsManager options;

    int reader_port = 49998;
    std::string logger;
    int day = 0;
    std::string start_time, end_time, output;

    options.addOption("-addr", "Logger's IP or", &logger);
    options.addOption("-port", "Logger's listening port", &reader_port);
    options.addOption("-day", "Day in YYYYMMDD format", &day);
    options.addOption("-start", "Start time in HH:MM:SS format", &start_time);
    options.addOption("-end", "End time in HH:MM:SS format", &end_time);
    options.addOption("-out", "Output file name", &output);

    if (logger.empty() || day == 0 || start_time.empty() || end_time.empty() || output.empty()) {
        std::cerr << "Error: Insufficient arguments." << std::endl;
        std::cerr << "Usage: get_can -addr 10.2.5.5 -day 20230223 -start 082300 -end 123456 -out /tmp/file.asc [-port 12345]" << std::endl;
        options.print();
        return 1;
    }

    LogReadClient client;

    if (!client.connect(logger, reader_port)) {
        std::cerr << "Error connecting to the logger" << std::endl;
        return 2;
    }

    // Determine time range
    uint64_t start_ns = timeNs(day, start_time) * 1E9;
    uint64_t end_ns = timeNs(day, end_time) * 1E9;

    // Open file for output
    std::ofstream file(output);
    if (!file.is_open()) {
        std::cerr << "Couldn't open file " << output << std::endl;
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
    auto count = combiner.save([&writer](const std::vector<embark::logger::DataPiece> & data){
        writer.write(data);
    });
    std::cout << "File " << output << " with " << count << " records was created successfully." << std::endl;
    file.close();
    return 0;
}
