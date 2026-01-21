//
// fileio_lua_bindings.cpp
// FasterBASICT - File I/O Lua Bindings
//
// Provides Lua bindings for BASIC file I/O operations (OPEN, CLOSE, INPUT#, etc.)
// Used by the standalone fbc compiler/runner.
//

#include "fileio_lua_bindings.h"
#include "FileManager.h"
#include <lua.hpp>
#include <cstring>

namespace FasterBASIC {

// Global FileManager instance for standalone fbc
static FileManager g_fileManager;

// =============================================================================
// Helper Functions
// =============================================================================

static void luaL_setglobalfunction(lua_State* L, const char* name, lua_CFunction func) {
    lua_pushcfunction(L, func);
    lua_setglobal(L, name);
}

// =============================================================================
// File I/O API Bindings
// =============================================================================

static int lua_basic_open(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* modeStr = luaL_checkstring(L, 2);
    int fileNumber = luaL_checkinteger(L, 3);
    int recordLength = luaL_optinteger(L, 4, 128);
    
    // Parse mode string
    FileMode mode;
    if (strcmp(modeStr, "INPUT") == 0 || strcmp(modeStr, "input") == 0) {
        mode = FileMode::INPUT;
    } else if (strcmp(modeStr, "OUTPUT") == 0 || strcmp(modeStr, "output") == 0) {
        mode = FileMode::OUTPUT;
    } else if (strcmp(modeStr, "APPEND") == 0 || strcmp(modeStr, "append") == 0) {
        mode = FileMode::APPEND;
    } else if (strcmp(modeStr, "RANDOM") == 0 || strcmp(modeStr, "random") == 0) {
        mode = FileMode::RANDOM;
    } else {
        return luaL_error(L, "Invalid file mode: %s", modeStr);
    }
    
    try {
        g_fileManager.open(fileNumber, filename, mode, recordLength);
        return 0;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_close(lua_State* L) {
    if (lua_gettop(L) == 0) {
        // CLOSE with no arguments - close all files
        g_fileManager.closeAll();
    } else {
        // CLOSE #n
        int fileNumber = luaL_checkinteger(L, 1);
        try {
            g_fileManager.close(fileNumber);
        } catch (const FileError& e) {
            return luaL_error(L, "File error: %s", e.what());
        }
    }
    return 0;
}

static int lua_basic_input_file(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        FileValue value = g_fileManager.readValue(fileNumber);
        
        // Push appropriate type to Lua
        if (std::holds_alternative<int>(value)) {
            lua_pushinteger(L, std::get<int>(value));
        } else if (std::holds_alternative<double>(value)) {
            lua_pushnumber(L, std::get<double>(value));
        } else {
            lua_pushstring(L, std::get<std::string>(value).c_str());
        }
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_line_input_file(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        std::string line = g_fileManager.readLine(fileNumber);
        lua_pushstring(L, line.c_str());
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_input_string_file(lua_State* L) {
    int count = luaL_checkinteger(L, 1);
    int fileNumber = luaL_checkinteger(L, 2);
    
    try {
        std::string result = g_fileManager.readChars(fileNumber, count);
        lua_pushstring(L, result.c_str());
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_print_file(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    const char* separator = luaL_optstring(L, 3, "\n");
    
    try {
        FileValue value;
        
        // Determine value type
        int valueType = lua_type(L, 2);
        if (valueType == LUA_TNUMBER) {
            double num = lua_tonumber(L, 2);
            // Check if it's an integer value
            if (num == static_cast<int>(num)) {
                value = static_cast<int>(num);
            } else {
                value = num;
            }
        } else if (valueType == LUA_TSTRING) {
            value = std::string(lua_tostring(L, 2));
        } else {
            return luaL_error(L, "Invalid value type for PRINT#");
        }
        
        g_fileManager.writeFormatted(fileNumber, value, separator);
        return 0;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_write_file(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    bool isLast = lua_toboolean(L, 3);
    
    try {
        FileValue value;
        
        // Determine value type
        int valueType = lua_type(L, 2);
        if (valueType == LUA_TNUMBER) {
            double num = lua_tonumber(L, 2);
            // Check if it's an integer value
            if (num == static_cast<int>(num)) {
                value = static_cast<int>(num);
            } else {
                value = num;
            }
        } else if (valueType == LUA_TSTRING) {
            value = std::string(lua_tostring(L, 2));
        } else {
            return luaL_error(L, "Invalid value type for WRITE#");
        }
        
        g_fileManager.writeQuoted(fileNumber, value, isLast);
        return 0;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_eof(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        bool eof = g_fileManager.isEOF(fileNumber);
        lua_pushboolean(L, eof ? 1 : 0);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_loc(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        long position = g_fileManager.getPosition(fileNumber);
        lua_pushinteger(L, position);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_lof(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        long length = g_fileManager.getLength(fileNumber);
        lua_pushinteger(L, length);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

// =============================================================================
// BBC BASIC File I/O API Bindings
// =============================================================================

static int lua_basic_openin(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    
    try {
        int fileHandle = g_fileManager.openIn(filename);
        lua_pushinteger(L, fileHandle);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_openout(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    
    try {
        int fileHandle = g_fileManager.openOut(filename);
        lua_pushinteger(L, fileHandle);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_openup(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    
    try {
        int fileHandle = g_fileManager.openUp(filename);
        lua_pushinteger(L, fileHandle);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_bget(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        int byte = g_fileManager.readByte(fileNumber);
        lua_pushinteger(L, byte);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_bput(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    int byte = luaL_checkinteger(L, 2);
    
    try {
        g_fileManager.writeByte(fileNumber, byte);
        return 0;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_eof_hash(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        bool eof = g_fileManager.isAtEOF(fileNumber);
        lua_pushboolean(L, eof ? 1 : 0);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_ext_hash(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        long extent = g_fileManager.getFileExtent(fileNumber);
        lua_pushinteger(L, extent);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_ptr_hash(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        long pointer = g_fileManager.getFilePointer(fileNumber);
        lua_pushinteger(L, pointer);
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_ptr_set(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    long position = luaL_checkinteger(L, 2);
    
    try {
        g_fileManager.setFilePointer(fileNumber, position);
        return 0;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_get_string_line(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    
    try {
        std::string result = g_fileManager.readLineFromFile(fileNumber);
        lua_pushstring(L, result.c_str());
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

static int lua_basic_get_string_to_char(lua_State* L) {
    int fileNumber = luaL_checkinteger(L, 1);
    const char* termStr = luaL_checkstring(L, 2);
    
    if (strlen(termStr) != 1) {
        return luaL_error(L, "Terminator must be a single character");
    }
    
    try {
        char terminator = termStr[0];
        std::string result = g_fileManager.readUntilChar(fileNumber, terminator);
        lua_pushstring(L, result.c_str());
        return 1;
    } catch (const FileError& e) {
        return luaL_error(L, "File error: %s", e.what());
    }
}

// =============================================================================
// Module Registration
// =============================================================================

void register_fileio_functions(lua_State* L) {
    // Register file I/O functions
    luaL_setglobalfunction(L, "basic_open", lua_basic_open);
    luaL_setglobalfunction(L, "basic_close", lua_basic_close);
    luaL_setglobalfunction(L, "basic_input_file", lua_basic_input_file);
    luaL_setglobalfunction(L, "basic_line_input_file", lua_basic_line_input_file);
    luaL_setglobalfunction(L, "basic_input_string_file", lua_basic_input_string_file);
    luaL_setglobalfunction(L, "basic_print_file", lua_basic_print_file);
    luaL_setglobalfunction(L, "basic_write_file", lua_basic_write_file);
    luaL_setglobalfunction(L, "basic_eof", lua_basic_eof);
    luaL_setglobalfunction(L, "basic_loc", lua_basic_loc);
    luaL_setglobalfunction(L, "basic_lof", lua_basic_lof);
    
    // Register BBC BASIC file I/O functions
    luaL_setglobalfunction(L, "basic_openin", lua_basic_openin);
    luaL_setglobalfunction(L, "basic_openout", lua_basic_openout);
    luaL_setglobalfunction(L, "basic_openup", lua_basic_openup);
    luaL_setglobalfunction(L, "basic_bget", lua_basic_bget);
    luaL_setglobalfunction(L, "basic_bput", lua_basic_bput);
    luaL_setglobalfunction(L, "basic_eof_hash", lua_basic_eof_hash);
    luaL_setglobalfunction(L, "basic_ext_hash", lua_basic_ext_hash);
    luaL_setglobalfunction(L, "basic_ptr_hash", lua_basic_ptr_hash);
    luaL_setglobalfunction(L, "basic_ptr_set", lua_basic_ptr_set);
    luaL_setglobalfunction(L, "basic_get_string_line", lua_basic_get_string_line);
    luaL_setglobalfunction(L, "basic_get_string_to_char", lua_basic_get_string_to_char);
}

void clear_fileio_state() {
    g_fileManager.clear();
}

} // namespace FasterBASIC