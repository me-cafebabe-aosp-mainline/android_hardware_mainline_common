#!/bin/bash
set -e

echo "Building test_hwdb..."
mkdir -p build
cd build
cmake ..
make

echo "Running test_hwdb..."
./test_hwdb
