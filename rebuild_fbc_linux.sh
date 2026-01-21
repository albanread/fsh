#!/bin/bash
#
# rebuild_fbc_linux.sh
# Rebuild the FasterBASIC Compiler (fbc) for Linux
#
# This script builds fbc on Linux systems with the appropriate
# compiler flags and library paths.
#

set -e

echo "=== Building FBC Compiler for Linux ==="
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Find LuaJIT
LUAJIT_INCLUDE=""
LUAJIT_LIB=""

# Check common LuaJIT installation locations on Linux
if [ -d "/usr/include/luajit-2.1" ]; then
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
    echo "Install with: sudo apt-get install libluajit-5.1-dev  (Debian/Ubuntu)"
    echo "           or: sudo yum install luajit-devel         (RedHat/CentOS)"
    echo "           or: sudo pacman -S luajit                 (Arch)"
    exit 1
fi

echo "LuaJIT include: $LUAJIT_INCLUDE"
echo "LuaJIT lib: $LUAJIT_LIB"
echo ""

# Build timer library if not already built
TIMER_LIB="FasterBASICT/runtime/libbasic_timer.a"
if [ ! -f "$TIMER_LIB" ]; then
    echo "Building timer library..."
    cd FasterBASICT/runtime
    ./build_timer_lib.sh
    cd "$SCRIPT_DIR"
    echo ""
fi

# Set up paths
SRC_DIR="FasterBASICT/src"
RUNTIME_DIR="FasterBASICT/runtime"
BUILD_DIR="FasterBASICT/build_new"

# Create build directory
mkdir -p "$BUILD_DIR"

echo "Compiling source files..."
echo ""

# Compile lexer
echo "  [1/9] Compiling fasterbasic_lexer.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_lexer.cpp" \
    -o "$BUILD_DIR/fasterbasic_lexer.o"

# Compile parser
echo "  [2/9] Compiling fasterbasic_parser.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_parser.cpp" \
    -o "$BUILD_DIR/fasterbasic_parser.o"

# Compile semantic analyzer
echo "  [3/9] Compiling fasterbasic_semantic.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_semantic.cpp" \
    -o "$BUILD_DIR/fasterbasic_semantic.o"

# Compile IR code
echo "  [4/9] Compiling fasterbasic_ircode.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_ircode.cpp" \
    -o "$BUILD_DIR/fasterbasic_ircode.o"

# Compile Lua codegen
echo "  [5/9] Compiling fasterbasic_lua_codegen.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_lua_codegen.cpp" \
    -o "$BUILD_DIR/fasterbasic_lua_codegen.o"

# Compile Lua expression generator
echo "  [6/9] Compiling fasterbasic_lua_expr.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_lua_expr.cpp" \
    -o "$BUILD_DIR/fasterbasic_lua_expr.o"

# Compile data preprocessor
echo "  [7/9] Compiling fasterbasic_data_preprocessor.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_data_preprocessor.cpp" \
    -o "$BUILD_DIR/fasterbasic_data_preprocessor.o"

# Compile optimizer
echo "  [8/9] Compiling fasterbasic_optimizer.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_optimizer.cpp" \
    -o "$BUILD_DIR/fasterbasic_optimizer.o"

# Compile main fbc
echo "  [9/9] Compiling fbc.cpp..."
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    -I"$RUNTIME_DIR" \
    "$SRC_DIR/fbc.cpp" \
    -o "$BUILD_DIR/fbc.o"

echo ""
echo "Compiling runtime dependencies..."

# Compile runtime modules needed by fbc
echo "  - ConstantsManager.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/ConstantsManager.cpp" \
    -o "$BUILD_DIR/ConstantsManager.o"

echo "  - constants_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/constants_lua_bindings.cpp" \
    -o "$BUILD_DIR/constants_lua_bindings.o"

echo "  - basic_bitwise.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/basic_bitwise.cpp" \
    -o "$BUILD_DIR/basic_bitwise.o"

echo "  - bitwise_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/bitwise_lua_bindings.cpp" \
    -o "$BUILD_DIR/bitwise_lua_bindings.o"

echo "  - unicode_runtime.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/unicode_runtime.cpp" \
    -o "$BUILD_DIR/unicode_runtime.o"

echo "  - unicode_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/unicode_lua_bindings.cpp" \
    -o "$BUILD_DIR/unicode_lua_bindings.o"

echo "  - fasterbasic_peephole.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_peephole.cpp" \
    -o "$BUILD_DIR/fasterbasic_peephole.o"

echo "  - fasterbasic_cfg.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/fasterbasic_cfg.cpp" \
    -o "$BUILD_DIR/fasterbasic_cfg.o"

