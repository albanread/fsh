//
// constants_lua_bindings.cpp
// FasterBASIC Runtime - Lua Bindings for Constants Manager
//
// Provides Lua interface to the C++ ConstantsManager for efficient constant storage.
//

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "ConstantsManager.h"
#include <memory>

using namespace FasterBASIC;

// Global constants manager instance
static std::unique_ptr<ConstantsManager> g_constantsManager;

// Initialize constants manager
static int lua_const_init(lua_State* L) {
    if (!g_constantsManager) {
        g_constantsManager = std::make_unique<ConstantsManager>();
        // Add predefined constants
        g_constantsManager->addPredefinedConstants();
    }
    return 0;
}

// Set a constant value (integer)
// Usage: const_set_int(index, value)
static int lua_const_set_int(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    int64_t value = static_cast<int64_t>(luaL_checkinteger(L, 2));
    
    if (!g_constantsManager) {
        g_constantsManager = std::make_unique<ConstantsManager>();
    }
    
    // Ensure index is valid by growing the vector if needed
    while (index >= static_cast<int>(g_constantsManager->getConstantCount())) {
        g_constantsManager->addConstant("_temp_" + std::to_string(g_constantsManager->getConstantCount()), static_cast<int64_t>(0));
    }
    
    // Now set the value at the index (we'll need to track this properly)
    // For now, just add with a generated name
    g_constantsManager->addConstant("const_" + std::to_string(index), value);
    
    return 0;
}

// Set a constant value (double)
// Usage: const_set_double(index, value)
static int lua_const_set_double(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    double value = luaL_checknumber(L, 2);
    
    if (!g_constantsManager) {
        g_constantsManager = std::make_unique<ConstantsManager>();
    }
    
    g_constantsManager->addConstant("const_" + std::to_string(index), value);
    
    return 0;
}

// Set a constant value (string)
// Usage: const_set_string(index, value)
static int lua_const_set_string(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    const char* value = luaL_checkstring(L, 2);
    
    if (!g_constantsManager) {
        g_constantsManager = std::make_unique<ConstantsManager>();
    }
    
    g_constantsManager->addConstant("const_" + std::to_string(index), std::string(value));
    
    return 0;
}

// Get a constant value as integer
// Usage: value = const_get_int(index)
static int lua_const_get_int(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    
    if (!g_constantsManager) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    try {
        int64_t value = g_constantsManager->getConstantAsInt(index);
        lua_pushinteger(L, static_cast<lua_Integer>(value));
    } catch (...) {
        lua_pushinteger(L, 0);
    }
    
    return 1;
}

// Get a constant value as double
// Usage: value = const_get_double(index)
static int lua_const_get_double(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    
    if (!g_constantsManager) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    
    try {
        double value = g_constantsManager->getConstantAsDouble(index);
        lua_pushnumber(L, value);
    } catch (...) {
        lua_pushnumber(L, 0.0);
    }
    
    return 1;
}

// Get a constant value as string
// Usage: value = const_get_string(index)
static int lua_const_get_string(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    
    if (!g_constantsManager) {
        lua_pushstring(L, "");
        return 1;
    }
    
    try {
        std::string value = g_constantsManager->getConstantAsString(index);
        lua_pushstring(L, value.c_str());
    } catch (...) {
        lua_pushstring(L, "");
    }
    
    return 1;
}

// Get a constant index by name
// Usage: index = const_get_index(name)
static int lua_const_get_index(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    
    if (!g_constantsManager) {
        lua_pushinteger(L, -1);
        return 1;
    }
    
    int index = g_constantsManager->getConstantIndex(name);
    lua_pushinteger(L, index);
    
    return 1;
}

// Clear all constants
// Usage: const_clear()
static int lua_const_clear(lua_State* L) {
    if (g_constantsManager) {
        g_constantsManager->clear();
    }
    return 0;
}

// Register functions
static const struct luaL_Reg constants_functions[] = {
    {"const_init", lua_const_init},
    {"const_set_int", lua_const_set_int},
    {"const_set_double", lua_const_set_double},
    {"const_set_string", lua_const_set_string},
    {"const_get_int", lua_const_get_int},
    {"const_get_double", lua_const_get_double},
    {"const_get_string", lua_const_get_string},
    {"const_get_index", lua_const_get_index},
    {"const_clear", lua_const_clear},
    {NULL, NULL}
};

// Get a constant value (auto-detect type)
// Usage: value = constants_get(index)
static int lua_constants_get(lua_State* L) {
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    
    if (!g_constantsManager) {
        lua_pushnil(L);
        return 1;
    }
    
    try {
        ConstantValue value = g_constantsManager->getConstant(index);
        
        if (std::holds_alternative<int64_t>(value)) {
            lua_pushinteger(L, static_cast<lua_Integer>(std::get<int64_t>(value)));
        } else if (std::holds_alternative<double>(value)) {
            lua_pushnumber(L, std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            lua_pushstring(L, std::get<std::string>(value).c_str());
        } else {
            lua_pushnil(L);
        }
    } catch (...) {
        lua_pushnil(L);
    }
    
    return 1;
}

// Module initialization
extern "C" int luaopen_constants(lua_State* L) {
    // Register functions in global namespace (LuaJIT compatible)
    #if LUA_VERSION_NUM >= 502
        luaL_setfuncs(L, constants_functions, 0);
    #else
        luaL_register(L, NULL, constants_functions);
    #endif
    
    // Initialize the constants manager
    lua_const_init(L);
    
    return 0;
}

// Set the global ConstantsManager from an external one (for fbc compilation)
extern "C" void set_constants_manager(ConstantsManager* manager) {
    if (!g_constantsManager) {
        g_constantsManager = std::make_unique<ConstantsManager>();
    }
    
    // Copy all constants from the external manager to the global one
    // Use copyFrom to preserve indices
    if (manager) {
        g_constantsManager->copyFrom(*manager);
    }
}

// Inject constants module into Lua state (for embedded use in fbc)
extern "C" void register_constants_module(lua_State* L) {
    if (!g_constantsManager) {
        g_constantsManager = std::make_unique<ConstantsManager>();
        g_constantsManager->addPredefinedConstants();
    }
    
    // Register constants_get function globally
    lua_pushcfunction(L, lua_constants_get);
    lua_setglobal(L, "constants_get");
    
    // Register constants_init function globally
    lua_pushcfunction(L, lua_const_init);
    lua_setglobal(L, "constants_init");
    
    // Call init to set up the manager
    lua_const_init(L);
}