cmake_minimum_required(VERSION 3.1)

project(csconnector)

set(THRIFT_EXECUTOR_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(THRIFT_EXECUTOR_GENERATED_FILES
  ${THRIFT_GEN_DIR}/API.cpp
  ${THRIFT_GEN_DIR}/api_constants.cpp
  ${THRIFT_GEN_DIR}/api_types.cpp
  ${THRIFT_GEN_DIR}/API.h
  ${THRIFT_GEN_DIR}/api_constants.h
  ${THRIFT_GEN_DIR}/api_types.h
  )
add_custom_command(
  OUTPUT ${THRIFT_EXECUTOR_GENERATED_FILES}
  COMMAND thrift-compiler -r -gen cpp:no_skeleton,pure_enums,moveable_types
    -out ${THRIFT_EXECUTOR_GEN_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/api.thrift
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/api.thrift
  )

add_library(csconnector
    include/csstats.h
    src/csstats.cpp
    include/csconnector/csconnector.h
    src/csconnector.cpp
    src/APIHandler.cpp
    include/APIHandler.h
    include/DebugLog.h
    )

target_link_libraries (csconnector PUBLIC csdb csnode csconnector_gen csconnector_executor_gen)


target_include_directories(csconnector
  PUBLIC include
  PRIVATE src
)

set_property(TARGET csconnector PROPERTY CXX_STANDARD 14)
set_property(TARGET csconnector PROPERTY CMAKE_CXX_STANDARD_REQUIRED ON)
