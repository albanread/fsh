//
// modular_commands.cpp
// FasterBASICT - Core Modular BASIC Commands System Implementation
//
// Implements the flexible system for adding custom BASIC commands and functions.
// This core system allows applications to register their own command sets.
//

#include "modular_commands.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cctype>
#include <set>
#include <mutex>
#include <shared_mutex>

namespace FasterBASIC {
namespace ModularCommands {

// =============================================================================
// Helper Functions Implementation
// =============================================================================

std::string parameterTypeToString(ParameterType type) {
    switch (type) {
        case ParameterType::INT:        return "int";
        case ParameterType::FLOAT:      return "float";
        case ParameterType::STRING:     return "string";
        case ParameterType::COLOR:      return "color";
        case ParameterType::BOOL:       return "boolean";
        case ParameterType::OPTIONAL:   return "optional";
        default:                        return "unknown";
    }
}

std::string returnTypeToString(ReturnType type) {
    switch (type) {
        case ReturnType::VOID:          return "void";
        case ReturnType::INT:           return "int";
        case ReturnType::FLOAT:         return "float";
        case ReturnType::STRING:        return "string";
        case ReturnType::BOOL:          return "boolean";
        default:                        return "unknown";
    }
}

ParameterType stringToParameterType(const std::string& typeStr) {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "int" || lower == "integer") return ParameterType::INT;
    if (lower == "float" || lower == "double") return ParameterType::FLOAT;
    if (lower == "string" || lower == "str") return ParameterType::STRING;
    if (lower == "color" || lower == "colour") return ParameterType::COLOR;
    if (lower == "bool" || lower == "boolean") return ParameterType::BOOL;
    if (lower == "optional") return ParameterType::OPTIONAL;
    
    return ParameterType::STRING; // Default fallback
}

bool validateParameter(const std::string& value, ParameterType type) {
    switch (type) {
        case ParameterType::INT: {
            // Check if string represents a valid integer
            try {
                std::stoi(value);
                return true;
            } catch (...) {
                return false;
            }
        }
        
        case ParameterType::FLOAT: {
            // Check if string represents a valid float
            try {
                std::stof(value);
                return true;
            } catch (...) {
                return false;
            }
        }
        
        case ParameterType::STRING:
            // Strings are always valid
            return true;
        
        case ParameterType::COLOR: {
            // Check if string represents a valid color (hex, named color, etc.)
            if (value.empty()) return false;
            
            // Check for hex format (#RRGGBB, #RRGGBBAA, 0xRRGGBB, etc.)
            if (value[0] == '#' || (value.size() >= 2 && value.substr(0, 2) == "0x")) {
                std::string hex = value;
                if (hex[0] == '#') hex = hex.substr(1);
                if (hex.size() >= 2 && hex.substr(0, 2) == "0x") hex = hex.substr(2);
                
                if (hex.size() == 6 || hex.size() == 8) { // RGB or RGBA
                    return std::all_of(hex.begin(), hex.end(), [](char c) {
                        return std::isxdigit(c);
                    });
                }
            }
            
            // Check for named colors (basic validation)
            std::string upper = value;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            if (upper == "RED" || upper == "GREEN" || upper == "BLUE" || 
                upper == "WHITE" || upper == "BLACK" || upper == "YELLOW" ||
                upper == "CYAN" || upper == "MAGENTA") {
                return true;
            }
            
            return false;
        }
        
        case ParameterType::BOOL: {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return (lower == "true" || lower == "false" || 
                    lower == "1" || lower == "0" ||
                    lower == "yes" || lower == "no");
        }
        
        case ParameterType::OPTIONAL:
            // Optional parameters are always valid
            return true;
        
        default:
            return false;
    }
}

std::string getDefaultValueForType(ParameterType type) {
    switch (type) {
        case ParameterType::INT:        return "0";
        case ParameterType::FLOAT:      return "0.0";
        case ParameterType::STRING:     return "\"\"";
        case ParameterType::COLOR:      return "0xFFFFFFFF";
        case ParameterType::BOOL:       return "false";
        case ParameterType::OPTIONAL:   return "nil";
        default:                        return "nil";
    }
}

uint32_t parseColorValue(const std::string& colorStr) {
    if (colorStr.empty()) return 0xFFFFFFFF; // Default to white
    
    std::string color = colorStr;
    std::transform(color.begin(), color.end(), color.begin(), ::toupper);
    
    // Named colors
    if (color == "BLACK")       return 0xFF000000;
    if (color == "WHITE")       return 0xFFFFFFFF;
    if (color == "RED")         return 0xFFFF0000;
    if (color == "GREEN")       return 0xFF00FF00;
    if (color == "BLUE")        return 0xFF0000FF;
    if (color == "YELLOW")      return 0xFFFFFF00;
    if (color == "CYAN")        return 0xFF00FFFF;
    if (color == "MAGENTA")     return 0xFFFF00FF;
    
    // Hex colors (#RRGGBB, #RRGGBBAA, 0xRRGGBB, etc.)
    std::string hex = color;
    if (hex[0] == '#') hex = hex.substr(1);
    if (hex.size() >= 2 && hex.substr(0, 2) == "0X") hex = hex.substr(2);
    
    if (hex.size() == 6) {
        // RGB -> RGBA (add full alpha)
        hex = "FF" + hex;
    }
    
    if (hex.size() == 8) {
        try {
            return static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
        } catch (...) {
            return 0xFFFFFFFF; // Fallback to white
        }
    }
    
    return 0xFFFFFFFF; // Default fallback
}

// =============================================================================
// CommandDefinition Implementation
// =============================================================================

std::string CommandDefinition::getUsage() const {
    // If usage is explicitly set, return it
    if (!usage.empty()) {
        return usage;
    }
    
    // Auto-generate usage from command name and parameters
    std::ostringstream ss;
    ss << commandName;
    
    // For functions, add opening parenthesis
    if (isFunction) {
        ss << "(";
    }
    
    // Add parameters
    bool firstParam = true;
    for (const auto& param : parameters) {
        if (!firstParam) {
            ss << ", ";
        } else {
            // For commands (not functions), add space before first parameter
            if (!isFunction) {
                ss << " ";
            }
            firstParam = false;
        }
        
        // Add opening bracket for optional parameters
        if (param.isOptional) {
            ss << "[";
        }
        
        ss << param.name;
        
        // Add closing bracket for optional parameters
        if (param.isOptional) {
            ss << "]";
        }
    }
    
    // For functions, add closing parenthesis
    if (isFunction) {
        ss << ")";
    }
    
    return ss.str();
}

// =============================================================================
// CommandRegistry Implementation
// =============================================================================

CommandRegistry::CommandRegistry() {
    // Empty constructor - applications will populate with their own commands
}

void CommandRegistry::registerCommand(const CommandDefinition& cmd) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_commands[cmd.commandName] = cmd;
}

