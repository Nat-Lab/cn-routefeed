cmake_minimum_required (VERSION 2.6)

project (routefeed)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

find_path(LIBBGP_INCLUDE_DIR NAMES libbgp/bgp-fsm.h)
include_directories("${LIBBGP_INCLUDE_DIR}" "${PROJECT_SOURCE_DIR}")

add_executable(routefeed feeder.cc fetcher.cc routefeed.cc)
find_library(LIBBGP_LIBRARY bgp)
target_link_libraries(routefeed curl pthread "${LIBBGP_LIBRARY}")

install(TARGETS routefeed DESTINATION bin)
