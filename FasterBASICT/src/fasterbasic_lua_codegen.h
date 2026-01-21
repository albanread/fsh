//
// fasterbasic_lua_codegen.h
// FasterBASIC - IR to Lua Code Generator
//
// Translates FasterBASIC IR bytecode to Lua source code for execution
// via LuaJIT. This leverages LuaJIT's world-class trace compiler to
// achieve high performance without manual optimization.
//
// Strategy:
// - Convert IR instructions to equivalent Lua code
// - Use Lua tables for arrays (1-indexed to match BASIC semantics)
// - Use local variables for performance
// - Generate loop-friendly code that LuaJIT can trace and optimize
//

#ifndef FASTERBASIC_LUA_CODEGEN_H
#define FASTERBASIC_LUA_CODEGEN_H

#include "fasterbasic_ircode.h"
#include "fasterbasic_lua_expr.h"

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>

namespace FasterBASIC {

// =============================================================================
// Lua Code Generation Configuration
// =============================================================================

struct LuaCodeGenConfig {
    bool emitComments = true;        // Include IR instruction comments
    bool emitLineNumbers = false;     // Include line number tracking
    bool optimizeLocals = true;       // Use local variables where possible
    bool inlineConstants = true;      // Inline constant values
    bool generateDebugInfo = false;   // Generate debug metadata
    bool useLuaJITHints = true;       // Add LuaJIT-specific optimizations
    bool useVariableCache = true;     // Use hot/cold variable caching (unlimited vars)
    bool enableBufferMode = false;    // Use string buffers for efficient MID$ assignment
    bool exitOnError = true;          // Call os.exit(1) on runtime error (disable for interactive shells)
    int maxLocalVariables = 150;      // Max locals to use (under 200 limit, leaving room for temps)

    LuaCodeGenConfig() = default;
};

// =============================================================================
// Lua Code Generator Statistics
// =============================================================================

struct LuaCodeGenStats {
    size_t irInstructions = 0;
    size_t linesGenerated = 0;
    size_t variablesUsed = 0;
    size_t arraysUsed = 0;
    size_t labelsGenerated = 0;
    double generationTimeMs = 0.0;

    void print() const;
};

// =============================================================================
// Lua Code Generator
// =============================================================================

class LuaCodeGenerator {
public:
    LuaCodeGenerator();
    explicit LuaCodeGenerator(const LuaCodeGenConfig& config);
    ~LuaCodeGenerator();

    // Main API: Generate Lua source from IR
    std::string generate(const IRCode& irCode);

    // Get generation statistics
    const LuaCodeGenStats& getStats() const { return m_stats; }

    // Configuration
    void setConfig(const LuaCodeGenConfig& config) { m_config = config; }
    const LuaCodeGenConfig& getConfig() const { return m_config; }

private:
    // Code generation state
    std::ostringstream m_output;
    LuaCodeGenConfig m_config;
    LuaCodeGenStats m_stats;
    const IRCode* m_code;  // Pointer to IR code for accessing metadata (types, etc.)
    int m_arrayBase;  // OPTION BASE: 0 or 1 (from IRCode metadata)
    bool m_unicodeMode;  // OPTION UNICODE: strings as codepoint arrays (from IRCode metadata)
    bool m_bufferMode;   // Buffer mode: use string buffers for efficient MID$ assignment
    bool m_errorTracking;  // OPTION ERROR: emit _LINE tracking for error messages (from IRCode metadata)
    bool m_forceYieldEnabled;  // OPTION FORCE_YIELD: quasi-preemptive handler yielding (from IRCode metadata)
    int m_forceYieldBudget;  // OPTION FORCE_YIELD budget: instructions before forced yield (from IRCode metadata)
    int m_lastEmittedLine;  // Track last emitted line number to avoid duplicate _LINE assignments
    int m_indentOffset;  // Additional indentation spaces for nested contexts (e.g., subroutines)
    bool m_usesConstants;  // True if program uses CONSTANT statement or predefined constants
    const class ConstantsManager* m_constantsManager;  // Pointer to constants for inlining values
    bool m_cancellableLoops;  // OPTION CANCELLABLE: inject script cancellation checks in loops
    bool m_eventsUsed;  // EVENT DETECTION: if true, program uses ON EVENT statements and needs event processing code
    bool m_usesSIMD;    // SIMD DETECTION: if true, program uses SIMD array operations and needs SIMD module

