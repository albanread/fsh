# FasterBASIC - Build Essentials

FasterBASIC is a very conventional BASIC designed to be extremely familiar to BASIC programmers

This is a compiler written in C, with a supporting runtime written in C, the final stage of code generation
transpiles to Lua; LuaJIT runs that code, it is LuaJIT that makes the resulting BASIC programs fast.

This works in a MacOS and (less tested) Linux terminal.


This is a minimal distribution containing only the source code needed to build
the FasterBASIC compiler (fbc) and interactive shell (fbsh).

## Building on macOS

```bash
./rebuild_fbsh.sh     # Build interactive shell
./rebuild_fbc.sh      # Build compiler
```

## Building on Linux

```bash
./rebuild_fbsh_linux.sh    # Build interactive shell
./rebuild_fbc_linux.sh     # Build compiler
```

Or use the setup script for first-time build:

```bash
./setup_lima_ubuntu.sh     # Install dependencies and build (Ubuntu/Debian)
```

## Building on macOS for Linux (via Lima)

```bash
# One command to build Linux binaries from macOS
./build_linux_via_lima.sh
```

This will create a distributable package at `releases/fasterbasic-linux-*.tar.gz`

## Requirements

### macOS
- Xcode Command Line Tools
- LuaJIT (`brew install luajit`)
- SQLite3 (included with macOS)

### Linux
- build-essential (gcc, g++, make)
- libluajit-5.1-dev
- libsqlite3-dev
- pkg-config

Install on Ubuntu/Debian:
```bash
sudo apt-get install build-essential libluajit-5.1-dev libsqlite3-dev pkg-config
```

## Project Structure

```
FasterBASICT/
├── src/           - Compiler source (lexer, parser, codegen, etc.)
├── shell/         - Interactive shell (REPL) source
├── runtime/       - Runtime bindings and Lua libraries
examples/          - Example BASIC programs
```

## Testing

After building, test with:

```bash
./fbsh                                    # Interactive mode
./fbc examples/hello.bas -o hello.lua    # Compile mode
```

## License

Copyright © 2025 FasterBASIC Project
# fsh
