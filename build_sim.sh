#!/bin/bash
# Build script for simulation mode
# This builds the project using native GCC for testing on macOS

echo "Building MP3 Rewind in simulation mode..."

# Create build directory
mkdir -p build_sim
cd build_sim

# Clean previous build
rm -f mp3_rewind_sim

# Compile with GCC
gcc -o mp3_rewind_sim \
    -I../src \
    -I../src/storage \
    -I../src/utils \
    -DUSE_SIMULATION=1 \
    ../test/sim_main.c \
    ../src/storage/sim_fs.c \
    ../src/utils/sim_error_handling.c \
    -std=c99

if [ $? -eq 0 ]; then
    echo "Build successful! Run with: ./build_sim/mp3_rewind_sim"
    echo "Make sure you're in the project root directory when running."
else
    echo "Build failed!"
    exit 1
fi
