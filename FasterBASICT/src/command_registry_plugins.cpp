//
// command_registry_plugins.cpp
// FasterBASICT - Plugin Management Commands
//
// Provides BASIC commands for runtime plugin loading and management.
// These commands allow BASIC programs to dynamically load plugins.
//

#include "command_registry_plugins.h"
#include "plugin_loader.h"
#include "modular_commands.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

#ifdef ENABLE_LUA_BINDINGS
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <limits.h>
#endif

namespace fs = std::filesystem;

namespace FasterBASIC {
namespace ModularCommands {

// =============================================================================
// Plugin Management Command Registration
// =============================================================================

void PluginCommandRegistry::registerPluginCommands(CommandRegistry& registry) {
    // LOADPLUGINS - Load all plugins from enabled directory
    CommandDefinition loadplugins("LOADPLUGINS",
                                  "Load all plugins from plugins/enabled directory",
                                  "loadplugins_handler", "plugins");
    registry.registerCommand(std::move(loadplugins));
    
    // LOADPLUGIN - Load plugins matching a pattern
    CommandDefinition loadplugin("LOADPLUGIN",
                                 "Load plugins matching a pattern (e.g., \"datetime*\" or \"math_plugin.dylib\")",
                                 "loadplugin_handler", "plugins");
    loadplugin.addParameter("pattern", ParameterType::STRING, "Plugin filename pattern to match");
    registry.registerCommand(std::move(loadplugin));
}

// =============================================================================
// Plugin Loading Implementation Helpers
// =============================================================================

namespace {

// Helper function to check if a filename matches a pattern
// Supports simple wildcard matching: * matches any sequence of characters
bool matchesPattern(const std::string& filename, const std::string& pattern) {
    // If pattern has no wildcards, do exact match (case-insensitive)
    if (pattern.find('*') == std::string::npos) {
        std::string lowerFilename = filename;
        std::string lowerPattern = pattern;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        return lowerFilename == lowerPattern;
    }
    
    // Simple wildcard matching
    std::string lowerFilename = filename;
    std::string lowerPattern = pattern;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
    
    size_t patternPos = 0;
    size_t filenamePos = 0;
    size_t lastWildcardPos = std::string::npos;
    size_t lastMatchPos = 0;
    
    while (filenamePos < lowerFilename.length()) {
        if (patternPos < lowerPattern.length() && lowerPattern[patternPos] == '*') {
            // Remember wildcard position for backtracking
            lastWildcardPos = patternPos;
            lastMatchPos = filenamePos;
            patternPos++;
        } else if (patternPos < lowerPattern.length() && 
                   (lowerPattern[patternPos] == lowerFilename[filenamePos] || lowerPattern[patternPos] == '?')) {
            // Character matches or '?' wildcard
            patternPos++;
            filenamePos++;
        } else if (lastWildcardPos != std::string::npos) {
            // Backtrack to last wildcard and try matching more characters
            patternPos = lastWildcardPos + 1;
            lastMatchPos++;
            filenamePos = lastMatchPos;
        } else {
            // No match
            return false;
        }
    }
    
    // Skip trailing wildcards in pattern
    while (patternPos < lowerPattern.length() && lowerPattern[patternPos] == '*') {
        patternPos++;
    }
    
    return patternPos == lowerPattern.length();
}

// Get the enabled plugins directory path
std::string getEnabledPluginsPath() {
    PluginSystem::PluginLoader& loader = PluginSystem::getGlobalPluginLoader();
    return loader.getEnabledPluginsDirectory();
}

// Get the plugins base directory from the app bundle
std::string getPluginsBasePath() {
#ifdef __APPLE__
    // On macOS, try to get the path from the app bundle
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
        CFURLRef resourceURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
        if (resourceURL) {
            char path[PATH_MAX];
            if (CFURLGetFileSystemRepresentation(resourceURL, TRUE, (UInt8 *)path, PATH_MAX)) {
                CFRelease(resourceURL);
                return std::string(path) + "/plugins";
            }
            CFRelease(resourceURL);
        }
    }
#endif
    // Fallback to relative path
    return "plugins";
}

} // anonymous namespace

// =============================================================================
// Runtime Plugin Loading Functions (called from Lua runtime)
// =============================================================================

// These functions are called by the Lua runtime when LOADPLUGINS/LOADPLUGIN
// commands are executed in a BASIC program.

#ifdef ENABLE_LUA_BINDINGS

extern "C" {

// LOADPLUGINS implementation - loads all plugins from enabled directory
int loadplugins_handler(lua_State* L) {
    try {
        PluginSystem::PluginLoader& loader = PluginSystem::getGlobalPluginLoader();
        CommandRegistry& registry = getGlobalCommandRegistry();
        
        // Set the base directory to the app bundle plugins path
        std::string pluginsPath = getPluginsBasePath();
        loader.setBaseDirectory(pluginsPath);
        
        std::cout << "Loading plugins from: " << pluginsPath << std::endl;
        
        // Load all enabled plugins
        int count = loader.loadEnabledPlugins(registry);
        
        if (count > 0) {
            std::cout << "Loaded " << count << " plugin(s)" << std::endl;
        } else {
            std::cout << "No plugins loaded" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading plugins: " << e.what() << std::endl;
    }
    return 0;
}

// LOADPLUGIN implementation - loads plugins matching a pattern
int loadplugin_handler(lua_State* L) {
    const char* pattern = luaL_checkstring(L, 1);
    
    if (!pattern || pattern[0] == '\0') {
        std::cerr << "Error: LOADPLUGIN requires a pattern parameter" << std::endl;
        return 0;
    }
    
    try {
        PluginSystem::PluginLoader& loader = PluginSystem::getGlobalPluginLoader();
        CommandRegistry& registry = getGlobalCommandRegistry();
        
        // Set the base directory to the app bundle plugins path
        std::string pluginsPath = getPluginsBasePath();
        loader.setBaseDirectory(pluginsPath);
        
        std::string enabledDir = getEnabledPluginsPath();
        
        if (!fs::exists(enabledDir) || !fs::is_directory(enabledDir)) {
            std::cerr << "Error: Plugin directory not found: " << enabledDir << std::endl;
            return 0;
        }
        
        // Scan for matching plugins
        std::vector<std::string> matchingPlugins;
        
        for (const auto& entry : fs::directory_iterator(enabledDir)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filename = entry.path().filename().string();
            
            // Check if filename matches pattern
            if (matchesPattern(filename, pattern)) {
                matchingPlugins.push_back(entry.path().string());
            }
        }
        
        if (matchingPlugins.empty()) {
            std::cout << "No plugins match pattern: " << pattern << std::endl;
            return 0;
        }
        
        // Load matching plugins
        int loadedCount = 0;
        std::vector<std::string> loadedNames;
        
        for (const auto& pluginPath : matchingPlugins) {
            if (loader.loadPlugin(pluginPath, registry, true)) {
                loadedCount++;
                std::string filename = fs::path(pluginPath).filename().string();
                loadedNames.push_back(filename);
            }
        }
        
        // Report results
        if (loadedCount > 0) {
            std::cout << "Loaded " << loadedCount << " plugin(s) matching \"" << pattern << "\":" << std::endl;
            for (const auto& name : loadedNames) {
                std::cout << "  - " << name << std::endl;
            }
        } else {
            std::cout << "Failed to load plugins matching pattern: " << pattern << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading plugin: " << e.what() << std::endl;
    }
    return 0;
}

} // extern "C"

#else // !ENABLE_LUA_BINDINGS

// Stub implementations when Lua is not available
struct lua_State;
extern "C" {
    int loadplugins_handler(lua_State* L) { return 0; }
    int loadplugin_handler(lua_State* L) { return 0; }
}

#endif // ENABLE_LUA_BINDINGS

} // namespace ModularCommands
} // namespace FasterBASIC

// =============================================================================
// Lua Runtime Registration
// =============================================================================

#ifdef ENABLE_LUA_BINDINGS

// Register plugin command handlers with Lua runtime
extern "C" void registerPluginLuaBindings(lua_State* L) {
    lua_register(L, "loadplugins_handler", loadplugins_handler);
    lua_register(L, "loadplugin_handler", loadplugin_handler);
    
    // Also register uppercase aliases for consistency with other BASIC commands
    lua_register(L, "LOADPLUGINS", loadplugins_handler);
}

#endif // ENABLE_LUA_BINDINGS