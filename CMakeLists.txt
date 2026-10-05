cmake_minimum_required(VERSION 3.20)
project(f264dec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform & compiler specific flags
if(MSVC)
  add_compile_options(/W3 /O2 /MP /utf-8 /permissive)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
  add_compile_options(-Wall -O3 -fno-strict-aliasing)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Compatibility and silence harmless legacy warnings during migration
    add_compile_options(-fpermissive -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-sign-compare)
  endif()
endif()

# Top-level detection
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(F264DEC_IS_TOP_LEVEL ON)
else()
  set(F264DEC_IS_TOP_LEVEL OFF)
endif()

option(F264DEC_BUILD_CLI "Build the f264dec command-line executable" ${F264DEC_IS_TOP_LEVEL})
option(F264DEC_BUILD_TESTS "Build tests for f264dec" ${F264DEC_IS_TOP_LEVEL})

# Source files
file(GLOB_RECURSE F264DEC_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM F264DEC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Core decoder static library
add_library(f264dec_lib STATIC ${F264DEC_SOURCES})
add_library(f264dec::f264dec ALIAS f264dec_lib)
target_include_directories(f264dec_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(UNIX AND NOT APPLE)
  target_link_libraries(f264dec_lib PUBLIC m)
endif()

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(f264dec_lib PUBLIC Threads::Threads)

# Main executable
if(F264DEC_BUILD_CLI)
  add_executable(f264dec src/main.cpp)
  target_link_libraries(f264dec PRIVATE f264dec_lib)
endif()

# Tests
if(F264DEC_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    # Memory API test executable
    add_executable(test_memory_api tests/test_memory_api.cpp)
    target_link_libraries(test_memory_api PRIVATE f264dec_lib)

    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams")
      find_package(Python3 COMPONENTS Interpreter QUIET)
      if(Python3_FOUND AND TARGET f264dec)
        add_test(
          NAME regression_test
          COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.py"
                  --decoder "$<TARGET_FILE:f264dec>"
                  --streams-dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams"
        )
      endif()
      add_test(
        NAME memory_api_test
        COMMAND test_memory_api "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams/base_cavlc_slices.264" "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams/base_cavlc_slices.md5" 8
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      )
      add_test(
        NAME memory_api_high10_test
        COMMAND test_memory_api "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams/high10.264" "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams/high10.md5" 10
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      )
    endif()
  endif()
endif()
