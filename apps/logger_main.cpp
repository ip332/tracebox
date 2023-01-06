#include "data_logger.h"
#include "log_reader.h"
#include "storage.h"
#include "unistd.h"

// TODO: move folder and port into the arguments list
int main(int argc, char *argv[]) {
    using namespace embark::logger;

    std::string folder("/tmp/nvme");
    auto storage = std::make_shared<Storage>(folder, 100);
    DataLogger logger(49999, storage);

    LogReader reader(49998, folder);

    while (true) {
        sleep(1);
    }
}