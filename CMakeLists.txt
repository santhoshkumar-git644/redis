cmake_minimum_required(VERSION 3.14)
project(InfernoCache VERSION 1.0.0 DESCRIPTION "A Redis-compatible distributed in-memory data store" LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wshadow")

# Build directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Threading
find_package(Threads REQUIRED)

# Executable target
add_executable(inferno-server
    src/server/main.cpp
    src/network/socket.cpp
    src/network/event_loop.cpp
    src/network/connection_manager.cpp
    src/network/tcp_server.cpp
    src/memory/allocator.cpp
    src/protocol/resp_parser.cpp
    src/protocol/resp_types.cpp
    src/storage/value.cpp
    src/storage/dict.cpp
    src/server/command_handler.cpp
)

target_link_libraries(inferno-server PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(inferno-server PRIVATE ws2_32)
endif()

# Testing (GoogleTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  inferno-tests
  tests/network_test.cpp
  tests/resp_test.cpp
  tests/storage_test.cpp
  tests/memory_test.cpp
  tests/expiration_test.cpp
  tests/list_test.cpp
  tests/set_hash_test.cpp
  tests/zset_test.cpp
  tests/aof_test.cpp
  tests/rdb_test.cpp
  tests/eviction_test.cpp
  tests/pubsub_test.cpp
  tests/transaction_test.cpp
  src/network/socket.cpp
  src/network/event_loop.cpp
  src/network/connection_manager.cpp
  src/network/tcp_server.cpp
  src/memory/allocator.cpp
  src/protocol/resp_parser.cpp
  src/protocol/resp_types.cpp
  src/storage/value.cpp
  src/storage/dict.cpp
  src/storage/skiplist.cpp
  src/server/pubsub.cpp
  src/persistence/aof.cpp
  src/persistence/rdb.cpp
  src/server/command_handler.cpp
)

target_link_libraries(
  inferno-tests
  gtest_main
  Threads::Threads
)
if(WIN32)
    target_link_libraries(inferno-tests ws2_32)
endif()

include(GoogleTest)
gtest_discover_tests(inferno-tests)
