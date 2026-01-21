//
// plugin_interface.h
// FasterBASICT - Plugin Interface for Extensible Commands
//
// This header defines the plugin API for FasterBASIC, allowing third-party
// developers to create dynamic libraries that extend the compiler with
// custom commands and functions.
//
// API Version: 1.0
//

#ifndef FASTERBASIC_PLUGIN_INTERFACE_H
#define FASTERBASIC_PLUGIN_INTERFACE_H

#include <cstdint>

// =============================================================================
// Parameter and Return Type Enumerations
// =============================================================================

// Parameter types (must match ParameterType enum in modular_commands.h)
enum FB_ParameterType {
    FB_PARAM_INT = 0,
    FB_PARAM_FLOAT = 1,
    FB_PARAM_STRING = 2,
    FB_PARAM_COLOR = 3,
    FB_PARAM_BOOL = 4,
    FB_PARAM_TYPENAME = 5  // User-defined TYPE name (generates schema at compile time)
};

// Return types (must match ReturnType enum in modular_commands.h)
enum FB_ReturnType {
    FB_RETURN_VOID = 0,
    FB_RETURN_INT = 1,
    FB_RETURN_FLOAT = 2,
    FB_RETURN_STRING = 3,
    FB_RETURN_BOOL = 4
};

// =============================================================================
// Plugin Callback Functions
// =============================================================================
// These are provided by the host application to the plugin during initialization.
// The plugin calls these functions to register its commands and functions.

extern "C" {
    // Begin registering a new command (statement with no return value)
    // Returns: handle ID for this command (>= 0 on success, -1 on error)
    typedef int (*FB_BeginCommandFunc)(
        void* userData,
        const char* name,
        const char* description,
        const char* luaFunction,
        const char* category
    );

    // Begin registering a new function (returns a value)
    // Returns: handle ID for this function (>= 0 on success, -1 on error)
    typedef int (*FB_BeginFunctionFunc)(
        void* userData,
        const char* name,
        const char* description,
        const char* luaFunction,
        const char* category,
        int returnType  // FB_ReturnType
    );

    // Add a parameter to the current command/function being defined
    // commandId: handle returned from BeginCommand or BeginFunction
    // Returns: 0 on success, -1 on error
    typedef int (*FB_AddParameterFunc)(
        void* userData,
        int commandId,
        const char* name,
        int type,  // FB_ParameterType
        const char* description,
        int isOptional,
        const char* defaultValue
    );

    // Finish registering the command/function
    // commandId: handle returned from BeginCommand or BeginFunction
    // Returns: 0 on success, -1 on error
    typedef int (*FB_EndCommandFunc)(
        void* userData,
        int commandId
    );

    // Set custom code generation template for a command
    // commandId: handle returned from BeginCommand or BeginFunction
    // Returns: 0 on success, -1 on error
    typedef int (*FB_SetCustomCodeGenFunc)(
        void* userData,
        int commandId,
        const char* codeTemplate
    );
}

// =============================================================================
// Plugin Callbacks Structure
// =============================================================================
// This structure is passed to the plugin's init function and contains
// all the callback functions the plugin can use to register commands.

struct FB_PluginCallbacks {
    // Callback functions
    FB_BeginCommandFunc beginCommand;
    FB_BeginFunctionFunc beginFunction;
    FB_AddParameterFunc addParameter;
    FB_EndCommandFunc endCommand;
    FB_SetCustomCodeGenFunc setCustomCodeGen;
    
    // User data (opaque pointer to CommandRegistry)
    void* userData;
};

// =============================================================================
// C API for Plugin Exports
// =============================================================================
// All plugin exports must use C linkage to ensure ABI compatibility across
// compilers and platforms.

extern "C" {
    // Plugin metadata functions
    typedef const char* (*FB_PluginNameFunc)();
    typedef const char* (*FB_PluginVersionFunc)();
    typedef const char* (*FB_PluginDescriptionFunc)();
    typedef const char* (*FB_PluginAuthorFunc)();
    typedef int (*FB_PluginAPIVersionFunc)();
    
    // Plugin runtime files
    // Returns: Comma-separated list of Lua runtime files (relative to runtime/)
    // Example: "my_plugin_runtime.lua" or "plugin_a.lua,plugin_b.lua"
    // Returns NULL or empty string if no runtime files needed
    typedef const char* (*FB_PluginRuntimeFilesFunc)();
    
    // Plugin lifecycle functions
    // FB_PLUGIN_INIT: Called when plugin is loaded
    //   - callbacks: Structure containing registration callbacks
    //   - Returns: 0 on success, non-zero on failure
    typedef int (*FB_PluginInitFunc)(FB_PluginCallbacks* callbacks);
    
    // FB_PLUGIN_SHUTDOWN: Called when plugin is unloaded
    typedef void (*FB_PluginShutdownFunc)();
    
    // API version constants
    #define FB_PLUGIN_API_VERSION_1 1
    #define FB_PLUGIN_API_VERSION_CURRENT FB_PLUGIN_API_VERSION_1
}

