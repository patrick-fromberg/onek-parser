cmake_minimum_required(VERSION 3.26)

set (Boost_USE_STATIC_LIBS OFF)
find_package (Boost REQUIRED COMPONENTS unit_test_framework)

file(GLOB SOURCES ./*.cpp )
add_executable(onek-cpp-parser-tests ${SOURCES})

target_link_libraries (onek-cpp-parser-tests ${Boost_LIBRARIES} onek-cpp-parser-lib)

target_compile_options(onek-cpp-parser-tests PRIVATE 
	$<$<CONFIG:Debug>:-g;-O0;-fno-omit-frame-pointer>
	$<$<CONFIG:Asan>:-g;-fsanitize=address;-fno-omit-frame-pointer>
	$<$<CONFIG:Release>:-O3>)

target_link_options(onek-cpp-parser-tests PRIVATE 
	$<$<CONFIG:Asan>:-fsanitize=address>)
