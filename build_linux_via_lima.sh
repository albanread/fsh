#!/bin/bash
#
# build_linux_via_lima.sh
# Build FasterBASIC for Linux using Lima, then copy back to macOS
#
# This script runs on macOS and:
# 1. Syncs source code to Lima Ubuntu VM
# 2. Builds fbsh and fbc for Linux
# 3. Packages the distribution
# 4. Copies the archive back to macOS
#

set -e

echo "=========================================="
echo "FasterBASIC Linux Build via Lima"
echo "=========================================="
echo ""

# Configuration
LIMA_INSTANCE="ubuntu"
MAC_SOURCE_DIR="$HOME/FasterBasicGreen"
LIMA_BUILD_DIR="FasterBasicGreen-build"
MAC_OUTPUT_DIR="$HOME/FasterBasicGreen/releases"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Lima is installed
if ! command -v limactl &> /dev/null; then
    error "Lima is not installed. Install with: brew install lima"
    exit 1
fi

# Check if Lima instance exists
if ! limactl list | grep -q "^$LIMA_INSTANCE"; then
    error "Lima instance '$LIMA_INSTANCE' not found"
    echo "Create it with: limactl start --name=$LIMA_INSTANCE"
    exit 1
fi

# Check if Lima instance is running
if ! limactl list | grep "^$LIMA_INSTANCE" | grep -q "Running"; then
    warn "Lima instance '$LIMA_INSTANCE' is not running"
    info "Starting Lima instance..."
    limactl start $LIMA_INSTANCE
    sleep 2
fi

info "Lima instance '$LIMA_INSTANCE' is running"
echo ""

# Step 1: Sync source code to Lima
info "Step 1: Syncing source code to Lima..."
limactl shell $LIMA_INSTANCE bash << 'EOF'
    set -e

    # Create build directory if it doesn't exist
    if [ ! -d ~/FasterBasicGreen-build ]; then
        echo "Creating build directory..."
        mkdir -p ~/FasterBasicGreen-build
    fi

    # Sync source files from macOS mount
    echo "Syncing source files..."
    rsync -a --exclude='build' --exclude='build_new' --exclude='.git' \
          --exclude='*.o' --exclude='fbsh' --exclude='fbc' \
          /Users/oberon/FasterBasicGreen/ ~/FasterBasicGreen-build/

    echo "Source sync complete"
EOF

echo ""

# Step 2: Install dependencies (if needed)
info "Step 2: Checking dependencies..."
limactl shell $LIMA_INSTANCE bash << 'EOF'
    set -e

    # Check if build tools are installed
    if ! command -v g++ &> /dev/null; then
        echo "Installing build dependencies..."
        sudo apt-get update
        sudo apt-get install -y build-essential libluajit-5.1-dev libsqlite3-dev pkg-config
    else
        echo "Build tools already installed"
    fi
EOF

echo ""

# Step 3: Apply all source fixes
info "Step 3: Applying source fixes for Linux compatibility..."
limactl shell $LIMA_INSTANCE bash << 'EOF'
    set -e
    cd ~/FasterBasicGreen-build

    echo "Copying fixed source files..."

    # Copy all fixed files from the macOS mount
    for file in \
        "FasterBASICT/src/fasterbasic_lexer.h" \
        "FasterBASICT/src/fasterbasic_ast.h" \
        "FasterBASICT/src/fasterbasic_cfg.cpp" \
        "FasterBASICT/src/modular_commands.cpp" \
        "FasterBASICT/src/SourceDocument.cpp" \
        "FasterBASICT/src/basic_formatter_lib.cpp" \
        "FasterBASICT/runtime/ConstantsManager.cpp" \
        "FasterBASICT/runtime/FileManager.h" \
        "FasterBASICT/runtime/FileManager.cpp" \
        "FasterBASICT/runtime/TimerManager.cpp" \
        "FasterBASICT/runtime/TimerManager_terminal.cpp" \
        "FasterBASICT/runtime/timer_lua_bindings.cpp" \
        "FasterBASICT/runtime/timer_lua_bindings_terminal.cpp"
    do
        if [ -f "/Users/oberon/FasterBasicGreen/$file" ]; then
            cp "/Users/oberon/FasterBasicGreen/$file" "$file"
            echo "  ✓ $file"
        fi
    done

    echo "Source fixes applied"
EOF

echo ""

# Step 4: Build fbsh and fbc
info "Step 4: Building Linux binaries..."
limactl shell $LIMA_INSTANCE bash << 'EOF'
    set -e
    cd ~/FasterBasicGreen-build

    echo "Building fbsh (interactive shell)..."
    ./rebuild_fbsh_linux.sh

    echo ""
    echo "Building fbc (compiler)..."
    ./rebuild_fbc_linux.sh

    echo ""
    echo "Build complete!"
    ls -lh fbsh fbc
EOF

echo ""

# Step 5: Package distribution
info "Step 5: Creating distribution package..."
limactl shell $LIMA_INSTANCE bash << 'EOF'
    set -e
    cd ~/FasterBasicGreen-build

    # Copy packaging script if it doesn't exist
    if [ ! -f package_linux.sh ]; then
        cp /Users/oberon/FasterBasicGreen/package_linux.sh .
    fi

    # Always ensure it's executable
    chmod +x package_linux.sh

    echo "Running packaging script..."
    ./package_linux.sh
EOF

echo ""

# Step 6: Copy archive back to macOS
info "Step 6: Copying distribution archive to macOS..."

# Create output directory
mkdir -p "$MAC_OUTPUT_DIR"

# Get the archive name
ARCHIVE_NAME=$(limactl shell $LIMA_INSTANCE bash << 'EOF'
    cd ~/FasterBasicGreen-build/dist_linux
    ls -1 fasterbasic-linux-*.tar.gz 2>/dev/null | head -1
EOF
)

if [ -z "$ARCHIVE_NAME" ]; then
    error "Could not find distribution archive"
    exit 1
fi

ARCHIVE_NAME=$(echo "$ARCHIVE_NAME" | tr -d '\r\n')

info "Found archive: $ARCHIVE_NAME"

# Copy the file (use tilde expansion in Lima's home directory)
limactl shell $LIMA_INSTANCE bash -c "cat ~/FasterBasicGreen-build/dist_linux/$ARCHIVE_NAME" > "$MAC_OUTPUT_DIR/$ARCHIVE_NAME"

# Verify the copy
if [ -f "$MAC_OUTPUT_DIR/$ARCHIVE_NAME" ]; then
    FILESIZE=$(du -h "$MAC_OUTPUT_DIR/$ARCHIVE_NAME" | cut -f1)
    info "Archive copied successfully: $MAC_OUTPUT_DIR/$ARCHIVE_NAME ($FILESIZE)"
else
    error "Failed to copy archive"
    exit 1
fi

echo ""
echo "=========================================="
echo "Build Complete!"
echo "=========================================="
echo ""
echo "Linux distribution package:"
echo "  $MAC_OUTPUT_DIR/$ARCHIVE_NAME"
echo ""
echo "To test on a Linux system:"
echo "  1. Copy the archive to Linux"
echo "  2. tar xzf $ARCHIVE_NAME"
echo "  3. cd fasterbasic-linux-x86_64-2025.1"
echo "  4. ./bin/fbsh"
echo ""
echo "To test in Lima:"
echo "  limactl shell $LIMA_INSTANCE"
echo "  cd ~/FasterBasicGreen-build"
echo "  ./fbsh"
echo ""
echo "=========================================="