// =============================================================================
// C++ Helper Macros and Utilities
// =============================================================================

#ifdef __cplusplus

// Plugin export macro
#define FB_PLUGIN_EXPORT extern "C"

// Plugin metadata macro - Use this at the start of your plugin
// Example:
//   FB_PLUGIN_BEGIN("My Plugin", "1.0.0", "Description", "Author", "my_runtime.lua")
#define FB_PLUGIN_BEGIN(name, version, description, author, runtimeFiles) \
    FB_PLUGIN_EXPORT const char* FB_PLUGIN_NAME() { return name; } \
    FB_PLUGIN_EXPORT const char* FB_PLUGIN_VERSION() { return version; } \
    FB_PLUGIN_EXPORT const char* FB_PLUGIN_DESCRIPTION() { return description; } \
    FB_PLUGIN_EXPORT const char* FB_PLUGIN_AUTHOR() { return author; } \
    FB_PLUGIN_EXPORT int FB_PLUGIN_API_VERSION() { return FB_PLUGIN_API_VERSION_CURRENT; } \
    FB_PLUGIN_EXPORT const char* FB_PLUGIN_RUNTIME_FILES() { return runtimeFiles; }

// Plugin initialization macro - Use this to define your init function
// Example:
//   FB_PLUGIN_INIT(callbacks) {
//       // Register commands using callbacks
//       return 0;
//   }
#define FB_PLUGIN_INIT(callbacks) \
    FB_PLUGIN_EXPORT int FB_PLUGIN_INIT(FB_PluginCallbacks* callbacks)

// Plugin shutdown macro - Use this to define your shutdown function
// Example:
//   FB_PLUGIN_SHUTDOWN() {
//       // Cleanup...
//   }
#define FB_PLUGIN_SHUTDOWN() \
    FB_PLUGIN_EXPORT void FB_PLUGIN_SHUTDOWN()

// =============================================================================
// C++ Helper Class for Easier Command Registration
// =============================================================================

class FB_CommandBuilder {
private:
    FB_PluginCallbacks* m_callbacks;
    int m_commandId;
    bool m_valid;

public:
    FB_CommandBuilder(FB_PluginCallbacks* callbacks, int commandId)
        : m_callbacks(callbacks), m_commandId(commandId), m_valid(commandId >= 0) {}

    // Add a required parameter
    FB_CommandBuilder& addParameter(const char* name, FB_ParameterType type, const char* description) {
        if (m_valid && m_callbacks->addParameter) {
            m_callbacks->addParameter(m_callbacks->userData, m_commandId, name, type, description, 0, "");
        }
        return *this;
    }

    // Add an optional parameter with default value
    FB_CommandBuilder& addOptionalParameter(const char* name, FB_ParameterType type, 
                                           const char* description, const char* defaultValue) {
        if (m_valid && m_callbacks->addParameter) {
            m_callbacks->addParameter(m_callbacks->userData, m_commandId, name, type, description, 1, defaultValue);
        }
        return *this;
    }

    // Set custom code generation template
    FB_CommandBuilder& setCustomCodeGen(const char* codeTemplate) {
        if (m_valid && m_callbacks->setCustomCodeGen) {
            m_callbacks->setCustomCodeGen(m_callbacks->userData, m_commandId, codeTemplate);
        }
        return *this;
    }

    // Finish command registration
    bool finish() {
        if (m_valid && m_callbacks->endCommand) {
            return m_callbacks->endCommand(m_callbacks->userData, m_commandId) == 0;
        }
        return false;
    }

    // Check if builder is valid
    bool isValid() const { return m_valid; }
};

// Helper function to begin a command
inline FB_CommandBuilder FB_BeginCommand(FB_PluginCallbacks* callbacks, const char* name,
                                        const char* description, const char* luaFunc,
                                        const char* category = "custom") {
    int id = callbacks->beginCommand(callbacks->userData, name, description, luaFunc, category);
    return FB_CommandBuilder(callbacks, id);
}

