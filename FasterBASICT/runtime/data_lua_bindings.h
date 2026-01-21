//
// data_lua_bindings.h
// FasterBASIC Runtime - DATA/READ/RESTORE Lua Bindings
//
// Provides Lua bindings for DATA/READ/RESTORE functionality.
// These bindings are used by compiled BASIC code to access data values.
//

#ifndef DATA_LUA_BINDINGS_H
#define DATA_LUA_BINDINGS_H

#include <string>
#include <vector>
#include <lua.hpp>

namespace FasterBASIC {

// =============================================================================
// DATA/READ/RESTORE Management Functions
// =============================================================================

// Initialize the data manager with DATA values
void initializeDataManager(const std::vector<std::string>& values);

// Add restore point by line number
void addDataRestorePoint(int lineNumber, size_t index);

// Add restore point by label name
void addDataRestorePointByLabel(const std::string& labelName, size_t index);

// Clear all data (call at end of script or before new script)
void clearDataManager();

// =============================================================================
// Lua Registration Function
// =============================================================================

// Register all DATA/READ/RESTORE Lua bindings
void registerDataBindings(lua_State* L);

} // namespace FasterBASIC

#endif // DATA_LUA_BINDINGS_H