echo "  - modular_commands.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/modular_commands.cpp" \
    -o "$BUILD_DIR/modular_commands.o"

echo "  - command_registry_core.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/command_registry_core.cpp" \
    -o "$BUILD_DIR/command_registry_core.o"

echo "  - command_registry_plugins.cpp"
g++ -std=c++17 -O2 -c \
    -DENABLE_LUA_BINDINGS \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/command_registry_plugins.cpp" \
    -o "$BUILD_DIR/command_registry_plugins.o"

echo "  - plugin_loader.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$SRC_DIR" \
    "$SRC_DIR/plugin_loader.cpp" \
    -o "$BUILD_DIR/plugin_loader.o"

echo "  - EventQueue.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/EventQueue.cpp" \
    -o "$BUILD_DIR/EventQueue.o"

echo "  - TimerManager.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/TimerManager.cpp" \
    -o "$BUILD_DIR/TimerManager.o"

echo "  - timer_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/timer_lua_bindings.cpp" \
    -o "$BUILD_DIR/timer_lua_bindings.o"

echo "  - DataManager.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/DataManager.cpp" \
    -o "$BUILD_DIR/DataManager.o"

echo "  - data_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/data_lua_bindings.cpp" \
    -o "$BUILD_DIR/data_lua_bindings.o"

echo "  - FileManager.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/FileManager.cpp" \
    -o "$BUILD_DIR/FileManager.o"

echo "  - fileio_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/fileio_lua_bindings.cpp" \
    -o "$BUILD_DIR/fileio_lua_bindings.o"

echo "  - terminal_io.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/terminal_io.cpp" \
    -o "$BUILD_DIR/terminal_io.o"

echo "  - terminal_lua_bindings.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/terminal_lua_bindings.cpp" \
    -o "$BUILD_DIR/terminal_lua_bindings.o"

echo "  - console_stubs.cpp"
g++ -std=c++17 -O2 -c \
    -I"$LUAJIT_INCLUDE" \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/console_stubs.cpp" \
    -o "$BUILD_DIR/console_stubs.o"

echo ""
echo "Linking fbc executable..."

# Link everything together (Linux-specific flags)
g++ -std=c++17 -O2 \
    "$BUILD_DIR/fbc.o" \
    "$BUILD_DIR/fasterbasic_lexer.o" \
    "$BUILD_DIR/fasterbasic_parser.o" \
    "$BUILD_DIR/fasterbasic_semantic.o" \
    "$BUILD_DIR/fasterbasic_ircode.o" \
    "$BUILD_DIR/fasterbasic_lua_codegen.o" \
    "$BUILD_DIR/fasterbasic_lua_expr.o" \
    "$BUILD_DIR/fasterbasic_data_preprocessor.o" \
    "$BUILD_DIR/fasterbasic_optimizer.o" \
    "$BUILD_DIR/fasterbasic_peephole.o" \
    "$BUILD_DIR/fasterbasic_cfg.o" \
    "$BUILD_DIR/ConstantsManager.o" \
    "$BUILD_DIR/constants_lua_bindings.o" \
    "$BUILD_DIR/basic_bitwise.o" \
    "$BUILD_DIR/bitwise_lua_bindings.o" \
    "$BUILD_DIR/unicode_runtime.o" \
    "$BUILD_DIR/unicode_lua_bindings.o" \
    "$BUILD_DIR/modular_commands.o" \
    "$BUILD_DIR/command_registry_core.o" \
    "$BUILD_DIR/command_registry_plugins.o" \
    "$BUILD_DIR/plugin_loader.o" \
    "$BUILD_DIR/DataManager.o" \
    "$BUILD_DIR/data_lua_bindings.o" \
    "$BUILD_DIR/FileManager.o" \
    "$BUILD_DIR/fileio_lua_bindings.o" \
    "$BUILD_DIR/terminal_io.o" \
    "$BUILD_DIR/terminal_lua_bindings.o" \
    "$BUILD_DIR/EventQueue.o" \
    "$BUILD_DIR/TimerManager.o" \
    "$BUILD_DIR/timer_lua_bindings.o" \
    "$BUILD_DIR/console_stubs.o" \
    -L"$LUAJIT_LIB" -lluajit-5.1 \
    -lpthread \
    -ldl \
    -o fbc

echo ""
echo "=== Build Complete ==="
echo "Executable: $SCRIPT_DIR/fbc"
echo ""
echo "Test with:"
echo "  ./fbc test_timers.bas -o test_timers.lua"
echo ""
echo "Note: Make sure runtime files are accessible:"
echo "  - FasterBASICT/runtime/*.lua"
echo "  - FasterBASICT/runtime/*.so"
echo ""
