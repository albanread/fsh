//
// modular_commands.h
// FasterBASICT - Core Modular BASIC Commands System
//
// Provides a flexible system for adding custom BASIC commands and functions.
// This core system allows applications (fbc, fbsh, FBRunner3) to register
// their own command sets while sharing common infrastructure.
//

#ifndef FASTERBASIC_MODULAR_COMMANDS_H
#define FASTERBASIC_MODULAR_COMMANDS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>

namespace FasterBASIC {
namespace ModularCommands {

// =============================================================================
// Parameter Types and Validation
// =============================================================================

enum class ParameterType {
    INT,        // Integer value
    FLOAT,      // Floating point value
    STRING,     // String value
    COLOR,      // Color value (0xRRGGBBAA format)
    BOOL,       // Boolean value
    TYPENAME,   // User-defined TYPE name (generates schema at compile time)
    OPTIONAL    // Parameter is optional
};

// Return types for modular functions
enum class ReturnType {
    VOID,       // No return value (commands/statements)
    INT,        // Integer return value
    FLOAT,      // Floating point return value
    STRING,     // String return value
    BOOL        // Boolean return value
};

struct ParameterDefinition {
    std::string name;                    // Parameter name (e.g., "x", "y", "text")
    ParameterType type;                  // Parameter type
    std::string defaultValue;            // Default value if optional
    std::string description;             // Human-readable description
    bool isOptional;                     // Whether parameter is optional
    
    ParameterDefinition(const std::string& name, 
                       ParameterType type,
                       const std::string& description,
                       bool optional = false,
                       const std::string& defaultVal = "")
        : name(name), type(type), defaultValue(defaultVal),
          description(description), isOptional(optional) {}
};

// =============================================================================
// Function/Command Definition
// =============================================================================

struct CommandDefinition {
    std::string commandName;             // BASIC command name (e.g., "PRINT_AT")
    std::string description;             // Human-readable description
    std::vector<ParameterDefinition> parameters; // Command parameters
    std::string luaFunction;             // Target Lua function to call
    std::string category;                // Command category ("text", "graphics", etc.)
    bool requiresParentheses;            // Whether command requires () syntax
    std::string customCodeTemplate;      // Custom code generation template (optional)
    bool hasCustomCodeGen;               // Whether to use custom code generation
    ReturnType returnType;               // Return type (VOID for commands, other types for functions)
    bool isFunction;                     // Whether this is a function (returns value) or command (statement)
    std::string usage;                   // Optional usage string (auto-generated if empty)
    
    // Default constructor for std::unordered_map
    CommandDefinition() : commandName(""), description(""), luaFunction(""), 
                         category("general"), requiresParentheses(false),
                         customCodeTemplate(""), hasCustomCodeGen(false),
                         returnType(ReturnType::VOID), isFunction(false), usage("") {}
    
    CommandDefinition(const std::string& name,
                     const std::string& desc,
                     const std::string& luaFunc,
                     const std::string& cat = "general",
                     bool needParens = false,
                     ReturnType retType = ReturnType::VOID)
        : commandName(name), description(desc), luaFunction(luaFunc),
          category(cat), requiresParentheses(needParens),
          customCodeTemplate(""), hasCustomCodeGen(false),
          returnType(retType), isFunction(retType != ReturnType::VOID), usage("") {}
    
    // Add a parameter to this command
    CommandDefinition& addParameter(const std::string& name,
                                   ParameterType type,
                                   const std::string& description,
                                   bool optional = false,
                                   const std::string& defaultValue = "") {
        parameters.emplace_back(name, type, description, optional, defaultValue);
        return *this;
    }
    
    // Get required parameter count
    size_t getRequiredParameterCount() const {
        size_t count = 0;
        for (const auto& param : parameters) {
            if (!param.isOptional) count++;
        }
        return count;
    }
    
    // Get total parameter count
    size_t getTotalParameterCount() const {
        return parameters.size();
    }
    
    // Set custom code generation template
    CommandDefinition& setCustomCodeGen(const std::string& codeTemplate) {
        customCodeTemplate = codeTemplate;
        hasCustomCodeGen = true;
        return *this;
    }
    
    // Set return type (makes this a function instead of command)
    CommandDefinition& setReturnType(ReturnType retType) {
        returnType = retType;
        isFunction = (retType != ReturnType::VOID);
        return *this;
    }
    
