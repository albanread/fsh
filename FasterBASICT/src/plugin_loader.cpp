//
// plugin_loader.cpp
// FasterBASICT - Plugin Loader Implementation
//
// Implements dynamic loading of plugin libraries with support for
// enabled/disabled plugin folders.
//
// =============================================================================
// IMPORTANT NOTES FOR PLUGIN DEVELOPERS
// =============================================================================
//
// How Plugin Runtime Files Are Loaded:
// ------------------------------------
// 1. Plugin runtime files are loaded with luaL_dofile() in the order specified
//    in FB_PLUGIN_BEGIN (comma-separated list of filenames).
//
// 2. Each runtime file is executed sequentially in the Lua state, so later
//    files can use functionality from earlier files.
//
// Critical Requirements for Multi-File Plugins:
// ---------------------------------------------
// If your plugin uses multiple Lua runtime files that depend on each other
// (e.g., parser.lua, engine.lua, api.lua), you MUST:
//
// 1. **Register modules at the TOP of each file** (before any code that might
//    require them):
//
//    local M = {}
//    package.loaded['module_name'] = M  -- Register IMMEDIATELY
//    -- Now other files can require('module_name')
//
// 2. **Define functions as GLOBAL**, not as table members:
//
//    Good:  function my_function()           -- Global, accessible from BASIC
//    Bad:   function M.my_function()         -- Table member, NOT accessible
//
//    The BASIC-generated Lua code expects to call functions by name directly
//    (e.g., my_function(), not M.my_function()).
//
// 3. **Avoid $ in BASIC function names**:
//
//    Good:  FB_BeginFunction(callbacks, "MYFUNC", ..., "my_func", FB_RETURN_STRING, ...)
//    Bad:   FB_BeginFunction(callbacks, "MYFUNC$", ..., "my_func", FB_RETURN_STRING, ...)
//
//    The compiler may transform function names ending with $, causing mismatches
//    between the expected Lua function name and the actual call.
//
// 4. **List runtime files in dependency order**:
//
//    FB_PLUGIN_BEGIN("MyPlugin", "1.0", "...", "...",
//                    "parser.lua,engine.lua,api.lua")  // parser first, api last
//
// Example Plugin Structure:
// -------------------------
// File: my_parser.lua
//   local M = {}
//   package.loaded['my_parser'] = M  -- Register immediately
//   function M.parse(text) ... end
//   return M
//
// File: my_engine.lua
//   local parser = require("my_parser")  -- Works because parser registered itself
//   local M = {}
//   package.loaded['my_engine'] = M
//   function M.execute(ast) ... end
//   return M
//
// File: my_plugin_runtime.lua
//   local engine = require("my_engine")
//   function my_command()  -- Global function, called from BASIC
//       engine.execute(...)
//   end
//   return { my_command = my_command }  -- Optional, for module system
//
// =============================================================================
//

#include "plugin_loader.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <map>
#include <fstream>

// Platform-specific includes for dynamic library loading
#ifdef _WIN32
    #include <windows.h>
    #define PLUGIN_EXT ".dll"
#elif __APPLE__
    #include <dlfcn.h>
    #define PLUGIN_EXT ".dylib"
#else
    #include <dlfcn.h>
    #define PLUGIN_EXT ".so"
#endif