void CommandRegistry::registerCommand(CommandDefinition&& cmd) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    std::string name = cmd.commandName;
    m_commands[name] = std::move(cmd);
}

void CommandRegistry::registerFunction(const CommandDefinition& func) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_functions[func.commandName] = func;
    
    // Automatic name mangling: if name contains $, also register _STRING variant
    if (func.commandName.find('$') != std::string::npos) {
        std::string mangledName = func.commandName;
        size_t pos = mangledName.find('$');
        mangledName.replace(pos, 1, "_STRING");
        
        // Create mangled variant with same properties
        CommandDefinition mangledFunc = func;
        mangledFunc.commandName = mangledName;
        m_functions[mangledName] = mangledFunc;
    }
}

void CommandRegistry::registerFunction(CommandDefinition&& func) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    std::string name = func.commandName;
    
    // Check if we need to create a mangled variant before moving
    bool needsMangling = (name.find('$') != std::string::npos);
    std::string mangledName;
    if (needsMangling) {
        mangledName = name;
        size_t pos = mangledName.find('$');
        mangledName.replace(pos, 1, "_STRING");
    }
    
    // Register the original
    m_functions[name] = std::move(func);
    
    // Register mangled variant if needed
    if (needsMangling) {
        CommandDefinition mangledFunc = m_functions[name];
        mangledFunc.commandName = mangledName;
        m_functions[mangledName] = mangledFunc;
    }
}

bool CommandRegistry::hasCommand(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_commands.find(name) != m_commands.end();
}

bool CommandRegistry::hasFunction(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_functions.find(name) != m_functions.end();
}

bool CommandRegistry::hasCommandOrFunction(const std::string& name) const {
    return hasCommand(name) || hasFunction(name);
}

