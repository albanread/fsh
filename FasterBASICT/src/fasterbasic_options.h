//
// fasterbasic_options.h
// FasterBASIC - Compiler Options
//
// Holds compiler directives from OPTION statements.
// These are set during initial parsing and affect all compilation phases.
//

#ifndef FASTERBASIC_OPTIONS_H
#define FASTERBASIC_OPTIONS_H

namespace FasterBASIC {

// =============================================================================
// Compiler Options Structure
// =============================================================================

struct CompilerOptions {
    // Array indexing: OPTION BASE 0 or OPTION BASE 1
    // Default is 1 (matches Lua's 1-based indexing)
    int arrayBase = 1;
    
    // String encoding: OPTION UNICODE
    // When true, strings are represented as Unicode codepoint arrays
    // When false, strings are byte sequences (standard Lua strings)
    bool unicodeMode = false;
    
    // Loop cancellation: OPTION CANCELLABLE ON/OFF
    // When true, inject script cancellation checks into loops
    // Default is true for safety (allows users to turn off for maximum speed)
    bool cancellableLoops = true;
    
    // Error tracking: OPTION ERROR
    // When true, emit _LINE tracking for better error messages
    // Default is true for better UX (shows BASIC line numbers in runtime errors)
    bool errorTracking = true;
    
    // Operator behavior: OPTION BITWISE vs OPTION LOGICAL
    // When true, AND/OR/XOR are bitwise operators
    // When false, AND/OR/XOR are logical operators (default BASIC behavior)
    bool bitwiseOperators = false;
    
    // Variable declaration: OPTION EXPLICIT
    // When true, all variables must be explicitly declared (DIM/LOCAL)
    // When false, variables can be implicitly declared on first use
    bool explicitDeclarations = false;
    
    // Forced yielding: OPTION FORCE_YIELD [budget]
    // When enabled, timer handlers are automatically yielded after N instructions
    // This prevents long-running handlers from blocking the main program
    bool forceYieldEnabled = false;
    int forceYieldBudget = 10000;  // Default: yield every 10,000 instructions
    
    // Constructor with defaults
    CompilerOptions() = default;
    
    // Reset to defaults
    void reset() {
        arrayBase = 1;
        unicodeMode = false;
        cancellableLoops = true;   // Default to enabled for safety
        errorTracking = true;      // Default to enabled for better UX
        bitwiseOperators = false;
        explicitDeclarations = false;
        forceYieldEnabled = false;
        forceYieldBudget = 10000;
    }
};

} // namespace FasterBASIC

#endif // FASTERBASIC_OPTIONS_H