add_library(Tracebox STATIC ${CMAKE_SOURCE_DIR}/src/tracebox_api.cpp)

target_include_directories(Tracebox
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_SOURCE_DIR}/libs/log_client
        ${CMAKE_SOURCE_DIR}/libs/log_reader
        ${CMAKE_SOURCE_DIR}/libs/log_writer/writer
        ${CMAKE_SOURCE_DIR}/libs/log_writer/storage_management)

target_link_libraries(Tracebox
    PUBLIC LogClient LogReader LogWriter StorageManagement data_protos
           ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(Tracebox LogClient LogReader LogWriter StorageManagement
                 data_protos)
