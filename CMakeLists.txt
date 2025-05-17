cmake_minimum_required(VERSION 4.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)

SET(CMAKE_C_COMPILER_WORKS TRUE)
SET(CMAKE_CXX_COMPILER_WORKS TRUE)

set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

set(ASM_FLAGS "-x assembler-with-cpp")
set(C_FLAGS "--specs=nosys.specs --specs=nano.specs -nostartfiles -Werror -Wall -Wextra -Wdouble-promotion -Wno-psabi -ffreestanding -fdiagnostics-color=always")
set(CXX_FLAGS "${C_FLAGS} -mcmodel=medany -Wpedantic -Wsuggest-override -fno-rtti -fno-exceptions -fno-threadsafe-statics")
set(DEBUG_FLAGS "-Og -g3 -ggdb3 -DDEBUG")
set(RELEASE_FLAGS "-Og -g3 -ggdb3 -DNDBUG")

if (CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set(C_FLAGS "${C_FLAGS} ${DEBUG_FLAGS}")
    set(CXX_FLAGS "${CXX_FLAGS} ${DEBUG_FLAGS}")
    message(STATUS "Build type: Debug")
else()
    set(C_FLAGS "${C_FLAGS} ${RELEASE_FLAGS}")
    set(CXX_FLAGS "${CXX_FLAGS} ${RELEASE_FLAGS}")
    message(STATUS "Build type: Release")
endif()

set(CMAKE_C_FLAGS "${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-T${CMAKE_SOURCE_DIR}/src/arch/riscv/link.ld \
    -Wl,--print-memory-usage,-Map=ReOS.map,--no-warn-rwx-segments,--gc-sections"
)

set(tool_prefix "riscv64-unknown-elf-")
find_program(TOOLCHAIN_PATH NAMES "${tool_prefix}gcc" REQUIRED)
get_filename_component(TOOLCHAIN_DIR ${TOOLCHAIN_PATH} DIRECTORY)
message(STATUS "Using toolchain from path: ${TOOLCHAIN_DIR}")

set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/${tool_prefix}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/${tool_prefix}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_DIR}/${tool_prefix}gcc)
set(CMAKE_LINKER ${TOOLCHAIN_DIR}/${tool_prefix}gcc)
set(CMAKE_CPP ${TOOLCHAIN_DIR}/${tool_prefix}cpp)
set(CMAKE_SIZE_UTIL ${TOOLCHAIN_DIR}/${tool_prefix}size)
set(CMAKE_OBJCOPY ${TOOLCHAIN_DIR}/${tool_prefix}objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_DIR}/${tool_prefix}objdump)
set(CMAKE_NM_UTIL ${TOOLCHAIN_DIR}/${tool_prefix}gcc-nm)
set(CMAKE_AR ${TOOLCHAIN_DIR}/${tool_prefix}gcc-ar)
set(CMAKE_RANLIB ${TOOLCHAIN_DIR}/${tool_prefix}gcc-ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_DIR}/${tool_prefix}strip)

project("ReOS"
    VERSION 0.0.1
    LANGUAGES C CXX ASM
)

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE source
    ${CMAKE_SOURCE_DIR}/src/*.S
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cc
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.cxx
    ${CMAKE_SOURCE_DIR}/src/*.cppm
)
target_sources(${PROJECT_NAME} PRIVATE ${source})
