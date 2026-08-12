find_package(GTest REQUIRED)
find_package(absl CONFIG QUIET)

set(TRACEBOX_TEST_PROTOBUF_LIBS ${PROTOBUF_LIBRARY})
if (TARGET absl::log AND TARGET absl::check)
    list(APPEND TRACEBOX_TEST_PROTOBUF_LIBS absl::log absl::check)
endif()

add_executable(tracebox_storage_tests tracebox_storage_test.cpp
               ${CMAKE_SOURCE_DIR}/libs/log_reader/storage_scanner.cpp
               ${CMAKE_SOURCE_DIR}/libs/log_reader/stream_reader.cpp)
target_link_libraries(tracebox_storage_tests PRIVATE StorageManagement data_protos
                      ${TRACEBOX_TEST_PROTOBUF_LIBS} GTest::gtest_main)
target_include_directories(tracebox_storage_tests PRIVATE
                           ${CMAKE_SOURCE_DIR}/libs/log_reader)

include(GoogleTest)
gtest_discover_tests(tracebox_storage_tests)

if (TRACEBOX_BUILD_RUNTIME)
    add_executable(tracebox_public_api_test public_api_test.cpp)
    target_link_libraries(tracebox_public_api_test PRIVATE Tracebox GTest::gtest_main)
    target_include_directories(tracebox_public_api_test PRIVATE
                               ${CMAKE_SOURCE_DIR}/include)
    include(GoogleTest)
    gtest_discover_tests(tracebox_public_api_test)
endif()
