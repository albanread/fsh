//
// plugin_loader.h
// FasterBASICT - Plugin Loader for Dynamic Command Extensions
//
// Provides dynamic loading of plugin libraries from the plugins/enabled directory.
// Plugins can be disabled by moving them to plugins/disabled without deletion.
//

#ifndef FASTERBASIC_PLUGIN_LOADER_H
#define FASTERBASIC_PLUGIN_LOADER_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "modular_commands.h"
#include "plugin_interface.h"

namespace FasterBASIC {
namespace PluginSystem {

// =============================================================================
// Plugin Information Structure
// =============================================================================

struct PluginInfo {
    std::string name;                    // Plugin name (from FB_PLUGIN_NAME)
    std::string version;                 // Plugin version (from FB_PLUGIN_VERSION)
    std::string description;             // Plugin description
    std::string author;                  // Plugin author
    std::string filePath;                // Full path to plugin file
    std::string fileName;                // Just the filename
    std::vector<std::string> runtimeFiles; // Lua runtime files needed by plugin
    void* libraryHandle;                 // OS handle to loaded library
    int apiVersion;                      // Plugin API version
    bool isEnabled;                      // True if loaded from enabled/ folder
    bool loadedSuccessfully;             // True if init succeeded
    std::string loadError;               // Error message if load failed
    size_t commandCount;                 // Number of commands/functions added
    
    FB_PluginShutdownFunc shutdownFunc;  // Shutdown function pointer
    
    PluginInfo() 
        : name(""), version(""), description(""), author(""),
          filePath(""), fileName(""), libraryHandle(nullptr),
          apiVersion(0), isEnabled(false), loadedSuccessfully(false),
          loadError(""), commandCount(0), shutdownFunc(nullptr) {}
};

// =============================================================================
// Plugin Loader Class
// =============================================================================

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();
    
    // =========================================================================
    // Loading Plugins
    // =========================================================================
    
    // Load all plugins from plugins/enabled directory
    // Returns number of successfully loaded plugins
    int loadEnabledPlugins(ModularCommands::CommandRegistry& registry);
    
    // Load plugins from a specific directory
    int loadPluginsFromDirectory(const std::string& directory,
                                  ModularCommands::CommandRegistry& registry);
    
    // Load a single plugin file
    // Returns true if plugin loaded successfully
    bool loadPlugin(const std::string& filepath,
                   ModularCommands::CommandRegistry& registry,
                   bool isEnabled = true);
    
    // =========================================================================
    // Plugin Management
    // =========================================================================
    
    // Unload all loaded plugins (calls shutdown functions)
    void unloadAllPlugins();
    
    // Get list of all loaded plugins
    const std::vector<PluginInfo>& getLoadedPlugins() const;
    
    // Get list of all available plugins (enabled and disabled)
    std::vector<PluginInfo> getAllAvailablePlugins() const;
    
    // Get list of disabled plugins (scans plugins/disabled directory)
    std::vector<PluginInfo> getDisabledPlugins() const;
    
    // =========================================================================
    // Plugin Validation
    // =========================================================================
    
    // Validate a plugin has all required exports
    bool validatePlugin(void* handle) const;
    
    // Get plugin info from a file without loading it
    bool getPluginInfo(const std::string& filepath, PluginInfo& info) const;
    
    // =========================================================================
    // Directory Management
    // =========================================================================
    
    // Set the base plugins directory path (must be called before loading plugins)
    void setBaseDirectory(const std::string& baseDir);
    
    // Get the base plugins directory path
    std::string getPluginsDirectory() const;
    
    // Get the enabled plugins directory path
    std::string getEnabledPluginsDirectory() const;
    
    // Get the disabled plugins directory path
    std::string getDisabledPluginsDirectory() const;
    
    // Create plugin directories if they don't exist
    bool createPluginDirectories();
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    // Get number of loaded plugins
    size_t getLoadedPluginCount() const { return m_plugins.size(); }
    
    // Get number of failed plugin loads
    size_t getFailedPluginCount() const { return m_failedPlugins.size(); }
    
    // Get list of failed plugins with error messages
    const std::vector<PluginInfo>& getFailedPlugins() const { return m_failedPlugins; }
    
    // Get list of all runtime files needed by loaded plugins
    std::vector<std::string> getRequiredRuntimeFiles() const;
    
    // Get cached Lua runtime content for all plugins
    // Returns a map of filename -> file contents
    const std::map<std::string, std::string>& getCachedRuntimeContents() const { 
        return m_cachedRuntimeContents; 
    }
    
    // Set the runtime directory path (where plugin .lua files are located)
    void setRuntimeDirectory(const std::string& path);
    
    // Get the runtime directory path
    std::string getRuntimeDirectory() const { return m_runtimeDirectory; }
    
private:
    // =========================================================================
    // Internal Data
    // =========================================================================
    
    std::vector<PluginInfo> m_plugins;        // Successfully loaded plugins
    std::vector<PluginInfo> m_failedPlugins;  // Plugins that failed to load
    std::string m_baseDirectory;              // Base plugins directory
    std::string m_runtimeDirectory;           // Runtime files directory
    std::map<std::string, std::string> m_cachedRuntimeContents;  // Cached Lua runtime file contents
    
    // =========================================================================
    // Platform-Specific Dynamic Library Operations
    // =========================================================================
    
    // Load a dynamic library, returns handle or nullptr on failure
    void* loadLibrary(const std::string& filepath);
    
    // Get a symbol (function pointer) from a loaded library
    void* getSymbol(void* handle, const char* name) const;
    
    // Unload a dynamic library
    void unloadLibrary(void* handle);
    
    // Get the platform-specific library extension (.dylib, .so, or .dll)
    std::string getLibraryExtension() const;
    
    // Get the last error message from dynamic library operations
    std::string getLastLibraryError() const;
    
    // =========================================================================
    // Internal Helper Methods
    // =========================================================================
    
    // Scan a directory for plugin files
    std::vector<std::string> scanDirectoryForPlugins(const std::string& directory) const;
    
    // Extract plugin metadata from loaded library
    bool extractPluginMetadata(void* handle, PluginInfo& info) const;
    
    // Initialize a loaded plugin (call FB_PLUGIN_INIT)
    bool initializePlugin(void* handle, PluginInfo& info,
                         ModularCommands::CommandRegistry& registry);
    
    // Check if a file is a valid plugin library
    bool isPluginFile(const std::string& filename) const;
    
    // Add a plugin to the loaded list
    void addLoadedPlugin(const PluginInfo& info);
    
    // Add a plugin to the failed list
    void addFailedPlugin(const PluginInfo& info);
    
    // Load and cache a runtime file
    bool cacheRuntimeFile(const std::string& filename);
};

// =============================================================================
// Global Plugin Loader Access
// =============================================================================

// Get the global plugin loader instance
// Note: This is created and managed by the application (FBRunner3, fbc, fbsh)
// The global instance ensures plugins are loaded once and persist for the
// lifetime of the application.
PluginLoader& getGlobalPluginLoader();

// Initialize the global plugin loader with the command registry
// This should be called once at application startup, after the command
// registry has been initialized with core commands.
// Returns the number of successfully loaded plugins.
int initializeGlobalPluginLoader(ModularCommands::CommandRegistry& registry);

// Shutdown the global plugin loader
// This should be called at application shutdown to cleanly unload all plugins.
void shutdownGlobalPluginLoader();

} // namespace PluginSystem
} // namespace FasterBASIC

#endif // FASTERBASIC_PLUGIN_LOADER_H