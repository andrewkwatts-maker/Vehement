# =============================================================================
# Old3DEngine Test Suite Configuration
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# =============================================================================
# GoogleTest Setup via FetchContent
# =============================================================================

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# Prevent GoogleTest from overriding compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# =============================================================================
# Google Benchmark for Performance Tests
# =============================================================================

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
    GIT_SHALLOW    TRUE
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

# =============================================================================
# Test Categories
# =============================================================================

# Enable CTest
enable_testing()
include(GoogleTest)

# Coverage reporting setup (optional, requires gcov/lcov)
option(NOVA_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(NOVA_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        set(COVERAGE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}")
    endif()
endif()

# =============================================================================
# Common Test Library
# =============================================================================

add_library(test_common INTERFACE)
target_include_directories(test_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_link_libraries(test_common INTERFACE
    GTest::gtest
    GTest::gmock
    nova3d
)

# =============================================================================
# Unit Tests - Engine Systems
# =============================================================================

set(ENGINE_TEST_SOURCES
    test_main.cpp
    engine/test_spatial.cpp
    engine/test_animation.cpp
    engine/test_reflection.cpp
    engine/test_scripting.cpp
    engine/test_physics.cpp
    engine/test_pool.cpp
    engine/test_job_system.cpp
)

add_executable(nova_unit_tests ${ENGINE_TEST_SOURCES})
target_link_libraries(nova_unit_tests PRIVATE
    test_common
    GTest::gtest_main
)
target_compile_definitions(nova_unit_tests PRIVATE
    NOVA_TESTING
    NOVA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

# Register unit tests with CTest
gtest_discover_tests(nova_unit_tests
    PROPERTIES
        LABELS "unit"
        TIMEOUT 60
)

# =============================================================================
# Unit Tests - Game Systems
# =============================================================================

set(GAME_TEST_SOURCES
    test_main.cpp
    game/test_config.cpp
    game/test_entities.cpp
    game/test_combat.cpp
    game/test_tech_tree.cpp
    game/test_spells.cpp
    game/test_replication.cpp
)

add_executable(nova_game_tests ${GAME_TEST_SOURCES})
target_link_libraries(nova_game_tests PRIVATE
    test_common
    GTest::gtest_main
)
target_include_directories(nova_game_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/game/src
)
target_compile_definitions(nova_game_tests PRIVATE
    NOVA_TESTING
    NOVA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

gtest_discover_tests(nova_game_tests
    PROPERTIES
        LABELS "game;unit"
        TIMEOUT 60
)

# =============================================================================
# Integration Tests
# =============================================================================

set(INTEGRATION_TEST_SOURCES
    test_main.cpp
    integration/test_editor_flow.cpp
    integration/test_game_loop.cpp
)

add_executable(nova_integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(nova_integration_tests PRIVATE
    test_common
    GTest::gtest_main
)
target_include_directories(nova_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/game/src
)
target_compile_definitions(nova_integration_tests PRIVATE
    NOVA_TESTING
    NOVA_INTEGRATION_TEST
    NOVA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

gtest_discover_tests(nova_integration_tests
    PROPERTIES
        LABELS "integration"
        TIMEOUT 120
)

# =============================================================================
# Performance Benchmarks
# =============================================================================

set(BENCHMARK_SOURCES
    benchmark/bench_spatial.cpp
    benchmark/bench_animation.cpp
    benchmark/bench_serialization.cpp
)

add_executable(nova_benchmarks ${BENCHMARK_SOURCES})
target_link_libraries(nova_benchmarks PRIVATE
    nova3d
    benchmark::benchmark
    benchmark::benchmark_main
)
target_compile_definitions(nova_benchmarks PRIVATE
    NOVA_BENCHMARK
)

# =============================================================================
# Custom Test Targets
# =============================================================================

# Run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS nova_unit_tests nova_game_tests nova_integration_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests"
)

# Run only unit tests
add_custom_target(run_unit_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS nova_unit_tests nova_game_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running unit tests"
)

# Run only integration tests
add_custom_target(run_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L integration --output-on-failure
    DEPENDS nova_integration_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running integration tests"
)

# Run benchmarks
add_custom_target(run_benchmarks
    COMMAND nova_benchmarks
    DEPENDS nova_benchmarks
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running benchmarks"
)

# =============================================================================
# Coverage Report Generation
# =============================================================================

if(NOVA_ENABLE_COVERAGE)
    find_program(LCOV lcov)
    find_program(GENHTML genhtml)

    if(LCOV AND GENHTML)
        add_custom_target(coverage
            COMMAND ${LCOV} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/googletest/*' '*/benchmark/*' --output-file coverage.info
            COMMAND ${GENHTML} coverage.info --output-directory coverage_report
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in coverage_report/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS run_tests
            COMMENT "Generating coverage report"
        )
    else()
        message(WARNING "lcov/genhtml not found - coverage reporting disabled")
    endif()
endif()

message(STATUS "Nova3D Test Suite configured")
message(STATUS "  Unit tests:        nova_unit_tests")
message(STATUS "  Game tests:        nova_game_tests")
message(STATUS "  Integration tests: nova_integration_tests")
message(STATUS "  Benchmarks:        nova_benchmarks")
