#!/bin/bash
set -e

CXX=${CXX:-g++}
STANDARD="-std=c++17"
FLAGS="-Wall -Wextra -Wpedantic -O2"
SOURCES="Main.cpp task.cpp TaskScheduler.cpp"
OUTPUT="Executable"

echo "Building with: $CXX $STANDARD $FLAGS"
$CXX $STANDARD $FLAGS $SOURCES -lpthread -o $OUTPUT
echo "Build successful -> ./$OUTPUT"
