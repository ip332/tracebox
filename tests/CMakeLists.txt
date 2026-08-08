find_package(GTest REQUIRED)
find_package(absl CONFIG QUIET)

set(DATA_LOGGER_TEST_PROTOBUF_LIBS ${PROTOBUF_LIBRARY})
if (TARGET absl::log AND TARGET absl::check)
    list(APPEND DATA_LOGGER_TEST_PROTOBUF_LIBS absl::log absl::check)
endif()

add_executable(data_logger_tests data_logger_test.cpp
               ${CMAKE_SOURCE_DIR}/libs/log_reader/storage_scanner.cpp
               ${CMAKE_SOURCE_DIR}/libs/log_reader/stream_reader.cpp)
target_link_libraries(data_logger_tests PRIVATE StorageManagement data_protos
                      ${DATA_LOGGER_TEST_PROTOBUF_LIBS} GTest::gtest_main)
target_include_directories(data_logger_tests PRIVATE
                           ${CMAKE_SOURCE_DIR}/libs/log_reader)

include(GoogleTest)
gtest_discover_tests(data_logger_tests)
