//
// command_registry_plugins.h
// FasterBASICT - Plugin Management Commands
//
// Provides BASIC commands for runtime plugin loading and management.
// These commands allow BASIC programs to dynamically load plugins.
//

#ifndef FASTERBASIC_COMMAND_REGISTRY_PLUGINS_H
#define FASTERBASIC_COMMAND_REGISTRY_PLUGINS_H

#include "modular_commands.h"

struct lua_State; // Forward declaration

namespace FasterBASIC {
namespace ModularCommands {

// =============================================================================
// Plugin Management Command Registration
// =============================================================================

class PluginCommandRegistry {
public:
    // Register all plugin management commands
    static void registerPluginCommands(CommandRegistry& registry);
};

} // namespace ModularCommands
} // namespace FasterBASIC

// =============================================================================
// Lua Runtime Registration
// =============================================================================

extern "C" {
    // Register plugin command handlers with Lua runtime
    void registerPluginLuaBindings(lua_State* L);
}

// =============================================================================
// Runtime Plugin Loading Functions (C linkage for Lua runtime)
// =============================================================================

#ifdef ENABLE_LUA_BINDINGS
extern "C" {
    // LOADPLUGINS - Load all plugins from plugins/enabled directory
    int loadplugins_handler(lua_State* L);
    
    // LOADPLUGIN - Load plugins matching a pattern (takes pattern as Lua arg)
    int loadplugin_handler(lua_State* L);
}
#endif // ENABLE_LUA_BINDINGS

#endif // FASTERBASIC_COMMAND_REGISTRY_PLUGINS_H