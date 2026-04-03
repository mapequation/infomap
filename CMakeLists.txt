cmake_minimum_required(VERSION 3.21)
project(infomap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic -Wshadow")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(INFOMAP_USE_OPENMP "Enable OpenMP support when available." ON)
option(INFOMAP_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers." OFF)
set(INFOMAP_EXTRA_CXX_FLAGS "" CACHE STRING "Extra C++ compiler flags appended to the global build flags.")
set(INFOMAP_EXTRA_LINK_FLAGS "" CACHE STRING "Extra linker flags appended to executable and shared library link flags.")

string(APPEND CMAKE_CXX_FLAGS " ${INFOMAP_EXTRA_CXX_FLAGS}")
string(APPEND CMAKE_EXE_LINKER_FLAGS " ${INFOMAP_EXTRA_LINK_FLAGS}")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${INFOMAP_EXTRA_LINK_FLAGS}")

if(INFOMAP_USE_OPENMP)
    find_package(OpenMP)
endif()
include(CTest)

find_program(CCACHE ccache)
if(CCACHE)
    message("-- Found Ccache: TRUE")
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
else()
    message("-- Found Ccache: FALSE")
endif()

file(GLOB_RECURSE infomap_headers CONFIGURE_DEPENDS "src/*.h")
file(GLOB_RECURSE infomap_sources CONFIGURE_DEPENDS "src/*.cpp")
set(infomap_main_source "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM infomap_sources "${infomap_main_source}")

add_library(infomap_core ${infomap_headers} ${infomap_sources})
target_include_directories(infomap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(INFOMAP_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(infomap_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(infomap_core PUBLIC -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "INFOMAP_ENABLE_SANITIZERS requires Clang or GCC.")
    endif()
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(infomap_core PUBLIC OpenMP::OpenMP_CXX)
endif()

add_executable(infomap_cli "${infomap_main_source}")
target_link_libraries(infomap_cli PRIVATE infomap_core)
set_target_properties(infomap_cli PROPERTIES OUTPUT_NAME "Infomap")

if(BUILD_TESTING)
    add_executable(
        infomap_cpp_tests
        test/cpp/doctest_main.cpp
        test/cpp/test_network.cpp
        test/cpp/test_infomap_wrapper.cpp
    )
    target_include_directories(
        infomap_cpp_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/cpp"
    )
    target_compile_definitions(
        infomap_cpp_tests
        PRIVATE
            INFOMAP_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(infomap_cpp_tests PRIVATE infomap_core)
    add_test(NAME infomap_cpp_tests COMMAND infomap_cpp_tests)
    set_tests_properties(infomap_cpp_tests PROPERTIES LABELS "fast;core;crash")
endif()
