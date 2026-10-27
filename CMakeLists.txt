cmake_minimum_required(VERSION 3.10)
project(MiniKernelSimulator LANGUAGES CXX)

enable_testing()

add_executable(mini_kernel src/main.cpp)
target_include_directories(mini_kernel PRIVATE include)
target_compile_features(mini_kernel PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(mini_kernel PRIVATE /W4)
else()
    target_compile_options(mini_kernel PRIVATE -Wall -Wextra)
endif()

add_executable(simulator_tests tests/simulator_tests.cpp)
target_include_directories(simulator_tests PRIVATE include)
target_compile_features(simulator_tests PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(simulator_tests PRIVATE /W4)
else()
    target_compile_options(simulator_tests PRIVATE -Wall -Wextra)
endif()
add_test(NAME simulator_tests COMMAND simulator_tests)

# The browser visualizer keeps its pure simulation core inside web/index.html
# between the KERNEL-CORE markers. When Node.js is available, register the
# dependency-free regression test for that core.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME web_kernel_core
             COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/kernel_core_test.js)
    add_test(NAME frontend_selftest
             COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/frontend_selftest_node.js)
endif()
