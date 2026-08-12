#include "tracebox_logger.h"
#include <gflags/gflags.h>

#include "log_reader.h"
#include "storage.h"
#include "unistd.h"

DEFINE_string(folder, "", "Storage folder.");
DEFINE_int32(max_size, -1, "Total files size limit in MiB.");
DEFINE_int32(logger_port, 49999, "Logger's listening port.");
DEFINE_int32(reader_port, 49998, "Reader's listening port.");

int main(int argc, char **argv) {
    gflags::SetUsageMessage("logger --folder <path> --max_size <MiB> [options]");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_folder.empty() || FLAGS_max_size < 0) {
        std::cerr << "Error: --folder and --max_size are required.\n";
        std::cerr << gflags::ProgramUsage();
        return 1;
    }

    if (!std::filesystem::is_directory(FLAGS_folder)) {
        std::cerr << "Folder '" << FLAGS_folder << "' doesn't exist" << std::endl;
        return 2;
    }

    using namespace tracebox::logger;

    auto storage = std::make_shared<Storage>(
        FLAGS_folder, static_cast<int64_t>(FLAGS_max_size) * 1024 * 1024);
    TraceboxLogger logger(FLAGS_logger_port, storage);
    LogReader reader(FLAGS_reader_port, FLAGS_folder);

    while (true) {
        sleep(1);
    }
}
