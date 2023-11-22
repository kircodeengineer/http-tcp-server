cmake_minimum_required(VERSION 3.11)

project(TCP_HTTP_Client LANGUAGES CXX)
# Исходый код будет компилироваться с поддержкой стандарта С++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

link_libraries(pthread)
#протобаф
find_package(Protobuf REQUIRED)


protobuf_generate_cpp(PROTO_SRC PROTO_HDRS
   proto/exchange.proto)

set(SOURCES
    src/main.cpp
    proto/exchange.proto)

include_directories(src proto)


add_executable(${PROJECT_NAME} ${SOURCES} ${PROTO_SRC} ${PROTO_HDRS})

# Просим компоновщик подключить библиотеку для поддержки потоков
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


target_include_directories(${PROJECT_NAME} PUBLIC ${Protobuf_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

string(REPLACE "protobuf.lib" "protobufd.lib" "Protobuf_LIBRARY_DEBUG" "$Protobuf_LIBRARY_DEBUG")
string(REPLACE "protobuf.a" "protobufd.a" "Protobuf_LIBRARY_DEBUG" "$Protobuf_LIBRARY_DEBUG")


target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads PUBLIC ${Protobuf_LIBRARY})

