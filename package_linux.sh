#!/bin/bash
#
# package_linux.sh
# Package FasterBASIC for Linux distribution
#
# Creates a distributable ZIP archive containing fbsh, fbc, runtime files,
# and documentation for Linux systems.
#

set -e

echo "=========================================="
echo "FasterBASIC Linux Distribution Packager"
echo "=========================================="
echo ""

# Configuration
VERSION="2025.1"
# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    ARCH_NAME="arm64"
elif [ "$ARCH" = "x86_64" ]; then
    ARCH_NAME="x86_64"
else
    ARCH_NAME="$ARCH"
fi
PACKAGE_NAME="fasterbasic-linux-${ARCH_NAME}-${VERSION}"
BUILD_DIR="FasterBASICT/build"
DIST_DIR="dist_linux"
ARCHIVE_NAME="${PACKAGE_NAME}.zip"

# Check if we're on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Warning: This script is designed to run on Linux"
    echo "Current OS: $OSTYPE"
    echo "Continuing anyway..."
    echo ""
fi

# Check if executables exist
if [ ! -f "fbsh" ] || [ ! -f "fbc" ]; then
    echo "Error: fbsh or fbc not found in current directory"
    echo "Please build the executables first:"
    echo "  ./rebuild_fbsh_linux.sh"
    echo "  ./rebuild_fbc_linux.sh"
    exit 1
fi

echo "Found executables:"
ls -lh fbsh fbc
echo ""

# Create distribution directory structure
echo "Creating distribution directory: $DIST_DIR/$PACKAGE_NAME"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/$PACKAGE_NAME"
mkdir -p "$DIST_DIR/$PACKAGE_NAME/bin"
mkdir -p "$DIST_DIR/$PACKAGE_NAME/runtime"
mkdir -p "$DIST_DIR/$PACKAGE_NAME/plugins/enabled"
mkdir -p "$DIST_DIR/$PACKAGE_NAME/examples"
mkdir -p "$DIST_DIR/$PACKAGE_NAME/docs"

echo ""
echo "=== Copying Executables ==="
cp -v fbsh "$DIST_DIR/$PACKAGE_NAME/bin/"
cp -v fbc "$DIST_DIR/$PACKAGE_NAME/bin/"
chmod +x "$DIST_DIR/$PACKAGE_NAME/bin/fbsh"
chmod +x "$DIST_DIR/$PACKAGE_NAME/bin/fbc"

