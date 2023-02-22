#include "data_logger.h"
#include "log_reader.h"
#include "storage.h"
#include "options.h"
#include "unistd.h"

int main(int argc, char *argv[]) {
    OptionsManager options;

    std::string folder;
    int max_size_mb = -1;
    int logger_port = 49999;
    int reader_port = 49998;

    options.addOption("-folder", "Storage folder.", &folder);
    options.addOption("-max_size", "Total files size limit in Mb.", &max_size_mb);
    options.addOption("-logger_port", "Logger's listening port", &logger_port);
    options.addOption("-reader_port", "Reader's listening port", &reader_port);

    if (folder.empty() || max_size_mb == -1) {
        std::cerr << "Usage: logger [options]" << std::endl;
        options.print();
        return 1;
    }

    if (!std::filesystem::is_directory(folder)) {
        std::cerr << "Folder '" << folder << "' doesn't exist" << std::endl;
        return 2;
    }

    using namespace embark::logger;

    auto storage = std::make_shared<Storage>(folder, max_size_mb * 1024 * 1024);
    DataLogger logger(logger_port, storage);
    LogReader reader(reader_port, folder);

    while (true) {
        sleep(1);
    }
}