    // Symbol tables
    std::unordered_map<std::string, int> m_variables;   // varName -> index
    std::unordered_map<std::string, int> m_arrays;      // arrayName -> index
    std::unordered_map<std::string, int> m_labels;      // labelName -> index
    std::unordered_map<int, std::string> m_stringTable; // stringId -> literal
    
    // Variable access tracking for hot/cold caching
    struct VariableAccessInfo {
        std::string name;
        int accessCount = 0;
        bool isHot = false;       // True if cached as local
        bool isLoopCounter = false; // Loop counters are always hot
    };
    std::unordered_map<std::string, VariableAccessInfo> m_variableAccess;
    std::vector<std::string> m_hotVariables;   // Variables cached as locals
    std::unordered_map<std::string, int> m_coldVariableIDs;  // Cold var -> integer ID mapping
    int m_usedLocalSlots = 0;  // Track how many local slots we've used
    
    // Array metadata for SAMM FFI integration
    struct ArrayInfo {
        std::string name;
        std::string typeSuffix;  // "%", "#", "!", "$", "&", or ""
        bool usesFFI;            // True if allocated via SAMM FFI
        std::string luaVarName;  // The Lua variable name for this array
    };
    std::unordered_map<std::string, ArrayInfo> m_arrayInfo;  // arrayName -> metadata
    
    // Function/Sub definition tracking
    struct FunctionInfo {
        std::string name;
        std::vector<std::string> parameters;
        std::vector<bool> parameterIsByRef;        // Track BYREF parameters
        std::vector<std::string> localVariables;   // LOCAL declarations
        std::vector<std::string> sharedVariables;  // SHARED declarations
        bool isFunction;  // true = FUNCTION, false = SUB
        size_t startIndex;  // IR instruction index where definition starts
    };
    std::unordered_map<std::string, FunctionInfo> m_functionDefs;  // funcName -> metadata
    FunctionInfo* m_currentFunction = nullptr;  // Currently being defined

    // Code generation helpers
    void emitHeader();
    void emitFooter();
    void emitVariableDeclarations();
    void emitArrayDeclarations();
    void emitDataSection(const IRCode& irCode);
    void emitTypeDefinitions(const IRCode& irCode);
    void emitUserFunctions(const IRCode& irCode);
    void emitMainFunction(const IRCode& irCode);
    
    // Event processing helpers
    bool shouldInjectEventProcessing() const;
    void emitEventProcessing();

    // Instruction translation
    void emitInstruction(const IRInstruction& instr, size_t index);
    void emitStackOp(const IRInstruction& instr);
    void emitArithmetic(const IRInstruction& instr);
    void emitStringConcat(const IRInstruction& instr);
    void emitComparison(const IRInstruction& instr);
    void emitLogical(const IRInstruction& instr);
    void emitVariable(const IRInstruction& instr);
    void emitConstant(const IRInstruction& instr);
    void emitArray(const IRInstruction& instr);
    void emitControlFlow(const IRInstruction& instr, size_t index);
    void emitLoop(const IRInstruction& instr);
    void emitIO(const IRInstruction& instr);
    void emitBuiltinFunction(const IRInstruction& instr);
    void emitFunctionDefinition(const IRInstruction& instr);
    void emitFunctionCall(const IRInstruction& instr);
    void emitReturn(const IRInstruction& instr);
    void emitExit(const IRInstruction& instr);
    void emitTimer(const IRInstruction& instr);
    
    // User-defined type (record/structure) operations
    void emitTypeDefinition(const IRInstruction& instr);
    void emitLoadMember(const IRInstruction& instr);
    void emitStoreMember(const IRInstruction& instr);
    void emitLoadArrayMember(const IRInstruction& instr);
    void emitStoreArrayMember(const IRInstruction& instr);
    void emitSwap(const IRInstruction& instr);
    void emitRedim(const IRInstruction& instr);
    void emitErase(const IRInstruction& instr);
    void emitArrayBounds(const IRInstruction& instr);
    void emitSIMD(const IRInstruction& instr);

    // Function/Sub collection
    void collectFunctionDefinitions(const IRCode& irCode);

    // Helper functions
    void emit(const std::string& code);
    void emitLine(const std::string& code);
    void emitComment(const std::string& comment);
    void emitLabel(const std::string& label);
    
    // Cancellation check helpers
    bool shouldInjectCancellationCheck() const;
    void emitCancellationCheck();
    void emitLoopJumpCancellationCheck();

    std::string getVarName(const std::string& name);
    std::string getArrayName(const std::string& name);
    std::string getLabelName(const std::string& label);
    std::string escapeString(const std::string& str);
    
