cmake_minimum_required(VERSION 3.20)

# Wrap compiles when ccache is on PATH. Honor an explicit launcher from the
# command line or cache so sccache and similar stay in control.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(alpha-centauri VERSION 0.1 LANGUAGES C CXX)

# Link with mold when installed; GNU ld takes ~15s on the debug test binary.
find_program(MOLD_PROGRAM mold)
if(MOLD_PROGRAM AND NOT CMAKE_LINKER_TYPE)
    message(STATUS "Using mold: ${MOLD_PROGRAM}")
    set(CMAKE_LINKER_TYPE MOLD)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
