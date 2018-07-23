cmake_minimum_required(VERSION 3.11)

set(THRIFT_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(THRIFT_GENERATED_FILES
    ContractExecutor.cpp
    ContractExecutor.h
    executor_types.cpp
    executor_types.h
    executor_constants.cpp
    executor_constants.h
  )

add_custom_command(
  OUTPUT ${THRIFT_GENERATED_FILES}
  COMMAND thrift-compiler -r -gen cpp:no_skeleton,pure_enums,moveable_types
    -out ${THRIFT_GEN_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../thrift-interface-definitions//executor.thrift
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../thrift-interface-definitions/executor.thrift
  )

add_library(csconnector_executor_gen
    ${THRIFT_GENERATED_FILES}
)

target_link_libraries (csconnector_executor_gen PUBLIC thrift_static)
target_include_directories(csconnector_executor_gen PUBLIC
  ${THRIFT_GEN_DIR})

set_property(TARGET csconnector_executor_gen PROPERTY CXX_STANDARD 14)
set_property(TARGET csconnector_executor_gen PROPERTY CMAKE_CXX_STANDARD_REQUIRED ON)
