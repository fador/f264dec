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

# Source files
file(GLOB_RECURSE F264DEC_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM F264DEC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Core decoder static library
add_library(f264dec_lib STATIC ${F264DEC_SOURCES})
target_include_directories(f264dec_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Main executable
add_executable(f264dec src/main.cpp)
target_link_libraries(f264dec PRIVATE f264dec_lib)

# Memory API test executable
add_executable(test_memory_api tests/test_memory_api.cpp)
target_link_libraries(test_memory_api PRIVATE f264dec_lib)

if(UNIX AND NOT APPLE)
  target_link_libraries(f264dec_lib PUBLIC m)
endif()

# Threading support (ready for threadpool)
find_package(Threads REQUIRED)
target_link_libraries(f264dec_lib PUBLIC Threads::Threads)

# CTest integration
include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams")
  enable_testing()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(
    NAME regression_test
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.py"
            --decoder "$<TARGET_FILE:f264dec>"
            --streams-dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams"
  )
  add_test(
    NAME memory_api_test
    COMMAND test_memory_api "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams/base_cavlc_slices.264" "${CMAKE_CURRENT_SOURCE_DIR}/tests/streams/base_cavlc_slices.md5"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()
