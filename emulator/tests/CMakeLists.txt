# Minimal test harness: custom test executable until GoogleTest is wired.

add_executable(emulator_test test_main.cpp)
emulator_apply_compile_options(emulator_test)
target_link_libraries(emulator_test emulator_lib)
add_test(NAME emulator_test COMMAND emulator_test)
