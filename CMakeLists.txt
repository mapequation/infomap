cmake_minimum_required(VERSION 3.21)
project(infomap LANGUAGES CXX)

option(INFOMAP_USE_OPENMP "Enable OpenMP support when available." ON)
option(INFOMAP_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers." OFF)
set(INFOMAP_MODE "release" CACHE STRING "Build mode shared with the Make-based native build policy.")
set_property(CACHE INFOMAP_MODE PROPERTY STRINGS release debug)
set(INFOMAP_EXTRA_CPPFLAGS "" CACHE STRING "Extra preprocessor/compiler flags appended to the shared build policy.")
set(INFOMAP_EXTRA_CXX_FLAGS "" CACHE STRING "Extra C++ compiler flags appended to the shared build policy.")
set(INFOMAP_EXTRA_LINK_FLAGS "" CACHE STRING "Extra linker flags appended to the shared build policy.")

# Keep CMake build types available for generators and output directories, but let the
# shared build policy own optimization, debug, and OpenMP flags across all surfaces.
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)

include(CTest)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(INFOMAP_USE_OPENMP)
    set(INFOMAP_OPENMP_ARG 1)
else()
    set(INFOMAP_OPENMP_ARG 0)
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "CPPFLAGS=${INFOMAP_EXTRA_CPPFLAGS}"
        "CXXFLAGS=${INFOMAP_EXTRA_CXX_FLAGS}"
        "LDFLAGS=${INFOMAP_EXTRA_LINK_FLAGS}"
        "MACOSX_DEPLOYMENT_TARGET=$ENV{MACOSX_DEPLOYMENT_TARGET}"
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_config.py" json
        --mode "${INFOMAP_MODE}"
        --openmp "${INFOMAP_OPENMP_ARG}"
        --compiler "${CMAKE_CXX_COMPILER}"
        --platform "${CMAKE_SYSTEM_NAME}"
    OUTPUT_VARIABLE INFOMAP_BUILD_CONFIG_JSON
    RESULT_VARIABLE INFOMAP_BUILD_CONFIG_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT INFOMAP_BUILD_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to resolve Infomap build config.")
endif()

string(JSON INFOMAP_COMPILE_FLAG_COUNT LENGTH "${INFOMAP_BUILD_CONFIG_JSON}" compile_flags)
math(EXPR INFOMAP_COMPILE_FLAG_LAST "${INFOMAP_COMPILE_FLAG_COUNT} - 1")
set(INFOMAP_COMPILE_FLAGS "")
if(INFOMAP_COMPILE_FLAG_COUNT GREATER 0)
    foreach(INDEX RANGE ${INFOMAP_COMPILE_FLAG_LAST})
        string(JSON INFOMAP_COMPILE_FLAG GET "${INFOMAP_BUILD_CONFIG_JSON}" compile_flags ${INDEX})
        list(APPEND INFOMAP_COMPILE_FLAGS "${INFOMAP_COMPILE_FLAG}")
    endforeach()
endif()

string(JSON INFOMAP_LINK_FLAG_COUNT LENGTH "${INFOMAP_BUILD_CONFIG_JSON}" link_flags)
math(EXPR INFOMAP_LINK_FLAG_LAST "${INFOMAP_LINK_FLAG_COUNT} - 1")
set(INFOMAP_LINK_FLAGS "")
if(INFOMAP_LINK_FLAG_COUNT GREATER 0)
    foreach(INDEX RANGE ${INFOMAP_LINK_FLAG_LAST})
        string(JSON INFOMAP_LINK_FLAG GET "${INFOMAP_BUILD_CONFIG_JSON}" link_flags ${INDEX})
        list(APPEND INFOMAP_LINK_FLAGS "${INFOMAP_LINK_FLAG}")
    endforeach()
endif()

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
target_compile_features(infomap_core PUBLIC cxx_std_14)
set_target_properties(infomap_core PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(infomap_core PRIVATE ${INFOMAP_COMPILE_FLAGS})
target_link_options(infomap_core PUBLIC ${INFOMAP_LINK_FLAGS})

if(INFOMAP_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(infomap_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(infomap_core PUBLIC -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "INFOMAP_ENABLE_SANITIZERS requires Clang or GCC.")
    endif()
endif()

add_executable(infomap_cli "${infomap_main_source}")
target_link_libraries(infomap_cli PRIVATE infomap_core)
set_target_properties(infomap_cli PROPERTIES OUTPUT_NAME "Infomap")

if(BUILD_TESTING)
    add_executable(
        infomap_native_benchmark
        test/bench/native_benchmark.cpp
    )
    target_include_directories(
        infomap_native_benchmark
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/test"
    )
    target_link_libraries(infomap_native_benchmark PRIVATE infomap_core)

    function(add_infomap_cpp_test target_name source_file labels)
        add_executable(
            ${target_name}
            test/cpp/doctest_main.cpp
            ${source_file}
        )
        target_include_directories(
            ${target_name}
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
                "${CMAKE_CURRENT_SOURCE_DIR}/test/cpp"
        )
        target_compile_definitions(
            ${target_name}
            PRIVATE
                INFOMAP_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_link_libraries(${target_name} PRIVATE infomap_core)
        add_test(NAME ${target_name} COMMAND ${target_name})
        set_tests_properties(${target_name} PROPERTIES LABELS "${labels}")
    endfunction()

    add_infomap_cpp_test(infomap_cpp_network_tests test/cpp/test_network.cpp "fast;core;network;parser")
    add_infomap_cpp_test(infomap_cpp_lifecycle_tests test/cpp/test_infomap_wrapper.cpp "fast;core;lifecycle;crash")
    add_infomap_cpp_test(infomap_cpp_partition_tests test/cpp/test_partition.cpp "fast;core;partition;parser")
    add_infomap_cpp_test(infomap_cpp_flow_tests test/cpp/test_flow.cpp "fast;core;flow")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(infomap_cpp_lifecycle_tests PRIVATE -fno-access-control)
        target_compile_options(infomap_cpp_partition_tests PRIVATE -fno-access-control)
    endif()

    add_custom_target(
        infomap_cpp_tests
        DEPENDS
            infomap_cpp_network_tests
            infomap_cpp_lifecycle_tests
            infomap_cpp_partition_tests
            infomap_cpp_flow_tests
    )
endif()
