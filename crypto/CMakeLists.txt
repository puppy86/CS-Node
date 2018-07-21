cmake_minimum_required (VERSION 2.6)

project (cscrypto)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC
  include/cscrypto/cscrypto.h
  src/cscrypto.cpp
  )

target_link_libraries (${PROJECT_NAME} ed25519 blake2)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include )

# add_subdirectory(dynamic)

# add_subdirectory(tests)

if ( USE_SWIG )
    add_subdirectory(swig)
endif()

option(BUILD_BENCHMARKS "Build benchmarks" OFF)

if ( BUILD_BENCHMARKS )
    add_subdirectory(benchmarks)
endif()
