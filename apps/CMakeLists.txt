add_executable(logger_demo logger_demo.cpp)
add_dependencies(logger_demo LogWriter LogReader)
target_link_libraries(logger_demo LogWriter LogReader)

add_executable(client_demo client_demo.cpp)
target_link_libraries(client_demo LogClient)

add_executable(reader_demo reader_demo.cpp)
target_link_libraries(reader_demo LogClient)

add_executable(data_logger logger.cpp)
add_dependencies(data_logger LogWriter LogReader)
target_link_libraries(data_logger LogWriter LogReader)

add_subdirectory(asc_writer)
include_directories(asc_writer)
add_executable(get_can get_can.cpp)
add_dependencies(get_can AscWriter)
target_link_libraries(get_can LogClient AscWriter)


