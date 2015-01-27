cmake_minimum_required(VERSION 2.8.4)
project(redox)

option(library "Build Redox as a library." ON)
option(tests "Build all tests." ON)
option(examples "Build all examples." ON)

# Use RelWithDebInfo if no configuration specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall")

# Print out compiler commands
# set(CMAKE_VERBOSE_MAKEFILE ON)

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(SRC_CORE
  ${SRC_DIR}/client.cpp
  ${SRC_DIR}/command.cpp
  ${SRC_DIR}/subscriber.cpp
)

set(SRC_LOGGER ${SRC_DIR}/utils/logger.cpp)

set(SRC_ALL ${SRC_CORE} ${SRC_LOGGER})

include_directories(${SRC_DIR})

# ---------------------------------------------------------
# Linked libraries
# ---------------------------------------------------------

set(LIB_REDOX redox hiredis ev pthread)

# ---------------------------------------------------------
# Library generation
# ---------------------------------------------------------

if (library)

  add_library(redox SHARED ${SRC_ALL})
  add_library(redox_static STATIC ${SRC_ALL})

endif()

# ---------------------------------------------------------
# Test suite
# ---------------------------------------------------------
if (tests)

  enable_testing()
  find_package(GTest REQUIRED)

  add_executable(test_redox test/test.cpp)

  target_include_directories(test_redox PUBLIC ${GTEST_INCLUDE_DIRS})
  target_link_libraries(test_redox ${LIB_REDOX} gtest)

  # So that we can run 'make test'
  add_test(test_redox test_redox)

endif()

# ---------------------------------------------------------
# Examples
# ---------------------------------------------------------
if (examples)

  add_executable(basic examples/basic.cpp)
  target_link_libraries(basic ${LIB_REDOX})

  add_executable(basic_threaded examples/basic_threaded.cpp)
  target_link_libraries(basic_threaded ${LIB_REDOX})

  add_executable(lpush_benchmark examples/lpush_benchmark.cpp)
  target_link_libraries(lpush_benchmark ${LIB_REDOX})

  add_executable(speed_test_async examples/speed_test_async.cpp)
  target_link_libraries(speed_test_async ${LIB_REDOX})

  add_executable(speed_test_sync examples/speed_test_sync.cpp)
  target_link_libraries(speed_test_sync ${LIB_REDOX})

  add_executable(speed_test_async_multi examples/speed_test_async_multi.cpp)
  target_link_libraries(speed_test_async_multi ${LIB_REDOX})

  add_executable(data_types examples/data_types.cpp)
  target_link_libraries(data_types ${LIB_REDOX})

  add_executable(multi_client examples/multi-client.cpp)
  target_link_libraries(multi_client ${LIB_REDOX})

  add_executable(binary_data examples/binary_data.cpp)
  target_link_libraries(binary_data ${LIB_REDOX})

  add_executable(pub_sub examples/pub_sub.cpp)
  target_link_libraries(pub_sub ${LIB_REDOX})

  add_executable(speed_test_pubsub examples/speed_test_pubsub ${SRC_ALL})
  target_link_libraries(speed_test_pubsub ${LIB_REDOX})

endif()