namespace FasterBASIC {
namespace PluginSystem {

namespace fs = std::filesystem;

// =============================================================================
// Plugin Callback Implementation
// =============================================================================
// These callbacks are passed to plugins so they can register commands
// without linking against the CommandRegistry

// Helper structure to track command being built
struct CommandInProgress {
    ModularCommands::CommandDefinition* definition;
    bool isValid;
};

// Map of command IDs to definitions being built
static std::map<int, CommandInProgress> g_commandsInProgress;
static int g_nextCommandId = 1;

// Callback: Begin registering a command
static int Plugin_BeginCommand(void* userData, const char* name, const char* description,
                              const char* luaFunction, const char* category) {
    if (!userData || !name || !description || !luaFunction || !category) {
        return -1;
    }
    
    int cmdId = g_nextCommandId++;
    
    auto* def = new ModularCommands::CommandDefinition(
        name, description, luaFunction, category
    );
    
    g_commandsInProgress[cmdId] = CommandInProgress{ def, true };
    return cmdId;
}

// Callback: Begin registering a function
static int Plugin_BeginFunction(void* userData, const char* name, const char* description,
                               const char* luaFunction, const char* category, int returnType) {
    if (!userData || !name || !description || !luaFunction || !category) {
        return -1;
    }
    
    int cmdId = g_nextCommandId++;
    
    // Convert int to ReturnType enum
    ModularCommands::ReturnType retType = static_cast<ModularCommands::ReturnType>(returnType);
    
    auto* def = new ModularCommands::CommandDefinition(
        name, description, luaFunction, category, false, retType
    );
    
    g_commandsInProgress[cmdId] = CommandInProgress{ def, true };
    return cmdId;
}

// Callback: Add parameter to command/function
static int Plugin_AddParameter(void* userData, int commandId, const char* name, int type,
                              const char* description, int isOptional, const char* defaultValue) {
    auto it = g_commandsInProgress.find(commandId);
    if (it == g_commandsInProgress.end() || !it->second.isValid) {
        return -1;
    }
    
    if (!name || !description) {
        return -1;
    }
    
    // Convert int to ParameterType enum
    ModularCommands::ParameterType paramType = static_cast<ModularCommands::ParameterType>(type);
    
    it->second.definition->addParameter(
        name, paramType, description, isOptional != 0, 
        defaultValue ? defaultValue : ""
    );
    
    return 0;
}

// Callback: End command registration and add to registry
static int Plugin_EndCommand(void* userData, int commandId) {
    auto it = g_commandsInProgress.find(commandId);
    if (it == g_commandsInProgress.end() || !it->second.isValid) {
        return -1;
    }
    
    if (!userData) {
        delete it->second.definition;
        g_commandsInProgress.erase(it);
        return -1;
    }
    
    auto* registry = static_cast<ModularCommands::CommandRegistry*>(userData);
    auto* def = it->second.definition;
    
    // Register the command or function
    if (def->isFunction) {
        registry->registerFunction(std::move(*def));
    } else {
        registry->registerCommand(std::move(*def));
    }
    
    delete def;
    g_commandsInProgress.erase(it);
    return 0;
}

// Callback: Set custom code generation template
static int Plugin_SetCustomCodeGen(void* userData, int commandId, const char* codeTemplate) {
    auto it = g_commandsInProgress.find(commandId);
    if (it == g_commandsInProgress.end() || !it->second.isValid) {
        return -1;
    }
    
    if (!codeTemplate) {
        return -1;
    }
    
    it->second.definition->setCustomCodeGen(codeTemplate);
    return 0;
}

// =============================================================================
// Global Plugin Loader Instance
// =============================================================================

static PluginLoader* g_pluginLoader = nullptr;

PluginLoader& getGlobalPluginLoader() {
    if (!g_pluginLoader) {
        g_pluginLoader = new PluginLoader();
    }
    return *g_pluginLoader;
}

int initializeGlobalPluginLoader(ModularCommands::CommandRegistry& registry) {
    PluginLoader& loader = getGlobalPluginLoader();
    return loader.loadEnabledPlugins(registry);
}

void shutdownGlobalPluginLoader() {
    if (g_pluginLoader) {
        g_pluginLoader->unloadAllPlugins();
        delete g_pluginLoader;
        g_pluginLoader = nullptr;
    }
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

PluginLoader::PluginLoader()
    : m_baseDirectory("plugins") {
    // Create plugin directories if they don't exist
    createPluginDirectories();
}

void PluginLoader::setBaseDirectory(const std::string& baseDir) {
    m_baseDirectory = baseDir;
}

PluginLoader::~PluginLoader() {
    unloadAllPlugins();
}

// =============================================================================
// Loading Plugins
// =============================================================================

int PluginLoader::loadEnabledPlugins(ModularCommands::CommandRegistry& registry) {
    std::string enabledDir = getEnabledPluginsDirectory();
    
    int loaded = loadPluginsFromDirectory(enabledDir, registry);
    
    // Print consolidated plugin loading message
    if (loaded > 0) {
        std::cout << "Loading plugins [";
        bool first = true;
        for (const auto& plugin : m_plugins) {
            if (plugin.loadedSuccessfully) {
                if (!first) std::cout << ", ";
                std::cout << plugin.name << " (" << plugin.commandCount << ")";
                first = false;
            }
        }
        std::cout << "]" << std::endl;
    }
    
    if (m_failedPlugins.size() > 0) {
        std::cerr << "Failed to load " << m_failedPlugins.size() << " plugin(s):" << std::endl;
        for (const auto& failed : m_failedPlugins) {
            std::cerr << "  - " << failed.fileName << ": " << failed.loadError << std::endl;
        }
    }
    
    return loaded;
}

int PluginLoader::loadPluginsFromDirectory(const std::string& directory,
                                           ModularCommands::CommandRegistry& registry) {
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        std::cerr << "Plugin directory not found: " << directory << std::endl;
        return 0;
    }
    
    std::vector<std::string> pluginFiles = scanDirectoryForPlugins(directory);
    int loadedCount = 0;
    
    for (const auto& filepath : pluginFiles) {
        if (loadPlugin(filepath, registry, true)) {
            loadedCount++;
        }
    }
    
    return loadedCount;
}

bool PluginLoader::loadPlugin(const std::string& filepath,
                              ModularCommands::CommandRegistry& registry,
                              bool isEnabled) {
    PluginInfo info;
    info.filePath = filepath;
    info.fileName = fs::path(filepath).filename().string();
    info.isEnabled = isEnabled;
    
    // Load the dynamic library
    void* handle = loadLibrary(filepath);
    if (!handle) {
        info.loadError = "Failed to load library: " + getLastLibraryError();
        addFailedPlugin(info);
        return false;
    }
    
    info.libraryHandle = handle;
    
    // Validate plugin has required exports
    if (!validatePlugin(handle)) {
        info.loadError = "Missing required exports (FB_PLUGIN_NAME, FB_PLUGIN_VERSION, FB_PLUGIN_INIT, or FB_PLUGIN_API_VERSION)";
        unloadLibrary(handle);
        addFailedPlugin(info);
        return false;
    }
    
    // Extract plugin metadata
    if (!extractPluginMetadata(handle, info)) {
        info.loadError = "Failed to extract plugin metadata";
        unloadLibrary(handle);
        addFailedPlugin(info);
        return false;
    }
    
    // Check API version compatibility
    if (info.apiVersion != FB_PLUGIN_API_VERSION_CURRENT) {
        info.loadError = "API version mismatch (expected " + 
                        std::to_string(FB_PLUGIN_API_VERSION_CURRENT) + 
                        ", got " + std::to_string(info.apiVersion) + ")";
        unloadLibrary(handle);
        addFailedPlugin(info);
        return false;
    }
    
    // Track command count before and after plugin initialization
    size_t commandsBefore = registry.getCommandCount();
    
    // Initialize the plugin
    if (!initializePlugin(handle, info, registry)) {
        // Error already set in initializePlugin
        unloadLibrary(handle);
        addFailedPlugin(info);
        return false;
    }
    
    // Calculate how many commands/functions this plugin added
    size_t commandsAfter = registry.getCommandCount();
    info.commandCount = commandsAfter - commandsBefore;
    
    // Store shutdown function pointer
    info.shutdownFunc = (FB_PluginShutdownFunc)getSymbol(handle, "FB_PLUGIN_SHUTDOWN");
    
    // Success!
    info.loadedSuccessfully = true;
    
    // Cache runtime files for this plugin
    for (const auto& runtimeFile : info.runtimeFiles) {
        if (!cacheRuntimeFile(runtimeFile)) {
            std::cerr << "Warning: Failed to cache runtime file: " << runtimeFile << std::endl;
        }
    }
    
    addLoadedPlugin(info);
    
    // Don't print individual plugin messages anymore - we'll print a consolidated message
    
    return true;
}

// =============================================================================
// Plugin Management
// =============================================================================

void PluginLoader::unloadAllPlugins() {
    // Unload in reverse order (in case there are dependencies)
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it) {
        // Call shutdown function if it exists
        if (it->shutdownFunc) {
            try {
                it->shutdownFunc();
            } catch (...) {
                std::cerr << "Exception in plugin shutdown: " << it->name << std::endl;
            }
        }
        
        // Unload the library
        if (it->libraryHandle) {
            unloadLibrary(it->libraryHandle);
        }
    }
    
