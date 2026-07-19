cmake_minimum_required(VERSION 3.21)
project(infomap LANGUAGES CXX)

option(INFOMAP_USE_OPENMP "Enable OpenMP support when available." ON)
option(INFOMAP_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers." OFF)
option(INFOMAP_NATIVE_ARCH "Enable -march=native, LTO, and loop unrolling for the host CPU. Produces non-portable binaries; opt-in for local builds only." OFF)
set(INFOMAP_MODE "release" CACHE STRING "Build mode shared with the Make-based native build policy.")
set_property(CACHE INFOMAP_MODE PROPERTY STRINGS release debug)
set(INFOMAP_EXTRA_CPPFLAGS "" CACHE STRING "Extra preprocessor/compiler flags appended to the shared build policy.")
set(INFOMAP_EXTRA_CXX_FLAGS "" CACHE STRING "Extra C++ compiler flags appended to the shared build policy.")
set(INFOMAP_EXTRA_LINK_FLAGS "" CACHE STRING "Extra linker flags appended to the shared build policy.")
set(INFOMAP_FEATURES "" CACHE STRING "Space- or comma-separated experimental feature names to compile in. Empty builds the default stable feature set.")

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

if(INFOMAP_NATIVE_ARCH)
    set(INFOMAP_NATIVE_ARCH_ARG 1)
else()
    set(INFOMAP_NATIVE_ARCH_ARG 0)
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
        --native-arch "${INFOMAP_NATIVE_ARCH_ARG}"
        --features "${INFOMAP_FEATURES}"
        --compiler "${CMAKE_CXX_COMPILER}"
        --platform "${CMAKE_SYSTEM_NAME}"
    OUTPUT_VARIABLE INFOMAP_BUILD_CONFIG_JSON
    ERROR_VARIABLE INFOMAP_BUILD_CONFIG_ERROR
    RESULT_VARIABLE INFOMAP_BUILD_CONFIG_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT INFOMAP_BUILD_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to resolve Infomap build config: ${INFOMAP_BUILD_CONFIG_ERROR}")
endif()

function(infomap_json_array output_name field_name)
    string(JSON ITEM_COUNT LENGTH "${INFOMAP_BUILD_CONFIG_JSON}" "${field_name}")
    set(ITEMS "")
    if(ITEM_COUNT GREATER 0)
        math(EXPR ITEM_LAST "${ITEM_COUNT} - 1")
        foreach(INDEX RANGE ${ITEM_LAST})
            string(JSON ITEM GET "${INFOMAP_BUILD_CONFIG_JSON}" "${field_name}" ${INDEX})
            list(APPEND ITEMS "${ITEM}")
        endforeach()
    endif()
    set(${output_name} "${ITEMS}" PARENT_SCOPE)
endfunction()

infomap_json_array(INFOMAP_COMPILE_FLAGS cmake_compile_flags)
infomap_json_array(INFOMAP_LINK_FLAGS link_flags)
infomap_json_array(INFOMAP_ENABLED_FEATURE_DEFINES enabled_feature_defines)

find_program(CCACHE ccache)
if(CCACHE)
    message("-- Found Ccache: TRUE")
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
else()
    message("-- Found Ccache: FALSE")
endif()

# GCC + LTO: plain `ar`/`ranlib` strip the LTO bitcode from object files,
# leaving an empty/broken static archive. Prefer the LTO-aware wrappers
# (gcc-ar/gcc-ranlib) when both are in play. clang's archiver handles LTO
# bitcode natively, so no override is needed there. Mirror of the mk/common.mk
# detection so the CMake build path is consistent with the Makefile build path.
if(INFOMAP_NATIVE_ARCH AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    find_program(INFOMAP_GCC_AR gcc-ar)
    find_program(INFOMAP_GCC_RANLIB gcc-ranlib)
    if(INFOMAP_GCC_AR AND INFOMAP_GCC_RANLIB)
        set(CMAKE_AR "${INFOMAP_GCC_AR}")
        set(CMAKE_RANLIB "${INFOMAP_GCC_RANLIB}")
        message(STATUS "INFOMAP_NATIVE_ARCH: using ${INFOMAP_GCC_AR} / ${INFOMAP_GCC_RANLIB} for LTO-safe static archives")
    else()
        message(WARNING "INFOMAP_NATIVE_ARCH=ON with GCC but gcc-ar / gcc-ranlib not found in PATH. The static infomap_core archive may end up empty under -flto.")
    endif()
endif()

file(GLOB_RECURSE infomap_headers CONFIGURE_DEPENDS "src/*.h")
file(GLOB_RECURSE infomap_sources CONFIGURE_DEPENDS "src/*.cpp")
set(infomap_main_source "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM infomap_sources "${infomap_main_source}")

add_library(infomap_core ${infomap_headers} ${infomap_sources})
target_include_directories(infomap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(infomap_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann_json/include")
# fmt is included from headers under src/ (utils/format.h), so it must be PUBLIC
# to propagate to the CLI, benchmark, and cpp-test targets that consume src/.
target_include_directories(infomap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/fmt/include")
target_compile_features(infomap_core PUBLIC cxx_std_17)
set_target_properties(infomap_core PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(infomap_core PRIVATE ${INFOMAP_COMPILE_FLAGS})
target_compile_definitions(infomap_core PUBLIC ${INFOMAP_ENABLED_FEATURE_DEFINES})
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
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann_json/include"
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
                "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann_json/include"
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
    add_infomap_cpp_test(infomap_cpp_utils_io_tests test/cpp/test_utils_io.cpp "fast;core;utils;io")
    add_infomap_cpp_test(infomap_cpp_program_interface_tests test/cpp/test_program_interface.cpp "fast;core;parser;cli")
    add_infomap_cpp_test(infomap_cpp_config_tests test/cpp/test_config.cpp "fast;core;config;cli")
    add_infomap_cpp_test(infomap_cpp_thread_config_tests test/cpp/test_thread_config.cpp "fast;core;threads")
    add_infomap_cpp_test(infomap_cpp_no_output_tests test/cpp/test_no_output.cpp "fast;core;output;library")
    add_infomap_cpp_test(infomap_cpp_trial_results_tests test/cpp/test_trial_results.cpp "fast;core;merge")
    add_infomap_cpp_test(infomap_cpp_output_view_tests test/cpp/test_output_view.cpp "fast;core;output")
    add_infomap_cpp_test(infomap_cpp_object_pool_tests test/cpp/test_object_pool.cpp "fast;core;memory")
    add_infomap_cpp_test(infomap_cpp_lossy_tests test/cpp/test_lossy.cpp "fast;core;lossy")
    add_infomap_cpp_test(infomap_cpp_preferred_levels_tests test/cpp/test_preferred_levels.cpp "fast;core;preferred-levels")
    add_infomap_cpp_test(infomap_cpp_random_tests test/cpp/test_random.cpp "fast;core;rng")
    add_infomap_cpp_test(infomap_cpp_map_equation_invariants_tests test/cpp/test_map_equation_invariants.cpp "fast;core;mapeq")

    add_test(
        NAME print_json_parameters
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_print_json_parameters.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(print_json_parameters PROPERTIES LABELS "fast;core;parser;cli")

    add_test(
        NAME binding_defaults
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_binding_defaults.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(binding_defaults PROPERTIES LABELS "fast;core;parser;cli")

    add_test(
        NAME shell_completion
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_shell_completion.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(shell_completion PROPERTIES LABELS "fast;core;parser;cli")

    add_test(
        NAME help_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_help_contract.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(help_contract PROPERTIES LABELS "fast;core;parser;cli")

    add_test(
        NAME pretty_output
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_pretty_output.py"
            "$<TARGET_FILE:infomap_cli>"
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_tests_properties(pretty_output PROPERTIES LABELS "fast;core;output;cli")

    add_test(
        NAME run_metadata_cli
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_run_metadata_cli.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(run_metadata_cli PROPERTIES LABELS "fast;core;output;cli")

    add_test(
        NAME run_reports_cli
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_run_reports_cli.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(run_reports_cli PROPERTIES LABELS "fast;core;output;cli")

    add_test(
        NAME trial_results_cli
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_trial_results_cli.py"
            "$<TARGET_FILE:infomap_cli>"
    )
    set_tests_properties(trial_results_cli PROPERTIES LABELS "fast;core;merge;cli")

    add_test(
        NAME js_output_format_manifest
        COMMAND
            "${CMAKE_COMMAND}" -E env "CXX=${CMAKE_CXX_COMPILER}"
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/scripts/check_js_output_formats.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/io/OutputFormats.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/interfaces/js/generated/output-formats.json"
    )
    set_tests_properties(js_output_format_manifest PROPERTIES LABELS "fast;core;output;js")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(infomap_cpp_lifecycle_tests PRIVATE -fno-access-control)
        target_compile_options(infomap_cpp_partition_tests PRIVATE -fno-access-control)
        # Reaches InfomapBase::calcCodelengthOnTree (private) to recompute a
        # partition from scratch and compare against the tracked codelength.
        target_compile_options(infomap_cpp_map_equation_invariants_tests PRIVATE -fno-access-control)
    endif()

    add_custom_target(
        infomap_cpp_tests
        DEPENDS
            infomap_cpp_network_tests
            infomap_cpp_lifecycle_tests
            infomap_cpp_partition_tests
            infomap_cpp_flow_tests
            infomap_cpp_utils_io_tests
            infomap_cpp_program_interface_tests
            infomap_cpp_config_tests
            infomap_cpp_thread_config_tests
            infomap_cpp_no_output_tests
            infomap_cpp_output_view_tests
            infomap_cpp_trial_results_tests
            infomap_cpp_object_pool_tests
            infomap_cpp_lossy_tests
            infomap_cpp_preferred_levels_tests
            infomap_cpp_random_tests
            infomap_cpp_map_equation_invariants_tests
            infomap_cli
    )
endif()
