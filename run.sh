#!/bin/bash

# POC Engine runner script
# Automatically sets up library paths and runs the basic example

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

cd deps/podi
make -Bj
cd "$SCRIPT_DIR"
make -Bj

# Build if needed
if [ ! -f "examples/basic" ]; then
    echo "Building POC Engine..."
    make
fi

# Run with proper library path
echo "Running POC Engine basic example..."
LD_LIBRARY_PATH="deps/podi/lib:$LD_LIBRARY_PATH" ./examples/basic
