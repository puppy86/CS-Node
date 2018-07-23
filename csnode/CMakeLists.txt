cmake_minimum_required(VERSION 3.10)

project(csnode)

add_library(csnode
	include/csnode/Blockchain.hpp
	include/csnode/Node.hpp
	include/csnode/Packstream.hpp
  	src/Blockchain.cpp
  	src/Node.cpp src/Packstream.cpp)

target_link_libraries (csnode net csdb Solver csconnector)

target_include_directories(csnode PUBLIC
  include
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PROJECT_NAME} PROPERTY CMAKE_CXX_STANDARD_REQUIRED ON)
