cmake_minimum_required(VERSION 3.27)

project(onek-cpp-parser-tests LANGUAGES CXX)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API aa1f7df0-828a-4fcd-9afc-2dc80491aca7)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_MAP_FORMAT "clang")

set (Boost_USE_STATIC_LIBS OFF)
find_package (Boost REQUIRED COMPONENTS unit_test_framework)
include_directories (${Boost_INCLUDE_DIRS})

add_executable(onek-cpp-parser-tests calculator.cpp)
target_link_libraries (onek-cpp-parser-tests ${Boost_LIBRARIES} onek-cpp-parser-lib)