// Helper function to begin a function
inline FB_CommandBuilder FB_BeginFunction(FB_PluginCallbacks* callbacks, const char* name,
                                         const char* description, const char* luaFunc,
                                         FB_ReturnType returnType, const char* category = "custom") {
    int id = callbacks->beginFunction(callbacks->userData, name, description, luaFunc, category, returnType);
    return FB_CommandBuilder(callbacks, id);
}

#endif // __cplusplus

// =============================================================================
// Plugin Development Guidelines
// =============================================================================
//
// 1. REQUIRED EXPORTS
//    Every plugin must export these functions:
//    - FB_PLUGIN_NAME()
//    - FB_PLUGIN_VERSION()
//    - FB_PLUGIN_DESCRIPTION()
//    - FB_PLUGIN_AUTHOR()
//    - FB_PLUGIN_API_VERSION()
//    - FB_PLUGIN_RUNTIME_FILES()  (can return NULL/empty if no runtime needed)
//    - FB_PLUGIN_INIT(FB_PluginCallbacks*)
//    - FB_PLUGIN_SHUTDOWN()
//
// 2. INITIALIZATION
//    - FB_PLUGIN_INIT receives a callbacks structure
//    - Use the callbacks to register commands and functions
//    - Return 0 for success, non-zero for failure
//    - Plugin will not load if init returns non-zero
//
// 3. COMMAND REGISTRATION
//    Use the callback functions to register:
//    a) Begin command/function
//    b) Add parameters
//    c) Set options (custom code gen, etc.)
//    d) End command/function
//
// 4. SHUTDOWN
//    - FB_PLUGIN_SHUTDOWN is called before plugin unloads
//    - Clean up any resources allocated during init or runtime
//    - Optional - can be empty if no cleanup needed
//
// 5. API VERSION
//    - Must return FB_PLUGIN_API_VERSION_CURRENT
//    - Plugin loader checks version compatibility
//    - Incompatible versions are rejected
//
// 6. THREAD SAFETY
//    - Init function is called single-threaded at startup
//    - No need for synchronization in init
//    - Shutdown is also single-threaded
//
// 7. ERROR HANDLING
//    - Return non-zero from init on failure
//    - Check callback return values
//    - Don't crash the compiler process
//
// 8. NAMING CONVENTIONS
//    - Commands: UPPERCASE (e.g., MY_COMMAND)
//    - Functions: UPPERCASE, use $ suffix for strings (e.g., REVERSE$)
//    - Lua functions: lowercase_with_underscores (e.g., my_command)
//    - Categories: lowercase (e.g., "math", "string", "custom")
//
// =============================================================================
// Example Plugin (Simple)
// =============================================================================
/*

#include "plugin_interface.h"

FB_PLUGIN_BEGIN("Example Plugin", "1.0.0", "Example plugin", "FasterBASIC Team",
                "example_runtime.lua")

FB_PLUGIN_INIT(callbacks) {
    // Register a command
    FB_BeginCommand(callbacks, "HELLO", "Print greeting", "my_hello")
        .addParameter("name", FB_PARAM_STRING, "Name to greet")
        .finish();
    
    // Register a function
    FB_BeginFunction(callbacks, "DOUBLE", "Double a number", "my_double", FB_RETURN_INT)
        .addParameter("value", FB_PARAM_INT, "Value to double")
        .finish();
    
    return 0;
}

FB_PLUGIN_SHUTDOWN() {
    // Cleanup if needed
}

*/

// =============================================================================
// Example Plugin (Using Raw Callbacks)
// =============================================================================
/*

#include "plugin_interface.h"

FB_PLUGIN_BEGIN("Example Plugin", "1.0.0", "Example plugin", "FasterBASIC Team",
                "example_runtime.lua")

FB_PLUGIN_INIT(callbacks) {
    // Register a command using raw callbacks
    int cmdId = callbacks->beginCommand(callbacks->userData, "HELLO", 
                                       "Print greeting", "my_hello", "custom");
    if (cmdId >= 0) {
        callbacks->addParameter(callbacks->userData, cmdId, "name", 
                              FB_PARAM_STRING, "Name to greet", 0, "");
        callbacks->endCommand(callbacks->userData, cmdId);
    }
    
    return 0;
}

FB_PLUGIN_SHUTDOWN() {
    // Cleanup if needed
}

*/

#endif // FASTERBASIC_PLUGIN_INTERFACE_H