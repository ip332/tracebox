#include <string.h>

#include <chrono>
#include <iostream>

#include "log_client.h"

int main(int argc, char *argv[]) {
    using namespace embark::logger;

    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: client_demo <StreamName> [record_size]"
                  << std::endl;
        return 1;
    }
    size_t record_size = 0;
    if (argc == 3) {
        record_size = std::stoul(argv[2]);
    }
    std::string stream(argv[1]);
    LogClient client(stream, record_size);

    while (true) {
        std::string record;
        std::getline(std::cin, record);
        if (record.empty()) {
            continue;
        }
        if (!client.is_connected()) {
            while (!client.connect("127.0.0.1", 49999)) {
                sleep(1);
                continue;
            }
        }
        using namespace std::chrono;
        if (record_size) {
            record.resize(record_size, ' ');
        }
        if (!client.logData(record,
                            time_point_cast<nanoseconds>(system_clock::now())
                                .time_since_epoch()
                                .count())) {
            client.disconnect();
        }
    }
}