    // Set custom usage string (overrides auto-generation)
    CommandDefinition& setUsage(const std::string& usageStr) {
        usage = usageStr;
        return *this;
    }
    
    // Get usage string (auto-generates if not set)
    // Format: "COMMAND param1, param2 [, optional]" for commands
    //         "FUNCTION(param1, param2 [, optional])" for functions
    std::string getUsage() const;
};

// =============================================================================
// Command Registry
// =============================================================================

class CommandRegistry {
public:
    CommandRegistry();
    ~CommandRegistry() = default;
    
    // Delete copy/move constructors and assignment operators (shared_mutex is not copyable)
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    CommandRegistry(CommandRegistry&&) = delete;
    CommandRegistry& operator=(CommandRegistry&&) = delete;
    
    // Register a new command or function
    void registerCommand(const CommandDefinition& cmd);
    void registerCommand(CommandDefinition&& cmd);
    void registerFunction(const CommandDefinition& func);
    void registerFunction(CommandDefinition&& func);
    
    // Query commands and functions
    bool hasCommand(const std::string& name) const;
    bool hasFunction(const std::string& name) const;
    bool hasCommandOrFunction(const std::string& name) const;
    const CommandDefinition* getCommand(const std::string& name) const;
    const CommandDefinition* getFunction(const std::string& name) const;
    const CommandDefinition* getCommandOrFunction(const std::string& name) const;
    
    // Get lists of commands and functions
    std::vector<std::string> getCommandNames() const;
    std::vector<std::string> getFunctionNames() const;
    std::vector<std::string> getAllNames() const;
    std::vector<std::string> getCommandsByCategory(const std::string& category) const;
    std::vector<std::string> getFunctionsByCategory(const std::string& category) const;
    std::vector<std::string> getCategories() const;
    
    // Get all commands (for documentation/help)
    const std::unordered_map<std::string, CommandDefinition>& getAllCommands() const {
        return m_commands;
    }
    
    // Clear all registered commands
    void clear();
    
    // Get command count
    size_t getCommandCount() const { return m_commands.size(); }
    
    // Initialize with built-in commands and functions
    void initializeBuiltinCommands();
    void initializeBuiltinFunctions();
    
private:
    std::unordered_map<std::string, CommandDefinition> m_commands;
    std::unordered_map<std::string, CommandDefinition> m_functions;
    mutable std::shared_mutex m_mutex;  // Protect concurrent access
    
    // Helper methods for registering built-in command sets
    void registerTextCommands();
    void registerGraphicsCommands();
    void registerAudioCommands();
    void registerInputCommands();
    void registerUtilityCommands();
    void registerMathCommands();
    void registerSpriteCommands();
    void registerParticleCommands();
    void registerChunkyGraphicsCommands();
    void registerSixelCommands();
    void registerTilemapCommands();
    
    // Helper methods for registering built-in function sets
    void registerMathFunctions();
    void registerStringFunctions();
    void registerTilemapFunctions();
};

// =============================================================================
// Helper Functions
// =============================================================================

// Convert ParameterType to string for debugging/documentation
std::string parameterTypeToString(ParameterType type);

// Convert ReturnType to string for debugging/documentation
std::string returnTypeToString(ReturnType type);

// Parse parameter type from string
ParameterType stringToParameterType(const std::string& typeStr);

// Validate parameter value against type
bool validateParameter(const std::string& value, ParameterType type);

// Generate default value expression for a parameter type
std::string getDefaultValueForType(ParameterType type);

// Convert color string to hex value (supports "RED", "BLUE", "#RRGGBB", etc.)
uint32_t parseColorValue(const std::string& colorStr);

// =============================================================================
// Global Registry Access
// =============================================================================

// Get the global command registry instance
CommandRegistry& getGlobalCommandRegistry();

// Initialize the global registry with built-in commands
void initializeGlobalRegistry();

// Mark the global registry as initialized (prevents clearing)
void markGlobalRegistryInitialized();

// Check if the global registry has been initialized
bool isGlobalRegistryInitialized();

} // namespace ModularCommands
} // namespace FasterBASIC

#endif // FASTERBASIC_MODULAR_COMMANDS_H