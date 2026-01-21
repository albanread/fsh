# FasterBASIC Shell (fsh)

[![Build FasterBASIC](https://github.com/albanread/fsh/actions/workflows/build.yml/badge.svg)](https://github.com/albanread/fsh/actions/workflows/build.yml)

FasterBASIC is a very conventional BASIC designed to be extremely familiar to BASIC programmers.

This is a compiler written in C, with a supporting runtime written in C. The final stage of code generation
transpiles to Lua; LuaJIT runs that code, and it is LuaJIT that makes the resulting BASIC programs fast.

This works on macOS and (less tested) Linux terminals.

## Features

- **fbsh** - Interactive BASIC shell with line-based program entry
- **fbc** - Standalone BASIC compiler with optimization options
- LuaJIT-powered execution for high performance
- Timer and event system support
- Unicode string handling
- Plugin system for extensibility
- Cross-platform (macOS and Linux)

## Download Pre-built Binaries

Pre-built binaries are automatically generated for each release:

**[Download Latest Release](https://github.com/albanread/fsh/releases)**

Available for:
- macOS (Intel/Apple Silicon)
- Linux (x86_64)

## Building from Source

### macOS

Requirements:
- Xcode Command Line Tools
- LuaJIT: `brew install luajit`
- SQLite3 (included with macOS)

Build:
```bash
./rebuild_fbsh.sh     # Build interactive shell
./rebuild_fbc.sh      # Build compiler
```

### Linux

Requirements:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libluajit-5.1-dev libsqlite3-dev

# RedHat/CentOS
sudo yum install gcc-c++ luajit-devel sqlite-devel

# Arch
sudo pacman -S base-devel luajit sqlite
```

Build:
```bash
./rebuild_fbsh_linux.sh    # Build interactive shell
./rebuild_fbc_linux.sh     # Build compiler
```

### Build Linux binaries from macOS (via Lima)

```bash
./build_linux_via_lima.sh
```

Creates a distributable package at `releases/fasterbasic-linux-*.tar.gz`

## Usage

### Interactive Shell (fbsh)

```bash
./fbsh
```

Classic BASIC shell with commands like LIST, RUN, LOAD, SAVE, etc.

```basic
10 PRINT "Hello, World!"
20 FOR I = 1 TO 10
30   PRINT "Count: "; I
40 NEXT I
50 END

RUN
```

### Compiler (fbc)

```bash
# Compile and run immediately
./fbc_new program.bas

# Compile to Lua file
./fbc_new program.bas -o program.lua

# With optimizations and timing
./fbc_new --opt-all -t program.bas

# Verbose mode with stats
./fbc_new -v program.bas
```

## Project Structure

```
FasterBASICT/
├── src/           - Compiler source (lexer, parser, codegen, optimizer)
├── shell/         - Interactive shell (REPL, editor, syntax highlighter)
├── runtime/       - Runtime bindings and Lua libraries
docs/              - Documentation
examples/          - Example BASIC programs (not included in this repo)
```

## Optimization Options

The compiler supports multiple optimization levels:

- `--opt-ast` - AST-level optimizations (constant folding, dead code elimination)
- `--opt-peep` - Peephole optimizations (IR-level)
- `--opt-all` - Enable all optimizers
- `--opt-stats` - Show detailed optimization statistics

## CI/CD

This project uses GitHub Actions for continuous integration:

- Automatic builds on every push
- Multi-platform testing (macOS and Linux)
- Pre-built binaries available as artifacts
- Automatic release creation on version tags

## Development

After making changes, rebuild:

```bash
./rebuild_fbsh.sh && ./rebuild_fbc.sh
```

Run tests:
```bash
./fbsh --version
./fbc_new --help
```

## License

Copyright © 2024-2025 FasterBASIC Project

## Contributing

Contributions are welcome! Please ensure:
- Code compiles on both macOS and Linux
- Follow existing code style
- Update documentation as needed

## Links

- [GitHub Repository](https://github.com/albanread/fsh)
- [Issues & Bug Reports](https://github.com/albanread/fsh/issues)
- [Latest Releases](https://github.com/albanread/fsh/releases)