#!/bin/bash
#
# build_timer_lib.sh
# Build the timer system as a static library for linking with fbsh/fbc
#

set -e

echo "=== Building Timer System Library ==="
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Find LuaJIT
if [ -d "/opt/homebrew/opt/luajit" ]; then
    LUAJIT_INCLUDE="/opt/homebrew/opt/luajit/include/luajit-2.1"
    LUAJIT_LIB="/opt/homebrew/opt/luajit/lib"
elif [ -d "/usr/local/opt/luajit" ]; then
    LUAJIT_INCLUDE="/usr/local/opt/luajit/include/luajit-2.1"
    LUAJIT_LIB="/usr/local/opt/luajit/lib"
elif [ -d "/usr/include/luajit-2.1" ]; then
    LUAJIT_INCLUDE="/usr/include/luajit-2.1"
    LUAJIT_LIB="/usr/lib"
elif [ -d "/usr/local/include/luajit-2.1" ]; then
    LUAJIT_INCLUDE="/usr/local/include/luajit-2.1"
    LUAJIT_LIB="/usr/local/lib"
elif [ -d "/usr/include/luajit-2.0" ]; then
    LUAJIT_INCLUDE="/usr/include/luajit-2.0"
    LUAJIT_LIB="/usr/lib"
else
    echo "Error: LuaJIT not found."
    echo "Install with: brew install luajit                         (macOS)"
    echo "           or: sudo apt-get install libluajit-5.1-dev    (Debian/Ubuntu)"
    echo "           or: sudo yum install luajit-devel             (RedHat/CentOS)"
    exit 1
fi

echo "LuaJIT include: $LUAJIT_INCLUDE"
echo "LuaJIT lib: $LUAJIT_LIB"
echo ""

# Compile EventQueue.cpp
echo "Compiling EventQueue.cpp..."
g++ -std=c++17 -O3 -c -fPIC \
    -I"$LUAJIT_INCLUDE" \
    -Wall -Wextra \
    EventQueue.cpp -o EventQueue.o

# Compile TimerManager.cpp
echo "Compiling TimerManager.cpp..."
g++ -std=c++17 -O3 -c -fPIC \
    -I"$LUAJIT_INCLUDE" \
    -Wall -Wextra \
    TimerManager.cpp -o TimerManager.o

# Compile timer_lua_bindings.cpp
echo "Compiling timer_lua_bindings.cpp..."
g++ -std=c++17 -O3 -c -fPIC \
    -I"$LUAJIT_INCLUDE" \
    -Wall -Wextra \
    timer_lua_bindings.cpp -o timer_lua_bindings.o

# Create static library
echo "Creating static library libbasic_timer.a..."
ar rcs libbasic_timer.a EventQueue.o TimerManager.o timer_lua_bindings.o

# Clean up object files
echo "Cleaning up object files..."
rm -f EventQueue.o TimerManager.o timer_lua_bindings.o

echo ""
echo "=== Build Complete ==="
echo "Library: $SCRIPT_DIR/libbasic_timer.a"
echo ""
echo "To link with your executable:"
echo "  g++ ... -L$SCRIPT_DIR -lbasic_timer -lpthread"
echo ""
