#include "data_logger.h"
#include "log_reader.h"
#include "storage.h"
#include "unistd.h"

int main(int argc, char *argv[]) {
    using namespace tracebox::logger;

    if (argc != 3) {
        std::cerr << "Usage: logger_demo <folder> <max_size_bytes>"
                  << std::endl;
        return 1;
    }
    size_t max_size = std::stoul(argv[2]);

    std::string folder(argv[1]);
    auto storage = std::make_shared<Storage>(folder, max_size);

    DataLogger logger(49999, storage);
    LogReader reader(49998, folder);

    while (true) {
        sleep(1);
    }
}