echo ""
echo "=== Copying Runtime Files ==="
# Copy Lua runtime files
for lua_file in FasterBASICT/runtime/*.lua; do
    if [ -f "$lua_file" ]; then
        cp -v "$lua_file" "$DIST_DIR/$PACKAGE_NAME/runtime/"
    fi
done

# Copy shared libraries (.so files)
if ls FasterBASICT/runtime/*.so 1> /dev/null 2>&1; then
    cp -v FasterBASICT/runtime/*.so "$DIST_DIR/$PACKAGE_NAME/runtime/"
fi

echo ""
echo "=== Copying Plugin Files ==="
# Copy plugins if they exist
if [ -d "plugins/enabled" ]; then
    for plugin in plugins/enabled/*.so; do
        if [ -f "$plugin" ]; then
            cp -v "$plugin" "$DIST_DIR/$PACKAGE_NAME/plugins/enabled/"
        fi
    done
fi

echo ""
echo "=== Copying Example Programs ==="
# Copy example BASIC programs
if [ -d "examples" ]; then
    cp -v examples/*.bas "$DIST_DIR/$PACKAGE_NAME/examples/" 2>/dev/null || true
fi

# Copy test programs as examples
for test_file in test_*.bas; do
    if [ -f "$test_file" ]; then
        cp -v "$test_file" "$DIST_DIR/$PACKAGE_NAME/examples/"
    fi
done

echo ""
echo "=== Creating Documentation ==="

# Create README
cat > "$DIST_DIR/$PACKAGE_NAME/README.md" << 'EOF'
# FasterBASIC for Linux

High-performance BASIC interpreter and compiler for Linux systems.

## Contents

- `bin/fbsh` - FasterBASIC interactive shell (REPL)
- `bin/fbc` - FasterBASIC compiler (standalone)
- `runtime/` - Lua runtime libraries and modules
- `plugins/enabled/` - Plugin modules
- `examples/` - Sample BASIC programs

## Quick Start

### Interactive Shell

```bash
cd fasterbasic-linux-*
./bin/fbsh
```

In the shell:
```basic
10 PRINT "Hello from FasterBASIC!"
20 END
RUN
```

### Compile a Program

```bash
./bin/fbc examples/test_simple.bas -o output.lua
```

## Installation (Optional)

To install system-wide:

```bash
sudo mkdir -p /opt/fasterbasic
sudo cp -r * /opt/fasterbasic/
sudo ln -s /opt/fasterbasic/bin/fbsh /usr/local/bin/fbsh
sudo ln -s /opt/fasterbasic/bin/fbc /usr/local/bin/fbc
```

Or add to your PATH:

```bash
export PATH="$PATH:/path/to/fasterbasic-linux-*/bin"
export LUA_PATH="/path/to/fasterbasic-linux-*/runtime/?.lua;;"
```

## Requirements

- Linux (x86_64 or ARM64/aarch64)
- LuaJIT 2.1+ (runtime dependency)
- glibc 2.31+ (or compatible)

### Installing LuaJIT

**Ubuntu/Debian:**
```bash
sudo apt-get install luajit
```

**Fedora/RHEL:**
```bash
sudo dnf install luajit
```

**Arch Linux:**
```bash
sudo pacman -S luajit
```

## Shell Commands

- `NEW` - Clear program
- `LIST` - List program
- `RUN` - Run program
- `LOAD "file.bas"` - Load program
- `SAVE "file.bas"` - Save program
- `HELP` - Show help
- `QUIT` - Exit shell

## BASIC Language Features

- Line numbers (10, 20, 30, ...)
- Variables (numeric and string with `$` suffix)
- Arrays (DIM)
- Control flow (IF/THEN, FOR/NEXT, WHILE/WEND, GOTO, GOSUB)
- File I/O (OPEN, PRINT#, INPUT#, CLOSE)
- Functions (SIN, COS, ABS, INT, RND, etc.)
- String operations (LEFT$, RIGHT$, MID$, LEN, etc.)
- DATA/READ/RESTORE
- Subroutines and procedures
- Plugin system

## Documentation

For complete documentation, visit:
https://github.com/yourusername/fasterbasic

## License

Copyright © 2025 FasterBASIC Project

## Support

- Report issues on GitHub
- Community forums
- Documentation wiki

---

Built with ❤️ for the BASIC community
EOF

# Create installation guide
cat > "$DIST_DIR/$PACKAGE_NAME/INSTALL.txt" << 'EOF'
FasterBASIC Linux Installation Guide
=====================================

METHOD 1: Run from Directory (Recommended for Testing)
-------------------------------------------------------
No installation needed! Just run from the extracted directory:

    cd fasterbasic-linux-*
    ./bin/fbsh

METHOD 2: System-Wide Installation
-----------------------------------
Install to /opt/fasterbasic:

    sudo mkdir -p /opt/fasterbasic
    sudo cp -r * /opt/fasterbasic/
    sudo ln -s /opt/fasterbasic/bin/fbsh /usr/local/bin/fbsh
    sudo ln -s /opt/fasterbasic/bin/fbc /usr/local/bin/fbc

Then you can run 'fbsh' or 'fbc' from anywhere.

METHOD 3: User Installation
----------------------------
Install to your home directory:

    mkdir -p ~/.local/fasterbasic
    cp -r * ~/.local/fasterbasic/

Add to ~/.bashrc or ~/.zshrc:

    export PATH="$HOME/.local/fasterbasic/bin:$PATH"
    export LUA_PATH="$HOME/.local/fasterbasic/runtime/?.lua;;"
    export LD_LIBRARY_PATH="$HOME/.local/fasterbasic/runtime:$LD_LIBRARY_PATH"

Then reload your shell:

    source ~/.bashrc  # or source ~/.zshrc

VERIFICATION
------------
Test the installation:

    fbsh --version
    fbc --help

If you get "command not found", make sure the binaries are in your PATH.

DEPENDENCIES
------------
FasterBASIC requires LuaJIT to be installed:

Ubuntu/Debian:  sudo apt-get install luajit
Fedora/RHEL:    sudo dnf install luajit
Arch Linux:     sudo pacman -S luajit

TROUBLESHOOTING
---------------
"fbsh: command not found"
  → Add the bin directory to your PATH

"module not found" errors
  → Set LUA_PATH to include the runtime directory

"error while loading shared libraries"
  → Install LuaJIT: sudo apt-get install luajit libluajit-5.1-2

For more help, see README.md or visit the documentation.
EOF

# Create a simple launcher script
cat > "$DIST_DIR/$PACKAGE_NAME/bin/fbsh-here" << 'EOF'
#!/bin/bash
# Launch fbsh with correct runtime paths set
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
INSTALL_DIR="$(dirname "$SCRIPT_DIR")"
export LUA_PATH="$INSTALL_DIR/runtime/?.lua;;"
exec "$SCRIPT_DIR/fbsh" "$@"
EOF

cat > "$DIST_DIR/$PACKAGE_NAME/bin/fbc-here" << 'EOF'
#!/bin/bash
# Launch fbc with correct runtime paths set
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
INSTALL_DIR="$(dirname "$SCRIPT_DIR")"
export LUA_PATH="$INSTALL_DIR/runtime/?.lua;;"
exec "$SCRIPT_DIR/fbc" "$@"
EOF

chmod +x "$DIST_DIR/$PACKAGE_NAME/bin/fbsh-here"
chmod +x "$DIST_DIR/$PACKAGE_NAME/bin/fbc-here"

echo ""
echo "=== Creating Archive ==="
cd "$DIST_DIR"

# Check if zip is available, otherwise use tar
if command -v zip &> /dev/null; then
    echo "Creating ZIP archive: $ARCHIVE_NAME"
    zip -r "$ARCHIVE_NAME" "$PACKAGE_NAME"
    CREATED_ARCHIVE="$ARCHIVE_NAME"
else
    echo "zip not found, creating tar.gz archive instead"
    ARCHIVE_NAME="${PACKAGE_NAME}.tar.gz"
    tar czf "$ARCHIVE_NAME" "$PACKAGE_NAME"
    CREATED_ARCHIVE="$ARCHIVE_NAME"
fi

cd ..

# Get archive size
ARCHIVE_SIZE=$(du -h "$DIST_DIR/$CREATED_ARCHIVE" | cut -f1)

echo ""
echo "=========================================="
echo "Package Created Successfully!"
echo "=========================================="
echo ""
echo "Archive: $DIST_DIR/$CREATED_ARCHIVE"
echo "Size: $ARCHIVE_SIZE"
echo ""
echo "Contents:"
echo "  - fbsh (interactive shell)"
echo "  - fbc (compiler)"
echo "  - $(ls FasterBASICT/runtime/*.lua 2>/dev/null | wc -l) runtime modules"
echo "  - $(ls FasterBASICT/runtime/*.so 2>/dev/null | wc -l) native libraries"
echo "  - $(ls plugins/enabled/*.so 2>/dev/null | wc -l) plugins"
echo "  - README.md and INSTALL.txt"
echo ""
echo "To test the package:"
echo "  cd $DIST_DIR"
echo "  unzip $CREATED_ARCHIVE  # or tar xzf for .tar.gz"
echo "  cd $PACKAGE_NAME"
echo "  ./bin/fbsh"
echo ""
echo "To distribute:"
echo "  Upload: $DIST_DIR/$CREATED_ARCHIVE"
echo ""
echo "=========================================="
