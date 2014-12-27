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

#add_executable(basic examples/basic.cpp ${SRC_ALL})
#target_link_libraries(basic ${LIB_REDIS})

#add_executable(progressive examples/progressive.cpp ${SRC_ALL})
#target_link_libraries(progressive ${LIB_REDIS})

#add_executable(basic_threaded examples/basic_threaded.cpp ${SRC_ALL})
#target_link_libraries(basic_threaded ${LIB_REDIS})

#add_executable(lpush_benchmark examples/lpush_benchmark.cpp ${SRC_ALL})
#target_link_libraries(lpush_benchmark ${LIB_REDIS})

add_executable(simple_loop examples/simple_loop.cpp ${SRC_ALL})
target_link_libraries(simple_loop ${LIB_REDIS})

add_executable(simple_sync_loop examples/simple_sync_loop.cpp ${SRC_ALL})
target_link_libraries(simple_sync_loop ${LIB_REDIS})
