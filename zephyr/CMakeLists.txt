# MP3 Rewind Media Player - CMakeLists.txt

cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Project configuration
project(mp3_rewind VERSION 1.0.0)

# Source files - conditionally compile fs.c or sim_fs.c
# Use -DUSE_SIMULATION=ON to build with simulation file system
option(USE_SIMULATION "Use simulation file system instead of real hardware" OFF)

if(USE_SIMULATION)
    message(STATUS "Building with simulation file system")
    target_sources(app PRIVATE
        ../src/main.c
        ../src/storage/sim_fs.c
        ../src/utils/error_handling.c
    )
    target_compile_definitions(app PRIVATE USE_SIMULATION=1)
else()
    message(STATUS "Building with real hardware file system")
    target_sources(app PRIVATE
        ../src/main.c
        ../src/storage/fs.c
        ../src/utils/error_handling.c
    )
endif()

# Include directories for header files
target_include_directories(app PRIVATE
    ../src/
    ../src/storage/
    ../src/utils/
)

# Compile definitions
target_compile_definitions(app PRIVATE
    -DAPP_VERSION_MAJOR=1
    -DAPP_VERSION_MINOR=0
    -DAPP_VERSION_PATCH=0
)

# Compiler flags for better code quality
target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -std=c99
)

# Debug vs Release configuration
if(CONFIG_DEBUG)
    target_compile_options(app PRIVATE -O0 -g3)
    target_compile_definitions(app PRIVATE -DDEBUG=1)
else()
    target_compile_options(app PRIVATE -Os)
    target_compile_definitions(app PRIVATE -DNDEBUG=1)
endif()
