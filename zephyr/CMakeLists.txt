# MP3 Rewind Media Player - Audio Streaming CMakeLists.txt

cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Project configuration
project(mp3_rewind_audio VERSION 1.0.0)

# Source files for audio streaming system
target_sources(app PRIVATE
    ../src/main.c
    ../src/audio/audiosys.c
    ../src/audio/audioplay_stubs.c
    ../src/audio/bluetooth.c
    ../src/audio/gatt_audio_service.c
    ../src/audio/audio_buffers.c
    ../src/audio/wav_decoder.c
    ../src/server_client/audio_client.c
    ../src/utils/circular_buffers.c
    ../src/utils/error_handling.c
)

# Include directories for header files
target_include_directories(app PRIVATE
    ../src/
    ../src/audio/
    ../src/server_client/
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
    -Wno-double-promotion
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
