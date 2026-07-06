cmake_minimum_required(VERSION 3.16)
project(ai_soc_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build emulator unit tests" ON)

add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -Werror)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(EMULATOR_SOURCES
    src/config.cpp
    src/hart.cpp
    src/decoder.cpp
    src/emulator_iss.cpp
    src/memory.cpp
    src/clint.cpp
    src/uart.cpp
    src/shell.cpp
    src/trace.cpp
    src/difftest.cpp
)

add_library(emulator_lib STATIC ${EMULATOR_SOURCES})

add_executable(emulator src/main.cpp)
target_link_libraries(emulator emulator_lib)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
