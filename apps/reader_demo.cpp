#include <iostream>

#include "reader_client.h"

// TODO: move the stream and size into the arguments list
int main(int argc, char *argv[]) {
    using namespace tracebox::logger;

    LogReadClient client;

    while (!client.connect("127.0.0.1", 49998)) {
        sleep(1);
        continue;
    }
    // Get list of available streams and print it
    auto list = client.getStreams(0, UINT64_MAX);
    std::cout << list->DebugString() << std::endl;

    while (list->stream_size() > 0 && true) {
        std::cout << "Enter the file name pattern: ";
        std::string pattern;
        std::getline(std::cin, pattern);
        std::string filename;
        for (const auto &s : list->stream()) {
            if (s.file().find(pattern) != std::string::npos) {
                filename = s.file();
            }
        }
        if (filename.empty()) {
            std::cerr << "No file name matches '" << pattern
                      << "' pattern, try again." << std::endl;
            continue;
        }
        auto data = client.getData(filename, 0, UINT64_MAX);
        std::cout << data->ShortDebugString() << std::endl;
    }
    return 0;
}
