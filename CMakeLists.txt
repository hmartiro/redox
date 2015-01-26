cmake_minimum_required(VERSION 2.8.4)
project(redox)

option(library "Build Redox as a library." ON)
option(tests "Build all tests." ON)
option(examples "Build all examples." ON)

# Use RelWithDebInfo if no configuration specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall")

# Print out compiler commands
# set(CMAKE_VERBOSE_MAKEFILE ON)

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(SRC_CORE
  ${SRC_DIR}/redox.cpp
  ${SRC_DIR}/command.cpp
)

set(SRC_LOGGER ${SRC_DIR}/utils/logger.cpp)

set(SRC_ALL ${SRC_CORE} ${SRC_LOGGER})

# ---------------------------------------------------------
# Libraries
# ---------------------------------------------------------

set(LIB_REDIS hiredis ev pthread)

# ---------------------------------------------------------
# Test suite
# ---------------------------------------------------------
if (tests)

  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  add_executable(test_redox test/test.cpp ${SRC_ALL})

  target_link_libraries(test_redox ${LIB_REDIS} gtest)

  # So that we can run 'make test'
  add_test(test_redox test_redox)

endif()

# ---------------------------------------------------------
# Examples
# ---------------------------------------------------------
if (examples)

  add_executable(basic examples/basic.cpp ${SRC_ALL})
  target_link_libraries(basic ${LIB_REDIS})

  add_executable(basic_threaded examples/basic_threaded.cpp ${SRC_ALL})
  target_link_libraries(basic_threaded ${LIB_REDIS})

  add_executable(lpush_benchmark examples/lpush_benchmark.cpp ${SRC_ALL})
  target_link_libraries(lpush_benchmark ${LIB_REDIS})

  add_executable(speed_test_async examples/speed_test_async.cpp ${SRC_ALL})
  target_link_libraries(speed_test_async ${LIB_REDIS})

  add_executable(speed_test_sync examples/speed_test_sync.cpp ${SRC_ALL})
  target_link_libraries(speed_test_sync ${LIB_REDIS})

  add_executable(speed_test_async_multi examples/speed_test_async_multi.cpp ${SRC_ALL})
  target_link_libraries(speed_test_async_multi ${LIB_REDIS})

  add_executable(data_types examples/data_types.cpp ${SRC_ALL})
  target_link_libraries(data_types ${LIB_REDIS})

  add_executable(multi_client examples/multi-client.cpp ${SRC_ALL})
  target_link_libraries(multi_client ${LIB_REDIS})

  add_executable(binary_data examples/binary_data.cpp ${SRC_ALL})
  target_link_libraries(binary_data ${LIB_REDIS})

  add_executable(pub_sub examples/pub_sub.cpp ${SRC_ALL})
  target_link_libraries(pub_sub ${LIB_REDIS})

  add_executable(speed_test_pubsub examples/speed_test_pubsub ${SRC_ALL})
  target_link_libraries(speed_test_pubsub ${LIB_REDIS})

endif()
