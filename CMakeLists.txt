cmake_minimum_required(VERSION 2.8.4)
project(redox)

set(REDOX_VERSION_MAJOR 0)
set(REDOX_VERSION_MINOR 3)
set(REDOX_VERSION_PATCH 0)
set(REDOX_VERSION_STRING ${REDOX_VERSION_MAJOR}.${REDOX_VERSION_MINOR}.${REDOX_VERSION_PATCH})

option(lib "Build Redox as a dynamic library." ON)
option(static_lib "Build Redox as a static library." ON)
option(tests "Build all tests." OFF)
option(examples "Build all examples." OFF)

# Use Release if no configuration specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall")
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  message("Found ccache ${CCACHE_FOUND}")
  message("Using ccache to speed up compilation")
  set(ENV{CCACHE_CPP2} "yes")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# Print out compiler commands
# set(CMAKE_VERBOSE_MAKEFILE ON)

# ---------------------------------------------------------
# Check for required dependencies
# ---------------------------------------------------------
if(CMAKE_VERSION VERSION_GREATER 2.8.5 OR CMAKE_VERSION VERSION_EQUAL 2.8.5)
  include(GNUInstallDirs)
endif()

find_package(Threads REQUIRED)
find_package(hiredis REQUIRED)
find_package(libev REQUIRED)

set(REDOX_LIB_INCLUDES
  ${HIREDIS_INCLUDE_DIRS}
  ${LIBEV_INCLUDE_DIRS})

set(REDOX_LIB_DEPS
  ${HIREDIS_LIBRARIES}
  ${LIBEV_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------

set(SRC_REDOX_DIR ${PROJECT_SOURCE_DIR}/src)
set(INC_REDOX_DIR ${PROJECT_SOURCE_DIR}/include)

set(SRC_REDOX_CORE
  ${SRC_REDOX_DIR}/client.cpp
  ${SRC_REDOX_DIR}/command.cpp
  ${SRC_REDOX_DIR}/subscriber.cpp)

set(INC_REDOX_CORE
    ${INC_REDOX_DIR}/redox/client.hpp
    ${INC_REDOX_DIR}/redox/subscriber.hpp
    ${INC_REDOX_DIR}/redox/command.hpp)

set(SRC_REDOX_UTILS ${SRC_REDOX_DIR}/utils/logger.cpp)
set(INC_REDOX_UTILS ${INC_REDOX_DIR}/redox/utils/logger.hpp)

set(INC_REDOX_WRAPPER ${INC_REDOX_DIR}/redox.hpp)

set(SRC_REDOX_ALL ${SRC_REDOX_CORE} ${SRC_REDOX_UTILS})
set(INC_REDOX_ALL ${INC_REDOX_CORE} ${INC_REDOX_UTILS} ${INC_REDOX_WRAPPER})

include_directories(${REDOX_LIB_INCLUDES})
include_directories(${INC_REDOX_DIR})
include_directories(${INC_REDOX_DIR}/redox)

# ---------------------------------------------------------
# Library generation
# ---------------------------------------------------------

if (lib)

  add_library(redox SHARED ${SRC_REDOX_ALL} ${INC_REDOX_CORE})
  target_link_libraries(redox ${REDOX_LIB_DEPS})

  set_target_properties(redox
    PROPERTIES VERSION ${REDOX_VERSION_STRING}
    SOVERSION ${REDOX_VERSION_MAJOR})

endif()

if (static_lib)

  add_library(redox_static STATIC ${SRC_REDOX_ALL})
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
  target_link_libraries(test_redox redox ${GTEST_BOTH_LIBRARIES})

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

  add_executable(binary_data_publish examples/binary_data_publish.cpp)
  target_link_libraries(binary_data_publish redox)

  add_executable(pub_sub examples/pub_sub.cpp)
  target_link_libraries(pub_sub redox)

  add_executable(speed_test_pubsub examples/speed_test_pubsub.cpp)
  target_link_libraries(speed_test_pubsub redox)

  add_executable(jitter_test examples/jitter_test.cpp)
  target_link_libraries(jitter_test redox)

  add_custom_target(examples)
  add_dependencies(examples
    basic basic_threaded lpush_benchmark speed_test_async speed_test_sync
    speed_test_async_multi data_types multi_client binary_data pub_sub
    speed_test_pubsub jitter_test
  )

endif()

# ---------------------------------------------------------
# Install (sudo make install)
# ---------------------------------------------------------

if(NOT CMAKE_INSTALL_PREFIX)
   set(CMAKE_INSTALL_PREFIX /usr/)
endif(NOT CMAKE_INSTALL_PREFIX)

# Check if it is a 64 bit system
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT LIB_SUFFIX)
    set(LIB_SUFFIX "64")
endif()

if (lib)
  # Install the dynamic library to /usr/lib[64]
  install(TARGETS redox DESTINATION lib${LIB_SUFFIX})
endif()

if (static_lib)
  # Install the static library to /usr/lib[64]
  install(TARGETS redox_static DESTINATION lib${LIB_SUFFIX})
endif()

# Install the headers into /usr/include/redox
install(FILES ${INC_REDOX_CORE} DESTINATION include/redox)
install(FILES ${INC_REDOX_UTILS} DESTINATION include/redox/utils)

# Install the top-level header directly into /usr/include
install(FILES ${INC_REDOX_WRAPPER} DESTINATION include)

# ---------------------------------------------------------
# Create system package (make package)
# ---------------------------------------------------------

# Determine build architecture
execute_process(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE)
message("Building for ${ARCHITECTURE}")

# build CPack driven installer packages
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++11 client for Redis")
set(CPACK_PACKAGE_VENDOR "Hayk Martirosyan")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)")
set(CPACK_PACKAGE_VERSION_MAJOR "${REDOX_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${REDOX_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${REDOX_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_RELEASE "1") # Increase this if a failed build was published
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${CPACK_PACKAGE_VERSION_RELEASE}")
set(CPACK_SYSTEM_NAME "${ARCHITECTURE}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}.${ARCHITECTURE}")
# Debian specific fields
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Hayk Martirosyan <hayk.mart@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${ARCHITECTURE}")
# RPM specific fields
set(CPACK_RPM_PACKAGE_ARCHITECTURE "${ARCHITECTURE}")
# Select CPack generators
set(CPACK_GENERATOR "DEB" "RPM")
include(CPack)
