add_executable(logger_demo logger_demo.cpp)
add_dependencies(logger_demo LogWriter LogReader)
target_link_libraries(logger_demo LogWriter LogReader)

add_executable(client_demo client_demo.cpp)
target_link_libraries(client_demo LogClient)

add_executable(reader_demo reader_demo.cpp)
target_link_libraries(reader_demo LogClient)
