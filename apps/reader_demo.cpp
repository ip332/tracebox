#include <iostream>

#include "logger.pb.h"
#include "reader_client.h"

// TODO: move the stream and size into the arguments list
int main(int argc, char *argv[]) {
    using namespace embark::logger;

    LogReadClient client;

    while (!client.connect("127.0.0.1", 49998)) {
        sleep(1);
        continue;
    }
    // Get list of available streams and print it
    auto list = client.getStreams(0, UINT64_MAX);
    std::cout << list->DebugString() << std::endl;

    while (true) {
        std::cout << "Enter the file name: ";
        std::string filename;
        std::getline(std::cin, filename);
        auto data = client.getData(filename, 0, UINT64_MAX);
        std::cout << data->ShortDebugString() << std::endl;
    }
    return 0;
}
