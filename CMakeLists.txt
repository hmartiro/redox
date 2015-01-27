cmake_minimum_required(VERSION 2.8.4)
project(redox)

set(REDOX_VERSION_MAJOR 0)
set(REDOX_VERSION_MINOR 2)
set(REDOX_VERSION_PATCH 0)
set(REDOX_VERSION_STRING ${REDOX_VERSION_MAJOR}.${REDOX_VERSION_MINOR}.${REDOX_VERSION_PATCH})

option(lib "Build Redox as a dynamic library." ON)
option(static_lib "Build Redox as a static library." OFF)
option(tests "Build all tests." OFF)
option(examples "Build all examples." OFF)

# Use Release if no configuration specified
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
set(INC_DIR ${CMAKE_SOURCE_DIR}/include/redox)

set(SRC_CORE
  ${SRC_DIR}/client.cpp
  ${SRC_DIR}/command.cpp
  ${SRC_DIR}/subscriber.cpp)

set(INC_CORE
    ${INC_DIR}/client.hpp
    ${INC_DIR}/subscriber.hpp
    ${INC_DIR}/command.hpp)

set(SRC_UTILS ${SRC_DIR}/utils/logger.cpp)
set(INC_UTILS ${INC_DIR}/utils/logger.hpp)

set(INC_WRAPPER ${CMAKE_SOURCE_DIR}/include/redox.hpp)

set(SRC_ALL ${SRC_CORE} ${SRC_UTILS})
set(INC_ALL ${INC_CORE} ${INC_UTILS} ${INC_WRAPPER})

include_directories(${INC_DIR} ${INC_DIR/redox})

# Dependent libraries - you may have to change
# pthread to whatever C++11 threads depends on
# for your platform
set(REDOX_LIB_DEPS ev pthread hiredis)

# ---------------------------------------------------------
# Library generation
# ---------------------------------------------------------

if (lib)

  add_library(redox SHARED ${SRC_ALL} ${INC_CORE})
  target_link_libraries(redox ${REDOX_LIB_DEPS})

  set_target_properties(redox
    PROPERTIES VERSION ${REDOX_VERSION_STRING}
    SOVERSION ${REDOX_VERSION_MAJOR})

endif()

if (static_lib)

  add_library(redox_static STATIC ${SRC_ALL})
  target_link_libraries(redox_static ${REDOX_LIB_DEPS})

  set_target_properties(redox_static
    PROPERTIES VERSION ${REDOX_VERSION_STRING}
    SOVERSION ${REDOX_VERSION_MAJOR})

endif()

# ---------------------------------------------------------
# Test suite
# ---------------------------------------------------------
if (tests)

  enable_testing()
  find_package(GTest REQUIRED)

  add_executable(test_redox test/test.cpp)

  target_include_directories(test_redox PUBLIC ${GTEST_INCLUDE_DIRS})
  target_link_libraries(test_redox redox gtest)

  # So that we can run 'make test'
  add_test(test_redox test_redox)

endif()

# ---------------------------------------------------------
# Examples
# ---------------------------------------------------------
if (examples)

  add_executable(basic examples/basic.cpp)
  target_link_libraries(basic redox)

  add_executable(basic_threaded examples/basic_threaded.cpp)
  target_link_libraries(basic_threaded redox)

  add_executable(lpush_benchmark examples/lpush_benchmark.cpp)
  target_link_libraries(lpush_benchmark redox)

  add_executable(speed_test_async examples/speed_test_async.cpp)
  target_link_libraries(speed_test_async redox)

  add_executable(speed_test_sync examples/speed_test_sync.cpp)
  target_link_libraries(speed_test_sync redox)

  add_executable(speed_test_async_multi examples/speed_test_async_multi.cpp)
  target_link_libraries(speed_test_async_multi redox)

  add_executable(data_types examples/data_types.cpp)
  target_link_libraries(data_types redox)

  add_executable(multi_client examples/multi-client.cpp)
  target_link_libraries(multi_client redox)

  add_executable(binary_data examples/binary_data.cpp)
  target_link_libraries(binary_data redox)

  add_executable(pub_sub examples/pub_sub.cpp)
  target_link_libraries(pub_sub redox)

  add_executable(speed_test_pubsub examples/speed_test_pubsub ${SRC_ALL})
  target_link_libraries(speed_test_pubsub redox)

  add_custom_target(examples)
  add_dependencies(examples
    basic basic_threaded lpush_benchmark speed_test_async speed_test_sync
    speed_test_async_multi data_types multi_client binary_data pub_sub
    speed_test_pubsub
  )

endif()

# ---------------------------------------------------------
# Install (sudo make install)
# ---------------------------------------------------------

set(CMAKE_INSTALL_PREFIX /usr/)

# Install the dynamic library to /usr/lib
install(TARGETS redox DESTINATION lib)

# Install the headers into /usr/include/redox
install(FILES ${INC_CORE} DESTINATION include/redox)
install(FILES ${INC_UTILS} DESTINATION include/redox/utils)

# Install the top-level header directly into /usr/include
install(FILES ${INC_WRAPPER} DESTINATION include)

# ---------------------------------------------------------
# Create system package (make package)
# ---------------------------------------------------------

# build a CPack driven installer package
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++11 client for Redis")
set(CPACK_PACKAGE_VENDOR "Hayk Martirosyan")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Hayk Martirosyan <hayk.mart@gmail.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)")
set(CPACK_PACKAGE_VERSION_MAJOR "${REDOX_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${REDOX_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${REDOX_VERSION_PATCH}")
set(CPACK_GENERATOR "DEB")
include(CPack)
