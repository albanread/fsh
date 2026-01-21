//
// command_registry_core.h
// FasterBASICT - Core BASIC Commands Registry
//
// Defines core BASIC commands and functions that are available to all
// FasterBASICT applications (fbc, fbsh, FBRunner3). These are fundamental
// BASIC language constructs that don't depend on specific runtime environments.
//

#ifndef FASTERBASIC_COMMAND_REGISTRY_CORE_H
#define FASTERBASIC_COMMAND_REGISTRY_CORE_H

#include "modular_commands.h"

namespace FasterBASIC {
namespace ModularCommands {

// =============================================================================
// Core Command Registration
// =============================================================================

class CoreCommandRegistry {
public:
    // Register all core BASIC commands and functions
    static void registerCoreCommands(CommandRegistry& registry);
    static void registerCoreFunctions(CommandRegistry& registry);
    
    // Register individual command categories
    static void registerBasicIOCommands(CommandRegistry& registry);
    static void registerMathCommands(CommandRegistry& registry);
    static void registerStringCommands(CommandRegistry& registry);
    static void registerControlFlowCommands(CommandRegistry& registry);
    static void registerDataCommands(CommandRegistry& registry);
    static void registerTestCommands(CommandRegistry& registry);  // Temporary for parser investigation
    
    // Register core function categories
    static void registerMathFunctions(CommandRegistry& registry);
    static void registerStringFunctions(CommandRegistry& registry);
    static void registerSystemFunctions(CommandRegistry& registry);
    static void registerFileIOFunctions(CommandRegistry& registry);
    static void registerFileIOCommands(CommandRegistry& registry);
    
private:
    // Helper methods
    static void registerBasicPrint(CommandRegistry& registry);
    static void registerBasicInput(CommandRegistry& registry);
    static void registerFileIO(CommandRegistry& registry);
    static void registerArrayCommands(CommandRegistry& registry);
};

// =============================================================================
// Convenience Functions
// =============================================================================

// Initialize a registry with only core commands (no application-specific ones)
void initializeCoreRegistry(CommandRegistry& registry);

// createCoreRegistry removed - use initializeCoreRegistry() directly instead
// (CommandRegistry is no longer copyable/movable due to thread safety mutex)

} // namespace ModularCommands
} // namespace FasterBASICT

#endif // FASTERBASIC_COMMAND_REGISTRY_CORE_H