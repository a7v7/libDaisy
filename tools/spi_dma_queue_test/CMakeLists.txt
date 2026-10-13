cmake_minimum_required(VERSION 3.20)

# Builds against a libDaisy checkout - by default the one this folder lives
# in. Point LIBDAISY_DIR at another checkout (e.g. a worktree of a
# branch with the fix) to build the "after" firmware.
set(LIBDAISY_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "libDaisy checkout")

# The toolchain must be chosen before project()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    include(${LIBDAISY_DIR}/cmake/toolchains/autodetect.cmake)
endif()

project(spi_dma_queue_test C CXX ASM)

add_subdirectory(${LIBDAISY_DIR} libDaisy)

set(FIRMWARE_NAME spi_dma_queue_test)
set(FIRMWARE_SOURCES main.cpp)
include(DaisyProject)
