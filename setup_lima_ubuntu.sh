#!/bin/bash
#
# setup_lima_ubuntu.sh
# Set up Lima Ubuntu environment for FasterBASIC builds
#
# This script installs all necessary build dependencies and then
# builds both fbsh and fbc for Linux.
#

set -e

echo "=========================================="
echo "FasterBASIC Lima Ubuntu Setup"
echo "=========================================="
echo ""

# Check if running in Lima
if [ ! -f /etc/os-release ]; then
    echo "Error: This doesn't appear to be a Linux system"
    exit 1
fi

source /etc/os-release
if [ "$ID" != "ubuntu" ]; then
    echo "Warning: This script is designed for Ubuntu"
    echo "Detected: $ID $VERSION_ID"
    echo "Continuing anyway..."
fi

echo "Detected: $PRETTY_NAME"
echo ""

# Update package lists
echo "=== Updating package lists ==="
sudo apt-get update

echo ""
echo "=== Installing build dependencies ==="
echo ""

# Install essential build tools
echo "Installing build-essential..."
sudo apt-get install -y build-essential

# Install g++ (should be included but make sure)
echo "Installing g++..."
sudo apt-get install -y g++

# Install pkg-config (needed for checking libraries)
echo "Installing pkg-config..."
sudo apt-get install -y pkg-config

# Install LuaJIT development files
echo "Installing LuaJIT..."
sudo apt-get install -y libluajit-5.1-dev

# Install SQLite3 (needed by fbsh)
echo "Installing SQLite3..."
sudo apt-get install -y libsqlite3-dev

# Install pthread (should be installed but check)
echo "Installing pthread..."
sudo apt-get install -y libpthread-stubs0-dev || true

# Install git if not present (useful for development)
echo "Installing git..."
sudo apt-get install -y git || true

echo ""
echo "=== Verifying installations ==="
echo ""

# Check g++ version
echo -n "g++ version: "
g++ --version | head -1

# Check if LuaJIT headers are available
if [ -d "/usr/include/luajit-2.1" ]; then
    echo "LuaJIT 2.1 headers: ✓ /usr/include/luajit-2.1"
elif [ -d "/usr/include/luajit-2.0" ]; then
    echo "LuaJIT 2.0 headers: ✓ /usr/include/luajit-2.0"
else
    echo "LuaJIT headers: ✗ NOT FOUND"
    exit 1
fi

# Check SQLite3
if [ -f "/usr/include/sqlite3.h" ]; then
    if pkg-config --exists sqlite3 2>/dev/null; then
        echo "SQLite3: ✓ $(pkg-config --modversion sqlite3)"
    else
        echo "SQLite3: ✓ (installed)"
    fi
else
    echo "SQLite3: ✗ NOT FOUND"
    exit 1
fi

echo ""
echo "=== All dependencies installed successfully ==="
echo ""

# Ask if user wants to build now
echo "Would you like to build FasterBASIC now? (y/n)"
read -r response

if [[ "$response" =~ ^[Yy]$ ]]; then
    echo ""
    echo "=== Building FasterBASIC for Linux ==="
    echo ""

    # Navigate to project directory
    SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    cd "$SCRIPT_DIR"

    # Build fbsh
    echo "Building fbsh (FasterBASIC Shell)..."
    if [ -f "./rebuild_fbsh_linux.sh" ]; then
        ./rebuild_fbsh_linux.sh
    else
        echo "Error: rebuild_fbsh_linux.sh not found"
        exit 1
    fi

    echo ""
    echo "Building fbc (FasterBASIC Compiler)..."
    if [ -f "./rebuild_fbc_linux.sh" ]; then
        ./rebuild_fbc_linux.sh
    else
        echo "Error: rebuild_fbc_linux.sh not found"
        exit 1
    fi

    echo ""
    echo "=========================================="
    echo "Build Complete!"
    echo "=========================================="
    echo ""
    echo "Executables:"
    echo "  - fbsh (Interactive Shell): $SCRIPT_DIR/fbsh"
    echo "  - fbc (Compiler):           $SCRIPT_DIR/fbc"
    echo ""
    echo "Test with:"
    echo "  ./fbsh"
    echo "  ./fbc test_timers.bas -o test_timers.lua"
    echo ""
else
    echo ""
    echo "Setup complete. Build manually with:"
    echo "  ./rebuild_fbsh_linux.sh"
    echo "  ./rebuild_fbc_linux.sh"
    echo ""
fi

echo "=========================================="
echo "Lima Ubuntu setup complete!"
echo "=========================================="
