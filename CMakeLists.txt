cmake_minimum_required(VERSION 2.8.4)
project(redisx)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall -g  -fno-omit-frame-pointer")
# set(CMAKE_VERBOSE_MAKEFILE ON)

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(SRC_CORE
  ${SRC_DIR}/redisx.cpp
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
