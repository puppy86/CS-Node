cmake_minimum_required(VERSION 3.4)

project(net)

add_library(net
  include/net/Hash.hpp
  include/net/Logger.hpp
  include/net/Packet.hpp
  include/net/Structures.hpp
  include/net/SessionIO.hpp
  src/SessionIO.cpp
  )

target_compile_features(net PRIVATE cxx_std_14)

target_include_directories(net
        INTERFACE include
)

set_property(TARGET net PROPERTY CMAKE_CXX_STANDARD_REQUIRED ON)
target_link_libraries(net Solver csnode)

set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_LIBS ON)
set (Boost_USE_STATIC_RUNTIME ON)

find_package (Boost REQUIRED COMPONENTS system filesystem)
target_link_libraries (net Boost::system Boost::filesystem Boost::disable_autolinking)
