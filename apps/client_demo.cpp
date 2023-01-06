#include <iostream>

#include "log_client.h"
#include "logger.pb.h"

// TODO: move the stream and size into the arguments list
int main(int argc, char *argv[]) {
    using namespace embark::logger;

    std::string stream("Sample");
    LogClient client(stream, 0);

    while (true) {
        std::string record;
        std::getline(std::cin, record);
        if (!client.is_connected()) {
            while (!client.connect("127.0.0.1", 49999)) {
                sleep(1);
                continue;
            }
        }
        using namespace std::chrono;
        if (!client.logData(record,
                            time_point_cast<nanoseconds>(system_clock::now())
                                .time_since_epoch()
                                .count())) {
            client.disconnect();
        }
    }
}
