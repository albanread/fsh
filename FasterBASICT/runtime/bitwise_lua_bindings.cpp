//
// bitwise_lua_bindings.cpp
// FasterBASIC - Bitwise Runtime Lua C API Bindings
//
// Provides Lua C API wrappers for bitwise functions
// These can be injected directly into the Lua state without FFI
//

#include "basic_bitwise.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// =============================================================================
// Lua C API Wrapper Functions
// =============================================================================

// bitwise.band(a, b) -> bitwise AND
static int lua_bitwise_band(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_band(a, b));
    return 1;
}

// bitwise.bor(a, b) -> bitwise OR
static int lua_bitwise_bor(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_bor(a, b));
    return 1;
}

// bitwise.bxor(a, b) -> bitwise XOR
static int lua_bitwise_bxor(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_bxor(a, b));
    return 1;
}

// bitwise.bnot(a) -> bitwise NOT
static int lua_bitwise_bnot(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    lua_pushinteger(L, basic_bnot(a));
    return 1;
}

// bitwise.beqv(a, b) -> bitwise EQV (equivalence)
static int lua_bitwise_beqv(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_beqv(a, b));
    return 1;
}

// bitwise.bimp(a, b) -> bitwise IMP (implication)
static int lua_bitwise_bimp(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_bimp(a, b));
    return 1;
}

// bitwise.shl(a, b) -> left shift
static int lua_bitwise_shl(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_shl(a, b));
    return 1;
}

// bitwise.shr(a, b) -> right shift (arithmetic)
static int lua_bitwise_shr(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushinteger(L, basic_shr(a, b));
    return 1;
}

// =============================================================================
// Module Registration
// =============================================================================

static const luaL_Reg bitwise_functions[] = {
    {"band", lua_bitwise_band},
    {"bor", lua_bitwise_bor},
    {"bxor", lua_bitwise_bxor},
    {"bnot", lua_bitwise_bnot},
    {"beqv", lua_bitwise_beqv},
    {"bimp", lua_bitwise_bimp},
    {"shl", lua_bitwise_shl},
    {"shr", lua_bitwise_shr},
    {NULL, NULL}
};

// Register bitwise module in Lua state
extern "C" int luaopen_bitwise(lua_State* L) {
    lua_newtable(L);
    luaL_register(L, NULL, bitwise_functions);
    
    // Set available flag
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "available");
    
    return 1;
}

// Inject bitwise module into global namespace
extern "C" void register_bitwise_module(lua_State* L) {
    // Create the module table
    lua_newtable(L);
    luaL_register(L, NULL, bitwise_functions);
    
    // Set available flag
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "available");
    
    // Register as global "bitwise"
    lua_setglobal(L, "bitwise");
}