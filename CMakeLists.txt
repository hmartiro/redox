cmake_minimum_required(VERSION 2.8.4)
project(redox)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall -O3")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall -g  -fno-omit-frame-pointer")
# set(CMAKE_VERBOSE_MAKEFILE ON)

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(SRC_CORE
  ${SRC_DIR}/redox.cpp
  ${SRC_DIR}/command.cpp
)

set(SRC_ALL ${SRC_CORE})

# ---------------------------------------------------------
# Libraries
# ---------------------------------------------------------

set(LIB_REDIS hiredis ev pthread)
set(LIB_ALL ${LIB_REDIS})

# ---------------------------------------------------------
# Examples
# ---------------------------------------------------------

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

add_executable(string_v_char examples/string_vs_charp.cpp ${SRC_ALL})
target_link_libraries(string_v_char ${LIB_REDIS})