const CommandDefinition* CommandRegistry::getCommand(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_commands.find(name);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

const CommandDefinition* CommandRegistry::getFunction(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_functions.find(name);
    return (it != m_functions.end()) ? &it->second : nullptr;
}

const CommandDefinition* CommandRegistry::getCommandOrFunction(const std::string& name) const {
    const CommandDefinition* cmd = getCommand(name);
    if (cmd) return cmd;
    return getFunction(name);
}

std::vector<std::string> CommandRegistry::getCommandNames() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_commands.size());
    for (const auto& pair : m_commands) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> CommandRegistry::getFunctionNames() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_functions.size());
    for (const auto& pair : m_functions) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> CommandRegistry::getAllNames() const {
    std::vector<std::string> names = getCommandNames();
    std::vector<std::string> functionNames = getFunctionNames();
    names.insert(names.end(), functionNames.begin(), functionNames.end());
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> CommandRegistry::getCommandsByCategory(const std::string& category) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& pair : m_commands) {
        if (pair.second.category == category) {
            names.push_back(pair.first);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> CommandRegistry::getFunctionsByCategory(const std::string& category) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& pair : m_functions) {
        if (pair.second.category == category) {
            names.push_back(pair.first);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> CommandRegistry::getCategories() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::set<std::string> categories;
    for (const auto& pair : m_commands) {
        categories.insert(pair.second.category);
    }
    for (const auto& pair : m_functions) {
        categories.insert(pair.second.category);
    }
    return std::vector<std::string>(categories.begin(), categories.end());
}

void CommandRegistry::clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_commands.clear();
    m_functions.clear();
}

void CommandRegistry::initializeBuiltinCommands() {
    // Clear any existing commands first
    clear();

    // Register built-in command sets that are needed by the compiler
    registerUtilityCommands();
    
    // Note: Applications can register additional commands as needed
}

void CommandRegistry::initializeBuiltinFunctions() {
    // NOTE: This method is deprecated in favor of application-specific
    // initialization. Applications should use CoreCommandRegistry for
    // basic functions and register their own specific function sets.
}

// =============================================================================
// Deprecated Command Registration Methods
// =============================================================================
// These methods exist for backward compatibility but are now deprecated.
// Applications should use their own command registration systems.

void CommandRegistry::registerTextCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific text commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own text commands as needed.
}

void CommandRegistry::registerGraphicsCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific graphics commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own graphics commands as needed.
}

void CommandRegistry::registerAudioCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific audio commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own audio commands as needed.
}

void CommandRegistry::registerInputCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific input commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own input commands as needed.
}

void CommandRegistry::registerUtilityCommands() {
    // Register core utility commands that are needed by the compiler
    // Applications can register additional utility commands as needed.
    
    // Note: CLS is handled by application-specific registries (e.g., SuperTerminal)
    // to allow for richer implementations beyond basic terminal clearing
}

void CommandRegistry::registerMathCommands() {
    // NOTE: Math functions are now handled by CoreCommandRegistry
    // and built into the core system. Applications can extend with
    // their own math-related commands if needed.
}

void CommandRegistry::registerSpriteCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific sprite commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own sprite commands as needed.
}

void CommandRegistry::registerParticleCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific particle commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own particle commands as needed.
}

void CommandRegistry::registerChunkyGraphicsCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific chunky graphics commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own chunky graphics commands as needed.
}

void CommandRegistry::registerSixelCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific sixel commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own sixel commands as needed.
}

void CommandRegistry::registerTilemapCommands() {
    // NOTE: This method is deprecated. SuperTerminal-specific tilemap commands
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own tilemap commands as needed.
}

void CommandRegistry::registerMathFunctions() {
    // NOTE: Math functions are now handled by CoreCommandRegistry
    // and built into the core system. Applications can extend with
    // their own math functions if needed.
}

void CommandRegistry::registerStringFunctions() {
    // NOTE: String functions are now handled by CoreCommandRegistry
    // and built into the core system. Applications can extend with
    // their own string functions if needed.
}

void CommandRegistry::registerTilemapFunctions() {
    // NOTE: This method is deprecated. SuperTerminal-specific tilemap functions
    // have been moved to FBRunner3's command_registry_superterminal.cpp
    // Applications should register their own tilemap functions as needed.
}

// =============================================================================
// Global Registry Access
// =============================================================================

static CommandRegistry* g_globalRegistry = nullptr;
static bool g_registryInitialized = false;

CommandRegistry& getGlobalCommandRegistry() {
    if (!g_globalRegistry) {
        g_globalRegistry = new CommandRegistry();
    }
    return *g_globalRegistry;
}

void initializeGlobalRegistry() {
    // Don't clear an already-initialized registry
    if (g_registryInitialized) {
        return;
    }
    
    CommandRegistry& registry = getGlobalCommandRegistry();
    registry.clear();
    
    // Mark as initialized to prevent re-clearing
    g_registryInitialized = true;
    
    // NOTE: Applications should initialize the global registry with their
    // own command sets. The core system no longer provides default commands.
    // 
    // Example:
    // CoreCommandRegistry::registerCoreCommands(registry);
    // CoreCommandRegistry::registerCoreFunctions(registry);
    // MyAppCommandRegistry::registerAppCommands(registry);
}

void markGlobalRegistryInitialized() {
    g_registryInitialized = true;
}

bool isGlobalRegistryInitialized() {
    return g_registryInitialized;
}

} // namespace ModularCommands
} // namespace FasterBASIC