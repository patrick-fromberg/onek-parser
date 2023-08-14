cmake_minimum_required(VERSION 3.27)

#
# !!! ONLY WORKS WITH CLANG FOR NOW !!!
# Will fix when modules are more uniformly supported.
#

project(onek-cpp-parser-lib LANGUAGES CXX)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API aa1f7df0-828a-4fcd-9afc-2dc80491aca7)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file (GLOB SOURCES ./*.ixx)

add_library(onek-cpp-parser-lib)
target_sources(
        onek-cpp-parser-lib
        PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES
        FILES $<PATH:GET_FILENAME,arena_ptr.ixx>
        FILES $<PATH:GET_FILENAME,ast.ixx>
        FILES $<PATH:GET_FILENAME,ast_graph.ixx>
        FILES $<PATH:GET_FILENAME,ast_node.ixx>
        FILES $<PATH:GET_FILENAME,error_messages.ixx>
        FILES $<PATH:GET_FILENAME,parser.ixx>
        FILES $<PATH:GET_FILENAME,parser_combinators.ixx>
        FILES $<PATH:GET_FILENAME,placeholders.ixx>
        FILES $<PATH:GET_FILENAME,scan_state.ixx>
        FILES $<PATH:GET_FILENAME,status_saver_template.ixx>
        FILES $<PATH:GET_FILENAME,token.ixx>
)

