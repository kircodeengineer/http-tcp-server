cmake_minimum_required(VERSION 3.11)

project(AsyncTCPservser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#протобаф
find_package(Protobuf REQUIRED)

include_directories(src proto)

protobuf_generate_cpp(PROTO_SRC PROTO_HDRS
    proto/exchange.proto)

set(SOURCES
    src/main.cpp
    src/http_server.cpp
    src/sdk.h
    proto/exchange.proto)

set(HEADERS
    src/main.cpp
    src/http_server.h
    proto/exchange.proto)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${PROTO_SRC} ${PROTO_HDRS} )

target_include_directories(${PROJECT_NAME} PUBLIC ${Protobuf_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

string(REPLACE "protobuf.lib" "protobufd.lib" "Protobuf_LIBRARY_DEBUG" "$Protobuf_LIBRARY_DEBUG")
string(REPLACE "protobuf.a" "protobufd.a" "Protobuf_LIBRARY_DEBUG" "$Protobuf_LIBRARY_DEBUG")


target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads PUBLIC ${Protobuf_LIBRARY})

