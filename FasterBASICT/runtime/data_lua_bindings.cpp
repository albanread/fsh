//
// data_lua_bindings.cpp
// FasterBASIC Runtime - DATA/READ/RESTORE Lua Bindings
//
// Provides Lua bindings for DATA/READ/RESTORE functionality.
// These bindings are used by compiled BASIC code to access data values.
//

#include "data_lua_bindings.h"
#include "DataManager.h"
#include <lua.hpp>
#include <stdexcept>

namespace FasterBASIC {

// Global DataManager instance
static DataManager g_dataManager;

// =============================================================================
// DATA/READ/RESTORE Management Functions
// =============================================================================

void initializeDataManager(const std::vector<std::string>& values) {
    g_dataManager.initialize(values);
}

void addDataRestorePoint(int lineNumber, size_t index) {
    g_dataManager.addRestorePoint(lineNumber, index);
}

void addDataRestorePointByLabel(const std::string& labelName, size_t index) {
    g_dataManager.addRestorePointByLabel(labelName, index);
}

void clearDataManager() {
    g_dataManager.clear();
}

// =============================================================================
// Lua Binding Functions
// =============================================================================

static int lua_basic_read_data(lua_State* L) {
    try {
        double value = g_dataManager.readDouble();
        lua_pushnumber(L, value);
        return 1;
    } catch (const OutOfDataError&) {
        return luaL_error(L, "OUT OF DATA");
    }
}

static int lua_basic_read_data_string(lua_State* L) {
    try {
        std::string value = g_dataManager.readString();
        lua_pushstring(L, value.c_str());
        return 1;
    } catch (const OutOfDataError&) {
        return luaL_error(L, "OUT OF DATA");
    }
}

static int lua_basic_restore(lua_State* L) {
    // Check what type of argument we got
    int argType = lua_type(L, 1);
    
    if (argType == LUA_TNONE || argType == LUA_TNIL) {
        // No argument - restore to beginning
        g_dataManager.restore();
    } else if (argType == LUA_TNUMBER) {
        // Number - restore to line number
        int lineNumber = lua_tointeger(L, 1);
        g_dataManager.restoreToLine(lineNumber);
    } else if (argType == LUA_TSTRING) {
        // String - restore to label
        const char* labelName = lua_tostring(L, 1);
        g_dataManager.restoreToLabel(labelName);
    } else {
        return luaL_error(L, "RESTORE expects line number (integer), label (string), or no argument");
    }
    
    return 0;
}

// =============================================================================
// Registration Function
// =============================================================================

void registerDataBindings(lua_State* L) {
    // Register DATA/READ/RESTORE functions
    lua_register(L, "basic_read_data", lua_basic_read_data);
    lua_register(L, "basic_read_data_string", lua_basic_read_data_string);
    lua_register(L, "basic_restore", lua_basic_restore);
}

} // namespace FasterBASIC