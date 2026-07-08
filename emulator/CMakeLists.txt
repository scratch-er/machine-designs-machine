cmake_minimum_required(VERSION 3.16)
project(ai_soc_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build emulator unit tests" ON)
option(BUILD_RTL "Build RTL simulation targets" OFF)

set(EMULATOR_COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND EMULATOR_COMPILE_OPTIONS -O2 -Werror)
endif()

function(emulator_apply_compile_options target)
    target_compile_options(${target} PRIVATE ${EMULATOR_COMPILE_OPTIONS})
endfunction()

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
emulator_apply_compile_options(emulator_lib)

# Interpreter binary (always built).
add_executable(emulator src/main.cpp)
emulator_apply_compile_options(emulator)
target_link_libraries(emulator emulator_lib)

# Optional RTL simulation targets.
if(BUILD_RTL)
    find_package(verilator REQUIRED HINTS $ENV{VERILATOR_ROOT})

    set(NPC_RTL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core/rtl)
    set(NPC_DPI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core/dpi)

    set(NPC_RTL_SOURCES
        ${NPC_RTL_DIR}/npc_core.v
        ${NPC_RTL_DIR}/npc_single_cycle.v
        ${NPC_RTL_DIR}/npc_icache.v
        ${NPC_RTL_DIR}/npc_axi_master.v
        ${NPC_RTL_DIR}/npc_decoder.v
        ${NPC_RTL_DIR}/npc_imm.v
        ${NPC_RTL_DIR}/npc_alu.v
        ${NPC_RTL_DIR}/npc_branch.v
        ${NPC_RTL_DIR}/npc_load_store_unit.v
        ${NPC_RTL_DIR}/npc_regfile.v
        ${NPC_RTL_DIR}/npc_csr_file.v
        ${NPC_RTL_DIR}/npc_clint.v
    )

    # RTL adapter that implements the ISS interface.
    add_library(rtl_iss STATIC src/rtl_iss.cpp)
    target_include_directories(rtl_iss PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(rtl_iss PUBLIC emulator_lib)

    # Generate the Verilated model and attach it to rtl_iss.
    verilate(rtl_iss
        TRACE_VCD
        TOP_MODULE npc_core
        PREFIX Vnpc_core
        SOURCES ${NPC_RTL_SOURCES}
        INCLUDE_DIRS ${NPC_RTL_DIR}
        VERILATOR_ARGS -Wall -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM
    )

    # emulator-rtl: same CLI as emulator, but drives the RTL core.
    add_executable(emulator-rtl src/main.cpp)
    emulator_apply_compile_options(emulator-rtl)
    target_compile_definitions(emulator-rtl PRIVATE WITH_RTL)
    target_link_libraries(emulator-rtl emulator_lib rtl_iss)

    # npc-difftest: compare emulator reference against RTL DUT.
    add_executable(npc-difftest src/difftest_main.cpp)
    emulator_apply_compile_options(npc-difftest)
    target_link_libraries(npc-difftest emulator_lib rtl_iss)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
