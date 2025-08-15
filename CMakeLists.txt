
cmake_minimum_required(VERSION 3.15)
project(instr_bench)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(ExternalProject)

set(LLVM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/llvm-project/llvm)
set(LLVM_BINARY_DIR ${CMAKE_BINARY_DIR}/third_party/llvm-build)

ExternalProject_Add(
  llvm-project-build
  SOURCE_DIR ${LLVM_SOURCE_DIR}
  BINARY_DIR ${LLVM_BINARY_DIR}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS=llvm -DLLVM_TARGETS_TO_BUILD=host -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_RTTI=ON -DCMAKE_INSTALL_PREFIX=${LLVM_BINARY_DIR}/install
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
  BUILD_ALWAYS 0
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_INSTALL ON
)

set(LLVM_DIR ${LLVM_BINARY_DIR}/install/lib/cmake/llvm)
find_package(LLVM REQUIRED PATHS ${LLVM_DIR} NO_DEFAULT_PATH)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Build and install spdlog using ExternalProject_Add
set(SPDLOG_SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/spdlog)
set(SPDLOG_BINARY_DIR ${CMAKE_BINARY_DIR}/third_party/spdlog-build)

ExternalProject_Add(
  spdlog-build
  SOURCE_DIR ${SPDLOG_SOURCE_DIR}
  BINARY_DIR ${SPDLOG_BINARY_DIR}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SPDLOG_BINARY_DIR}/install
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
  BUILD_ALWAYS 0
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_INSTALL ON
)

set(SPDLOG_DIR ${CMAKE_BINARY_DIR}/third_party/spdlog-build/install/lib/cmake/spdlog)
list(PREPEND CMAKE_PREFIX_PATH ${SPDLOG_DIR})
find_package(spdlog REQUIRED PATHS ${LLVM_DIR} NO_DEFAULT_PATH)

add_subdirectory(src)
