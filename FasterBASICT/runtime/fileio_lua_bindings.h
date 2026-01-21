//
// fileio_lua_bindings.h
// FasterBASICT - File I/O Lua Bindings
//
// Provides Lua bindings for BASIC file I/O operations (OPEN, CLOSE, INPUT#, etc.)
// Used by the standalone fbc compiler/runner.
//

#ifndef FILEIO_LUA_BINDINGS_H
#define FILEIO_LUA_BINDINGS_H

#include <lua.hpp>

namespace FasterBASIC {

// Register all file I/O functions in the Lua state
void register_fileio_functions(lua_State* L);

// Clear file I/O state (close all files)
void clear_fileio_state();

} // namespace FasterBASIC

#endif // FILEIO_LUA_BINDINGS_H