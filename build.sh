#!/bin/bash

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
  mkdir build
fi

cd build

# Run CMake to configure the project
cmake ..

# Build the project
make

echo "Build complete. Executable is located at build/rv_demo"