    m_plugins.clear();
    m_failedPlugins.clear();
}

const std::vector<PluginInfo>& PluginLoader::getLoadedPlugins() const {
    return m_plugins;
}

std::vector<PluginInfo> PluginLoader::getAllAvailablePlugins() const {
    std::vector<PluginInfo> all;
    
    // Add loaded plugins
    all.insert(all.end(), m_plugins.begin(), m_plugins.end());
    
    // Add disabled plugins
    std::vector<PluginInfo> disabled = getDisabledPlugins();
    all.insert(all.end(), disabled.begin(), disabled.end());
    
    return all;
}

std::vector<PluginInfo> PluginLoader::getDisabledPlugins() const {
    std::vector<PluginInfo> disabled;
    std::string disabledDir = getDisabledPluginsDirectory();
    
    if (!fs::exists(disabledDir) || !fs::is_directory(disabledDir)) {
        return disabled;
    }
    
    std::vector<std::string> pluginFiles = scanDirectoryForPlugins(disabledDir);
    
    for (const auto& filepath : pluginFiles) {
        PluginInfo info;
        if (getPluginInfo(filepath, info)) {
            info.isEnabled = false;
            disabled.push_back(info);
        }
    }
    
    return disabled;
}

std::vector<std::string> PluginLoader::getRequiredRuntimeFiles() const {
    std::vector<std::string> allFiles;
    for (const auto& plugin : m_plugins) {
        if (plugin.loadedSuccessfully) {
            allFiles.insert(allFiles.end(), 
                          plugin.runtimeFiles.begin(), 
                          plugin.runtimeFiles.end());
        }
    }
    return allFiles;
}

// =============================================================================
// Plugin Validation
// =============================================================================

bool PluginLoader::validatePlugin(void* handle) const {
    if (!handle) return false;
    
    // Check for required exports
    return getSymbol(handle, "FB_PLUGIN_NAME") != nullptr &&
           getSymbol(handle, "FB_PLUGIN_VERSION") != nullptr &&
           getSymbol(handle, "FB_PLUGIN_INIT") != nullptr &&
           getSymbol(handle, "FB_PLUGIN_API_VERSION") != nullptr;
}

bool PluginLoader::getPluginInfo(const std::string& filepath, PluginInfo& info) const {
    info.filePath = filepath;
    info.fileName = fs::path(filepath).filename().string();
    
    // Temporarily load the library to extract metadata
    void* handle = const_cast<PluginLoader*>(this)->loadLibrary(filepath);
    if (!handle) {
        info.loadError = "Failed to load library";
        return false;
    }
    
    bool success = validatePlugin(handle) && 
                   const_cast<PluginLoader*>(this)->extractPluginMetadata(handle, info);
    
    const_cast<PluginLoader*>(this)->unloadLibrary(handle);
    
    return success;
}

// =============================================================================
// Directory Management
// =============================================================================

std::string PluginLoader::getPluginsDirectory() const {
    return m_baseDirectory;
}

std::string PluginLoader::getEnabledPluginsDirectory() const {
    return m_baseDirectory + "/enabled";
}

std::string PluginLoader::getDisabledPluginsDirectory() const {
    return m_baseDirectory + "/disabled";
}

bool PluginLoader::createPluginDirectories() {
    try {
        fs::create_directories(getEnabledPluginsDirectory());
        fs::create_directories(getDisabledPluginsDirectory());
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create plugin directories: " << e.what() << std::endl;
        return false;
    }
}

// =============================================================================
// Platform-Specific Dynamic Library Operations
// =============================================================================

void* PluginLoader::loadLibrary(const std::string& filepath) {
#ifdef _WIN32
    return (void*)LoadLibraryA(filepath.c_str());
#else
    return dlopen(filepath.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

void* PluginLoader::getSymbol(void* handle, const char* name) const {
    if (!handle) return nullptr;
    
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)handle, name);
#else
    return dlsym(handle, name);
#endif
}

void PluginLoader::unloadLibrary(void* handle) {
    if (!handle) return;
    
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

std::string PluginLoader::getLibraryExtension() const {
    return PLUGIN_EXT;
}

std::string PluginLoader::getLastLibraryError() const {
#ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0) return "Unknown error";
    
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
#else
    const char* error = dlerror();
    return error ? std::string(error) : "Unknown error";
#endif
}

// =============================================================================
// Internal Helper Methods
// =============================================================================

std::vector<std::string> PluginLoader::scanDirectoryForPlugins(const std::string& directory) const {
    std::vector<std::string> plugins;
    
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return plugins;
    }
    
    std::string ext = getLibraryExtension();
    
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filepath = entry.path().string();
                std::string filename = entry.path().filename().string();
                
                if (isPluginFile(filename)) {
                    plugins.push_back(filepath);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error scanning directory " << directory << ": " << e.what() << std::endl;
    }
    
    // Sort alphabetically for consistent loading order
    std::sort(plugins.begin(), plugins.end());
    
    return plugins;
}

bool PluginLoader::extractPluginMetadata(void* handle, PluginInfo& info) const {
    if (!handle) return false;
    
    // Get plugin metadata functions
    auto getName = (FB_PluginNameFunc)getSymbol(handle, "FB_PLUGIN_NAME");
    auto getVersion = (FB_PluginVersionFunc)getSymbol(handle, "FB_PLUGIN_VERSION");
    auto getDesc = (FB_PluginDescriptionFunc)getSymbol(handle, "FB_PLUGIN_DESCRIPTION");
    auto getAuthor = (FB_PluginAuthorFunc)getSymbol(handle, "FB_PLUGIN_AUTHOR");
    auto getAPIVersion = (FB_PluginAPIVersionFunc)getSymbol(handle, "FB_PLUGIN_API_VERSION");
    auto getRuntimeFiles = (FB_PluginRuntimeFilesFunc)getSymbol(handle, "FB_PLUGIN_RUNTIME_FILES");
    
    if (!getName || !getVersion || !getAPIVersion) {
        return false;
    }
    
    try {
        info.name = getName();
        info.version = getVersion();
        info.description = getDesc ? getDesc() : "";
        info.author = getAuthor ? getAuthor() : "Unknown";
        info.apiVersion = getAPIVersion();
        
        // Extract runtime files (comma-separated list)
        if (getRuntimeFiles) {
            const char* runtimeFilesStr = getRuntimeFiles();
            if (runtimeFilesStr && runtimeFilesStr[0] != '\0') {
                std::string filesStr(runtimeFilesStr);
                size_t pos = 0;
                while (pos < filesStr.length()) {
                    size_t comma = filesStr.find(',', pos);
                    if (comma == std::string::npos) {
                        comma = filesStr.length();
                    }
                    std::string file = filesStr.substr(pos, comma - pos);
                    // Trim whitespace
                    size_t start = file.find_first_not_of(" \t\r\n");
                    size_t end = file.find_last_not_of(" \t\r\n");
                    if (start != std::string::npos) {
                        file = file.substr(start, end - start + 1);
                        if (!file.empty()) {
                            info.runtimeFiles.push_back(file);
                        }
                    }
                    pos = comma + 1;
                }
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool PluginLoader::initializePlugin(void* handle, PluginInfo& info,
                                   ModularCommands::CommandRegistry& registry) {
    auto initFunc = (FB_PluginInitFunc)getSymbol(handle, "FB_PLUGIN_INIT");
    if (!initFunc) {
        info.loadError = "Missing FB_PLUGIN_INIT function";
        return false;
    }
    
    // Set up callbacks structure
    FB_PluginCallbacks callbacks;
    callbacks.beginCommand = Plugin_BeginCommand;
    callbacks.beginFunction = Plugin_BeginFunction;
    callbacks.addParameter = Plugin_AddParameter;
    callbacks.endCommand = Plugin_EndCommand;
    callbacks.setCustomCodeGen = Plugin_SetCustomCodeGen;
    callbacks.userData = &registry;
    
    try {
        int result = initFunc(&callbacks);
        if (result != 0) {
            info.loadError = "Plugin initialization failed with code " + std::to_string(result);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        info.loadError = std::string("Exception during initialization: ") + e.what();
        return false;
    } catch (...) {
        info.loadError = "Unknown exception during initialization";
        return false;
    }
}

bool PluginLoader::isPluginFile(const std::string& filename) const {
    std::string ext = getLibraryExtension();
    
    // Check if filename ends with the correct extension
    if (filename.length() < ext.length()) {
        return false;
    }
    
    return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
}

void PluginLoader::addLoadedPlugin(const PluginInfo& info) {
    m_plugins.push_back(info);
}

void PluginLoader::addFailedPlugin(const PluginInfo& info) {
    m_failedPlugins.push_back(info);
}

bool PluginLoader::cacheRuntimeFile(const std::string& filename) {
    // Check if already cached
    if (m_cachedRuntimeContents.find(filename) != m_cachedRuntimeContents.end()) {
        return true; // Already cached
    }
    
    // Build full path to runtime file
    std::string filepath;
    if (!m_runtimeDirectory.empty()) {
        filepath = m_runtimeDirectory + "/" + filename;
    } else {
        filepath = "runtime/" + filename;
    }
    
    // Open and read the file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[PluginLoader::cacheRuntimeFile] Failed to open runtime file: " << filepath << std::endl;
        return false;
    }
    
    // Read entire file into string
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Cache the content
    m_cachedRuntimeContents[filename] = content;
    
    return true;
}

void PluginLoader::setRuntimeDirectory(const std::string& path) {
    m_runtimeDirectory = path;
}

} // namespace PluginSystem
} // namespace FasterBASIC