    // TYPE schema generation for TYPENAME parameters
    std::string generateTypeSchemaTable(const std::string& typeName);
    std::string mapToSQLType(VariableType type);
    
    // Variable access tracking and hot/cold management
    void analyzeVariableAccess(const IRCode& irCode);
    void selectHotVariables();
    bool isHotVariable(const std::string& varName);
    std::string getVariableReference(const std::string& varName);
    void emitVariableTableDeclaration();
    void emitParameterPoolDeclaration();

    // Stack simulation (for translating stack-based IR to Lua expressions)
    struct StackEntry {
        std::string expr;
        bool isTemp;
    };
    std::vector<StackEntry> m_exprStack;
    int m_tempVarCounter = 0;
    int m_gosubReturnCounter = 0;

    std::string popExpr();
    void pushExpr(const std::string& expr, bool isTemp = false);
    std::string allocTemp();
    void freeTemp(const std::string& temp);

    // Expression optimizer
    ExpressionOptimizer m_exprOptimizer;
    bool m_useExpressionMode = true;  // When true, build expressions; when false, use stack mode
    
    // Helper to check if we can use expression mode
    bool canUseExpressionMode() const;
    
    // Enhanced side-effect analysis for expression preservation
    bool canPreserveExpressions(const IRInstruction& nextInstr) const;
    
    // Smart flush that only flushes when necessary
    void smartFlushExpressions(const IRInstruction& nextInstr);
    
    // Flush expression optimizer to stack-based code if needed
    void flushExpressionToStack();

    // Label resolution
    std::unordered_map<std::string, int> m_labelAddresses;
    void resolveLabels(const IRCode& irCode);
    
    // GOSUB/RETURN tracking
    std::map<size_t, int> m_gosubReturnIds;

    // FOR loop tracking
    struct ForLoopInfo {
        std::string varName;
        std::string endValue;
        std::string stepValue;
        int startAddress;
        std::string loopBackLabel;  // Label to jump back to for FOR_NEXT
        
        // Native loop support
        bool canUseNativeLoop;      // Can we emit a native Lua for loop?
        std::string startExpr;      // Start value expression
        std::string endExpr;        // End value expression  
        std::string stepExpr;       // Step value expression
        bool nativeLoopEmitted;     // Have we emitted the native loop start?
        int loopBodyStartIndex;     // IR index where loop body starts
    };
    std::vector<ForLoopInfo> m_forLoopStack;
    
    // FOR...IN loop tracking
    struct ForInLoopInfo {
        std::string varName;           // Loop variable name
        std::string indexVarName;     // Index variable name (empty if not used)
        std::string arrayName;        // Array variable name
        std::string loopBackLabel;    // Label to jump back to for FOR_IN_NEXT
        bool canUseNativeLoop;        // Can we emit a native Lua for loop?
        bool nativeLoopEmitted;       // Have we emitted the native loop start?
    };
    std::vector<ForInLoopInfo> m_forInLoopStack;
    
    // DO loop tracking
    enum class DoLoopType {
        PRE_TEST_WHILE,   // DO WHILE ... LOOP (emits: while ... do / end)
        PRE_TEST_UNTIL,   // DO UNTIL ... LOOP (emits: while not ... do / end)
        POST_TEST,        // DO ... LOOP WHILE/UNTIL (emits: repeat / until)
        INFINITE          // DO ... LOOP (emits: repeat / until false)
    };
    
    struct DoLoopInfo {
        DoLoopType type;
    };
    std::vector<DoLoopInfo> m_doLoopStack;
    
    // WHILE loop tracking
    enum class WhileLoopType {
        WITH_CONDITION,   // Condition expression available - emit while true + check + break
        FROM_STACK        // Condition pushed to stack - use repeat...until pattern
    };
    
    struct WhileLoopInfo {
        WhileLoopType type;
    };
    std::vector<WhileLoopInfo> m_whileLoopStack;
    
    // Track last emitted opcode to skip unreachable code
    IROpcode m_lastEmittedOpcode = IROpcode::NOP;
};

// =============================================================================
// Utility Functions
// =============================================================================

// Quick helper to generate Lua code from IR
inline std::string generateLuaCode(const IRCode& irCode) {
    LuaCodeGenerator gen;
    return gen.generate(irCode);
}

// Generate with custom configuration
inline std::string generateLuaCode(const IRCode& irCode, const LuaCodeGenConfig& config) {
    LuaCodeGenerator gen(config);
    return gen.generate(irCode);
}

} // namespace FasterBASIC

#endif // FASTERBASIC_LUA_CODEGEN_H
