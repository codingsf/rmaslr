cmake_minimum_required(VERSION 2.8.7)
project(rmaslr C CXX)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to Debug")
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++14 -stdlib=libc++ ")

add_executable(rmaslr main.cpp)
target_link_libraries(rmaslr "-framework CoreFoundation")
