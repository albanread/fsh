//
// unicode_lua_bindings.cpp
// FasterBASIC - Unicode Runtime Lua C API Bindings
//
// Provides Lua C API wrappers for Unicode functions
// These can be injected directly into the Lua state without FFI
//

#include "unicode_runtime.h"
#include <cstring>
#include <cstdlib>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// =============================================================================
// Lua C API Wrapper Functions
// =============================================================================

// unicode.from_utf8(utf8_string) -> table of codepoints
static int lua_unicode_from_utf8(lua_State* L) {
    // Get UTF-8 string argument
    size_t len;
    const char* utf8_str = luaL_checklstring(L, 1, &len);
    
    // Convert to codepoints
    int32_t out_len;
    int32_t* codepoints = unicode_from_utf8(utf8_str, &out_len);
    
    if (!codepoints) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to convert UTF-8 to codepoints");
        return 2;
    }
    
    // Create Lua table
    lua_createtable(L, out_len, 0);
    
    for (int32_t i = 0; i < out_len; i++) {
        lua_pushinteger(L, codepoints[i]);
        lua_rawseti(L, -2, i + 1);  // Lua uses 1-based indexing
    }
    
    // Free the C array
    unicode_free(codepoints);
    
    return 1;  // Return the table
}

// unicode.to_utf8(codepoint_table) -> utf8_string
static int lua_unicode_to_utf8(lua_State* L) {
    // Check that argument is a table
    luaL_checktype(L, 1, LUA_TTABLE);
    
    // Get table length
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (len == 0) {
        lua_pushstring(L, "");
        return 1;
    }
    
    // Allocate array for codepoints
    int32_t* codepoints = (int32_t*)malloc(len * sizeof(int32_t));
    if (!codepoints) {
        return luaL_error(L, "Memory allocation failed");
    }
    
    // Extract codepoints from table
    for (int32_t i = 0; i < len; i++) {
        lua_rawgeti(L, 1, i + 1);  // Lua uses 1-based indexing
        codepoints[i] = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    
    // Convert to UTF-8
    int32_t utf8_len;
    char* utf8_str = unicode_to_utf8(codepoints, len, &utf8_len);
    
    free(codepoints);
    
    if (!utf8_str) {
        return luaL_error(L, "Failed to convert codepoints to UTF-8");
    }
    
    // Push UTF-8 string to Lua
    lua_pushlstring(L, utf8_str, utf8_len);
    
    // Free the C string
    unicode_free(utf8_str);
    
    return 1;
}

// unicode.upper(codepoint_table) -> uppercase_table
static int lua_unicode_upper(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (len == 0) {
        lua_newtable(L);
        return 1;
    }
    
    // Allocate and extract codepoints
    int32_t* codepoints = (int32_t*)malloc(len * sizeof(int32_t));
    if (!codepoints) {
        return luaL_error(L, "Memory allocation failed");
    }
    
    for (int32_t i = 0; i < len; i++) {
        lua_rawgeti(L, 1, i + 1);
        codepoints[i] = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    
    // Convert to uppercase
    unicode_upper(codepoints, len);
    
    // Create result table
    lua_createtable(L, len, 0);
    for (int32_t i = 0; i < len; i++) {
        lua_pushinteger(L, codepoints[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    free(codepoints);
    return 1;
}

// unicode.lower(codepoint_table) -> lowercase_table
static int lua_unicode_lower(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (len == 0) {
        lua_newtable(L);
        return 1;
    }
    
    // Allocate and extract codepoints
    int32_t* codepoints = (int32_t*)malloc(len * sizeof(int32_t));
    if (!codepoints) {
        return luaL_error(L, "Memory allocation failed");
    }
    
    for (int32_t i = 0; i < len; i++) {
        lua_rawgeti(L, 1, i + 1);
        codepoints[i] = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    
    // Convert to lowercase
    unicode_lower(codepoints, len);
    
    // Create result table
    lua_createtable(L, len, 0);
    for (int32_t i = 0; i < len; i++) {
        lua_pushinteger(L, codepoints[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    free(codepoints);
    return 1;
}

// unicode.len(codepoint_table) -> length
static int lua_unicode_len(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushinteger(L, lua_objlen(L, 1));
    return 1;
}

// unicode.concat(table1, table2, ...) -> concatenated_table
static int lua_unicode_concat(lua_State* L) {
    int nargs = lua_gettop(L);
    
    // Calculate total length
    int32_t total_len = 0;
    for (int i = 1; i <= nargs; i++) {
        luaL_checktype(L, i, LUA_TTABLE);
        total_len += (int32_t)lua_objlen(L, i);
    }
    
    // Create result table
    lua_createtable(L, total_len, 0);
    
    int32_t pos = 1;
    for (int i = 1; i <= nargs; i++) {
        int32_t len = (int32_t)lua_objlen(L, i);
        for (int32_t j = 1; j <= len; j++) {
            lua_rawgeti(L, i, j);
            lua_rawseti(L, -2, pos++);
        }
    }
    
    return 1;
}

// unicode.reverse(codepoint_table) -> reversed_table
static int lua_unicode_reverse(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    lua_createtable(L, len, 0);
    
    for (int32_t i = 0; i < len; i++) {
        lua_rawgeti(L, 1, len - i);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

// unicode.chr(codepoint) -> single-character table
static int lua_unicode_chr(lua_State* L) {
    int32_t codepoint = (int32_t)luaL_checkinteger(L, 1);
    
    if (!unicode_is_valid_codepoint(codepoint)) {
        return luaL_error(L, "Invalid Unicode codepoint: %d", codepoint);
    }
    
    lua_createtable(L, 1, 0);
    lua_pushinteger(L, codepoint);
    lua_rawseti(L, -2, 1);
    
    return 1;
}

// unicode.asc(codepoint_table) -> first_codepoint
static int lua_unicode_asc(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    
    if (lua_objlen(L, 1) == 0) {
        return luaL_error(L, "Empty string in ASC");
    }
    
    lua_rawgeti(L, 1, 1);
    return 1;
}

// unicode.left(codepoint_table, n) -> substring
static int lua_unicode_left(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int32_t n = (int32_t)luaL_checkinteger(L, 2);
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (n <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    if (n > len) n = len;
    
    lua_createtable(L, n, 0);
    for (int32_t i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        lua_rawseti(L, -2, i);
    }
    
    return 1;
}

// unicode.right(codepoint_table, n) -> substring
static int lua_unicode_right(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int32_t n = (int32_t)luaL_checkinteger(L, 2);
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (n <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    if (n > len) n = len;
    
    int32_t start = len - n + 1;
    lua_createtable(L, n, 0);
    for (int32_t i = 0; i < n; i++) {
        lua_rawgeti(L, 1, start + i);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

// unicode.mid(codepoint_table, start [, length]) -> substring
static int lua_unicode_mid(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int32_t start = (int32_t)luaL_checkinteger(L, 2);
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    int32_t length = len - start + 1;
    if (lua_gettop(L) >= 3) {
        length = (int32_t)luaL_checkinteger(L, 3);
    }
    
    if (start < 1 || start > len || length <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    if (start + length - 1 > len) {
        length = len - start + 1;
    }
    
    lua_createtable(L, length, 0);
    for (int32_t i = 0; i < length; i++) {
        lua_rawgeti(L, 1, start + i);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

// unicode.space(n) -> string of n spaces
static int lua_unicode_space(lua_State* L) {
    int32_t n = (int32_t)luaL_checkinteger(L, 1);
    
    if (n <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    lua_createtable(L, n, 0);
    for (int32_t i = 1; i <= n; i++) {
        lua_pushinteger(L, 32);  // Space character
        lua_rawseti(L, -2, i);
    }
    
    return 1;
}

// unicode.string_repeat(count, codepoint) -> repeated string
static int lua_unicode_string_repeat(lua_State* L) {
    int32_t count = (int32_t)luaL_checkinteger(L, 1);
    int32_t codepoint = (int32_t)luaL_checkinteger(L, 2);
    
    if (count <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    lua_createtable(L, count, 0);
    
    for (int32_t i = 1; i <= count; i++) {
        lua_pushinteger(L, codepoint);
        lua_rawseti(L, -2, i);
    }
    
    return 1;
}

// Helper function to check if codepoint is whitespace
static bool is_space_codepoint(int32_t cp) {
    return cp == 32 || cp == 9 || cp == 10 || cp == 13;
}

// unicode.ltrim(codepoint_table) -> trimmed table
static int lua_unicode_ltrim(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (len == 0) {
        lua_newtable(L);
        return 1;
    }
    
    // Find first non-space character
    int32_t start = 1;
    while (start <= len) {
        lua_rawgeti(L, 1, start);
        int32_t cp = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (!is_space_codepoint(cp)) {
            break;
        }
        start++;
    }
    
    if (start > len) {
        lua_newtable(L);
        return 1;
    }
    
    // Create result table
    lua_createtable(L, len - start + 1, 0);
    int32_t pos = 1;
    for (int32_t i = start; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        lua_rawseti(L, -2, pos++);
    }
    
    return 1;
}

// unicode.rtrim(codepoint_table) -> trimmed table
static int lua_unicode_rtrim(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (len == 0) {
        lua_newtable(L);
        return 1;
    }
    
    // Find last non-space character
    int32_t end = len;
    while (end >= 1) {
        lua_rawgeti(L, 1, end);
        int32_t cp = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (!is_space_codepoint(cp)) {
            break;
        }
        end--;
    }
    
    if (end < 1) {
        lua_newtable(L);
        return 1;
    }
    
    // Create result table
    lua_createtable(L, end, 0);
    for (int32_t i = 1; i <= end; i++) {
        lua_rawgeti(L, 1, i);
        lua_rawseti(L, -2, i);
    }
    
    return 1;
}

// unicode.trim(codepoint_table) -> trimmed table
static int lua_unicode_trim(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int32_t len = (int32_t)lua_objlen(L, 1);
    
    if (len == 0) {
        lua_newtable(L);
        return 1;
    }
    
    // Find first non-space character
    int32_t start = 1;
    while (start <= len) {
        lua_rawgeti(L, 1, start);
        int32_t cp = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (!is_space_codepoint(cp)) {
            break;
        }
        start++;
    }
    
    if (start > len) {
        lua_newtable(L);
        return 1;
    }
    
    // Find last non-space character
    int32_t end = len;
    while (end >= start) {
        lua_rawgeti(L, 1, end);
        int32_t cp = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (!is_space_codepoint(cp)) {
            break;
        }
        end--;
    }
    
    // Create result table
    lua_createtable(L, end - start + 1, 0);
    int32_t pos = 1;
    for (int32_t i = start; i <= end; i++) {
        lua_rawgeti(L, 1, i);
        lua_rawseti(L, -2, pos++);
    }
    
    return 1;
}

// unicode.instr(haystack_table, needle_table) -> position or 0
static int lua_unicode_instr(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    
    int32_t haystack_len = (int32_t)lua_objlen(L, 1);
    int32_t needle_len = (int32_t)lua_objlen(L, 2);
    
    if (needle_len == 0) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    // Search for needle in haystack
    for (int32_t i = 1; i <= haystack_len - needle_len + 1; i++) {
        bool match = true;
        for (int32_t j = 1; j <= needle_len; j++) {
            lua_rawgeti(L, 1, i + j - 1);
            int32_t h_cp = (int32_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            lua_rawgeti(L, 2, j);
            int32_t n_cp = (int32_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            if (h_cp != n_cp) {
                match = false;
                break;
            }
        }
        if (match) {
            lua_pushinteger(L, i);
            return 1;
        }
    }
    
    lua_pushinteger(L, 0);
    return 1;
}

// unicode.instr_start(start, haystack_table, needle_table) -> position or 0
static int lua_unicode_instr_start(lua_State* L) {
    int32_t start = (int32_t)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);
    
    int32_t haystack_len = (int32_t)lua_objlen(L, 2);
    int32_t needle_len = (int32_t)lua_objlen(L, 3);
    
    if (start < 1) start = 1;
    if (needle_len == 0 || start > haystack_len) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    // Search for needle in haystack starting from position start
    for (int32_t i = start; i <= haystack_len - needle_len + 1; i++) {
        bool match = true;
        for (int32_t j = 1; j <= needle_len; j++) {
            lua_rawgeti(L, 2, i + j - 1);
            int32_t h_cp = (int32_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            lua_rawgeti(L, 3, j);
            int32_t n_cp = (int32_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            if (h_cp != n_cp) {
                match = false;
                break;
            }
        }
        if (match) {
            lua_pushinteger(L, i);
            return 1;
        }
    }
    
    lua_pushinteger(L, 0);
    return 1;
}

// unicode.version() -> version string
static int lua_unicode_version(lua_State* L) {
    lua_pushstring(L, unicode_version());
    return 1;
}

// =============================================================================
// Module Registration
// =============================================================================

static const luaL_Reg unicode_functions[] = {
    {"from_utf8", lua_unicode_from_utf8},
    {"to_utf8", lua_unicode_to_utf8},
    {"upper", lua_unicode_upper},
    {"lower", lua_unicode_lower},
    {"len", lua_unicode_len},
    {"concat", lua_unicode_concat},
    {"reverse", lua_unicode_reverse},
    {"chr", lua_unicode_chr},
    {"asc", lua_unicode_asc},
    {"left", lua_unicode_left},
    {"right", lua_unicode_right},
    {"mid", lua_unicode_mid},
    {"space", lua_unicode_space},
    {"string_repeat", lua_unicode_string_repeat},
    {"ltrim", lua_unicode_ltrim},
    {"rtrim", lua_unicode_rtrim},
    {"trim", lua_unicode_trim},
    {"instr", lua_unicode_instr},
    {"instr_start", lua_unicode_instr_start},
    {"version", lua_unicode_version},
    {NULL, NULL}
};

// Register unicode module in Lua state
extern "C" int luaopen_unicode(lua_State* L) {
    lua_newtable(L);
    luaL_register(L, NULL, unicode_functions);
    
    // Set available flag
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "available");
    
    return 1;
}

// Inject unicode module into global namespace
extern "C" void register_unicode_module(lua_State* L) {
    // Create the module table
    lua_newtable(L);
    luaL_register(L, NULL, unicode_functions);
    
    // Set available flag
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "available");
    
    // Register as global "unicode"
    lua_setglobal(L, "unicode");
}