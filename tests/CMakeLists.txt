cmake_minimum_required(VERSION 3.26)
project(onek-cpp-parser-tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (Boost_USE_STATIC_LIBS OFF)
find_package (Boost REQUIRED COMPONENTS unit_test_framework)
include_directories (${Boost_INCLUDE_DIRS})

include_directories(./  ../include )
file(GLOB SOURCES ./*.cpp ./*.h)

add_executable(onek-cpp-parser-tests ${SOURCES})
target_link_libraries (onek-cpp-parser-tests ${Boost_LIBRARIES})
