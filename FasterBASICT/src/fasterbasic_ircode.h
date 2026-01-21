//
// fasterbasic_ircode.h
// FasterBASIC - Intermediate Representation (IR) Code Generator
//
// Generates linearized bytecode/IR from the Control Flow Graph.
// The IR is a stack-based instruction sequence suitable for interpretation
// and optimization. This is Phase 5 of the compilation pipeline.
//

#ifndef FASTERBASIC_IRCODE_H
#define FASTERBASIC_IRCODE_H

#include "fasterbasic_ast.h"
#include "fasterbasic_semantic.h"
#include "fasterbasic_cfg.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <variant>
#include <sstream>
#include <iomanip>

namespace FasterBASIC {

// =============================================================================
// IR Opcode Definitions
// =============================================================================

enum class IROpcode {
    // === Stack Operations ===
    PUSH_INT,           // Push integer constant
    PUSH_FLOAT,         // Push float constant
    PUSH_DOUBLE,        // Push double constant
    PUSH_STRING,        // Push string constant
    POP,                // Pop and discard top of stack
    DUP,                // Duplicate top of stack

    // === Arithmetic Operations (binary, pop 2, push 1) ===
    ADD,                // Add: pop b, pop a, push a+b
    SUB,                // Subtract: pop b, pop a, push a-b
    MUL,                // Multiply: pop b, pop a, push a*b
    DIV,                // Divide: pop b, pop a, push a/b
    IDIV,               // Integer divide: pop b, pop a, push a\b
    MOD,                // Modulo: pop b, pop a, push a MOD b
    POW,                // Power: pop b, pop a, push a^b

    // === Unary Operations (pop 1, push 1) ===
    NEG,                // Negate: pop a, push -a
    NOT,                // Logical NOT: pop a, push NOT a

    // === Comparison Operations (pop 2, push boolean as int) ===
    EQ,                 // Equal: pop b, pop a, push a=b
    NE,                 // Not equal: pop b, pop a, push a<>b
    LT,                 // Less than: pop b, pop a, push a<b
    LE,                 // Less or equal: pop b, pop a, push a<=b
    GT,                 // Greater than: pop b, pop a, push a>b
    GE,                 // Greater or equal: pop b, pop a, push a>=b

    // === Logical Operations (pop 2, push 1) ===
    AND,                // Logical AND: pop b, pop a, push a AND b
    OR,                 // Logical OR: pop b, pop a, push a OR b
    XOR,                // Logical XOR: pop b, pop a, push a XOR b
    EQV,                // Logical EQV: pop b, pop a, push a EQV b
    IMP,                // Logical IMP: pop b, pop a, push a IMP b

    // === Variable Operations ===
    LOAD_VAR,           // Push variable value (operand: var name)
    STORE_VAR,          // Pop value, store in variable (operand: var name)
    LOAD_CONST,         // Push constant value by index (operand: constant index)
    MID_ASSIGN,         // MID$ assignment: operand1=var name; pops replacement, len, pos from stack

    // === Array Operations ===
    LOAD_ARRAY,         // Pop indices, push array element (operand: array name)
    STORE_ARRAY,        // Pop value, pop indices, store in array (operand: array name)
    DIM_ARRAY,          // Pop dimensions, allocate array (operand: array name)
    REDIM_ARRAY,        // Pop dimensions, resize array (operand: array name, operand2: preserve flag)
    ERASE_ARRAY,        // Deallocate/clear array (operand: array name)
    LBOUND_ARRAY,       // Push lower bound of array dimension (operand: array name, operand2: dimension)
    UBOUND_ARRAY,       // Push upper bound of array dimension (operand: array name, operand2: dimension)
    FILL_ARRAY,         // Pop value, fill all array elements (operand: array name)
    
    // Element-wise array operations (for regular non-SIMD arrays)
    ARRAY_ADD,          // result() = a() + b() element-wise (operand1: result, operand2: a, operand3: b)
    ARRAY_SUB,          // result() = a() - b() element-wise
    ARRAY_MUL,          // result() = a() * b() element-wise
    ARRAY_DIV,          // result() = a() / b() element-wise
    ARRAY_ADD_SCALAR,   // result() = a() + scalar element-wise (operand1: result, operand2: a, value on stack)
    ARRAY_SUB_SCALAR,   // result() = a() - scalar element-wise
    ARRAY_MUL_SCALAR,   // result() = a() * scalar element-wise
    ARRAY_DIV_SCALAR,   // result() = a() / scalar element-wise
    
    SWAP_VAR,           // Swap two variables (operand1: var1 name, operand2: var2 name)

    // === Control Flow ===
    LABEL,              // Label for jump target (operand: label ID)
    JUMP,               // Unconditional jump (operand: label ID)
    JUMP_IF_TRUE,       // Pop condition, jump if true (operand: label ID)
    JUMP_IF_FALSE,      // Pop condition, jump if false (operand: label ID)
    CALL_GOSUB,         // Call subroutine (operand: label ID)
    RETURN_GOSUB,       // Return from subroutine
    ON_GOTO,            // Pop selector, computed GOTO (operand: comma-separated label IDs)
    ON_GOSUB,           // Pop selector, computed GOSUB (operand: comma-separated label IDs)
    ON_CALL,            // Pop selector, computed CALL (operand: comma-separated function names)
    ON_EVENT,           // Register event handler (operand: event name, operand2: handler type and target)

    // === Structured Control Flow (for native Lua generation) ===
    IF_START,           // Begin IF block; condition on stack (will be popped)
    ELSEIF_START,       // Begin ELSEIF block; condition on stack (will be popped)
    ELSE_START,         // Begin ELSE block
    IF_END,             // End IF/ELSEIF/ELSE block

    // === Function Calls ===
    CALL_BUILTIN,       // Call built-in function (operand: function name, arg count)
    CALL_USER_FN,       // Call user-defined function (operand: function name)
    CALL_FUNCTION,      // Call user-defined FUNCTION (operand: function name, arg count)
    CALL_SUB,           // Call user-defined SUB (operand: sub name, arg count)
    DEFINE_FUNCTION,    // Begin function definition (operand: function name)
    DEFINE_SUB,         // Begin sub definition (operand: sub name)
    END_FUNCTION,       // End function definition
    END_SUB,            // End sub definition
    RETURN_VALUE,       // Return from function with value on stack
    RETURN_VOID,        // Return from sub (no value)
    DECLARE_LOCAL,      // Declare local variable in function (operand: variable name)
    DECLARE_SHARED,     // Declare shared (module-level) variable access (operand: variable name)
    PARAM_BYREF,        // Mark parameter as BYREF (operand: parameter name)
    EXIT_FOR,           // Exit from FOR loop
    EXIT_DO,            // Exit from DO loop
    EXIT_WHILE,         // Exit from WHILE loop
    EXIT_REPEAT,        // Exit from REPEAT loop
    EXIT_FUNCTION,      // Exit from FUNCTION (early return)
    EXIT_SUB,           // Exit from SUB (early return)

    // === I/O Operations ===
    PRINT,              // Pop and print value to screen at cursor (operand: format flags)
    CONSOLE,            // Pop and print value to console (operand: format flags)
    PRINT_NEWLINE,      // Print newline
    PRINT_TAB,          // Pop position, move to tab position
    PRINT_USING,        // Pop format string, then N values, apply format and print
    PRINT_AT,           // Pop x, y, N text items, fg, bg; concatenate and display
    PRINT_AT_USING,     // Pop x, y, format, N values, fg, bg; format and display
    INPUT_AT,           // Pop x, y; input text at coordinates (operands: prompt, var name)
    INPUT,              // Read input (operand: var name)
    INPUT_PROMPT,       // Read input with prompt (operand: prompt string)

    // === File I/O Operations ===
    OPEN_FILE,          // Open file (operands: filename, mode, filenum)
    CLOSE_FILE,         // Close file (operand: filenum)
    CLOSE_FILE_ALL,     // Close all files
    PRINT_FILE,         // Print to file (operands: filenum, separator)
    PRINT_FILE_NEWLINE, // Print newline to file (operand: filenum)
    INPUT_FILE,         // Read from file (operands: filenum, varname)
    LINE_INPUT_FILE,    // Read line from file (operands: filenum, varname)
    WRITE_FILE,         // Write quoted to file (operands: filenum, value)

    // === Data Statement Support ===
    READ_DATA,          // Pop var name, read next DATA value
    RESTORE,            // Reset DATA pointer (operand: optional line number)

    // === Loop Support ===
    FOR_INIT,           // Initialize FOR loop (operand: var name)
    FOR_CHECK,          // Check FOR loop condition, jump if done (operand: label ID)
    FOR_NEXT,           // Increment FOR loop, jump to start (operand: label ID)
    FOR_IN_INIT,        // Initialize FOR...IN loop (operand: var name, operand2: index var or empty)
    FOR_IN_CHECK,       // Check FOR...IN loop condition, jump if done (operand: label ID)
    FOR_IN_NEXT,        // Advance FOR...IN loop to next element (operand: label ID)
    
    // WHILE...WEND loop (pre-test)
    WHILE_START,        // Begin WHILE loop; condition on stack (will be popped)
    WHILE_END,          // End WHILE loop (jump back to start)
    
    // REPEAT...UNTIL loop (post-test)
    REPEAT_START,       // Begin REPEAT loop body
    REPEAT_END,         // End REPEAT loop; condition on stack (will be popped)
    
    // DO...LOOP variants
    DO_WHILE_START,     // Begin DO WHILE loop; condition on stack (pre-test)
    DO_UNTIL_START,     // Begin DO UNTIL loop; condition on stack (pre-test)
    DO_START,           // Begin plain DO loop (infinite until EXIT)
    DO_LOOP_WHILE,      // End DO loop with WHILE condition on stack (post-test)
    DO_LOOP_UNTIL,      // End DO loop with UNTIL condition on stack (post-test)
    DO_LOOP_END,        // End plain DO loop

    // === String Operations ===
    STR_CONCAT,         // Pop 2 strings, push concatenation (byte-based)
    UNICODE_CONCAT,     // Pop 2 Unicode strings (codepoint arrays), push concatenation
    STR_LEFT,           // Pop string and length, push left substring
    STR_RIGHT,          // Pop string and length, push right substring
    STR_MID,            // Pop string, start, length, push middle substring

    // === Type Conversion ===
    CONV_TO_INT,        // Pop value, push as integer
    CONV_TO_FLOAT,      // Pop value, push as float
    CONV_TO_STRING,     // Pop value, push as string

    // === User-Defined Types (Records/Structures) ===
    DEFINE_TYPE,        // Define a type constructor (operand: type name)
    CREATE_RECORD,      // Create record instance (operand: type name, result pushed on stack)
    LOAD_MEMBER,        // Pop record, push member value (operand: member name)
    STORE_MEMBER,       // Pop value, pop record, store member (operand: member name)
    LOAD_ARRAY_MEMBER,  // Pop indices, load array element, push member (operand1: array name, operand2: member name)
    STORE_ARRAY_MEMBER, // Pop value, pop indices, load array element, store member (operand1: array name, operand2: member name)

    // === SIMD Array Operations (ARM NEON acceleration) ===
    // PAIR operations (2 doubles per element, float64x2_t)
    SIMD_PAIR_ARRAY_ADD,        // result() = a() + b() for PAIR arrays
    SIMD_PAIR_ARRAY_SUB,        // result() = a() - b() for PAIR arrays
    SIMD_PAIR_ARRAY_SCALE,      // result() = a() * scalar for PAIR arrays
    SIMD_PAIR_ARRAY_ADD_SCALAR, // result() = a() + scalar for PAIR arrays
    SIMD_PAIR_ARRAY_SUB_SCALAR, // result() = a() - scalar for PAIR arrays
    
    // QUAD operations (4 floats per element, float32x4_t)
    SIMD_QUAD_ARRAY_ADD,        // result() = a() + b() for QUAD arrays
    SIMD_QUAD_ARRAY_SUB,        // result() = a() - b() for QUAD arrays
    SIMD_QUAD_ARRAY_SCALE,      // result() = a() * scalar for QUAD arrays
    SIMD_QUAD_ARRAY_ADD_SCALAR, // result() = a() + scalar for QUAD arrays
    SIMD_QUAD_ARRAY_SUB_SCALAR, // result() = a() - scalar for QUAD arrays

    // === Timer Operations ===
    AFTER_TIMER,        // Register one-shot timer: operand1=duration_ms, operand2=handler_name
    EVERY_TIMER,        // Register repeating timer: operand1=duration_ms, operand2=handler_name
    AFTER_FRAMES,       // Register one-shot frame timer: operand1=frame_count, operand2=handler_name
    EVERY_FRAMES,       // Register repeating frame timer: operand1=frame_count, operand2=handler_name
    TIMER_STOP,         // Stop timer: operand=timer_id or handler_name or "ALL"
    TIMER_INTERVAL,     // Set timer check interval: operand=interval_count

    // === Special ===
    NOP,                // No operation
    HALT,               // End program execution
    END                 // Synonym for HALT
};

inline const char* opcodeToString(IROpcode op) {
    switch (op) {
        case IROpcode::PUSH_INT: return "PUSH_INT";
        case IROpcode::PUSH_FLOAT: return "PUSH_FLOAT";
        case IROpcode::PUSH_DOUBLE: return "PUSH_DOUBLE";
        case IROpcode::PUSH_STRING: return "PUSH_STRING";
        case IROpcode::POP: return "POP";
        case IROpcode::DUP: return "DUP";
        case IROpcode::ADD: return "ADD";
        case IROpcode::SUB: return "SUB";
        case IROpcode::MUL: return "MUL";
        case IROpcode::DIV: return "DIV";
        case IROpcode::IDIV: return "IDIV";
        case IROpcode::MOD: return "MOD";
        case IROpcode::POW: return "POW";
        case IROpcode::NEG: return "NEG";
        case IROpcode::NOT: return "NOT";
        case IROpcode::EQ: return "EQ";
        case IROpcode::NE: return "NE";
        case IROpcode::LT: return "LT";
        case IROpcode::LE: return "LE";
        case IROpcode::GT: return "GT";
        case IROpcode::GE: return "GE";
        case IROpcode::AND: return "AND";
        case IROpcode::OR: return "OR";
        case IROpcode::XOR: return "XOR";
        case IROpcode::EQV: return "EQV";
        case IROpcode::IMP: return "IMP";
        case IROpcode::LOAD_VAR: return "LOAD_VAR";
        case IROpcode::STORE_VAR: return "STORE_VAR";
        case IROpcode::LOAD_CONST: return "LOAD_CONST";
        case IROpcode::MID_ASSIGN: return "MID_ASSIGN";
        case IROpcode::LOAD_ARRAY: return "LOAD_ARRAY";
        case IROpcode::STORE_ARRAY: return "STORE_ARRAY";
        case IROpcode::DIM_ARRAY: return "DIM_ARRAY";
        case IROpcode::REDIM_ARRAY: return "REDIM_ARRAY";
        case IROpcode::ERASE_ARRAY: return "ERASE_ARRAY";
        case IROpcode::LBOUND_ARRAY: return "LBOUND_ARRAY";
        case IROpcode::UBOUND_ARRAY: return "UBOUND_ARRAY";
        case IROpcode::SWAP_VAR: return "SWAP_VAR";
        case IROpcode::LABEL: return "LABEL";
        case IROpcode::JUMP: return "JUMP";
        case IROpcode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case IROpcode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case IROpcode::CALL_GOSUB: return "CALL_GOSUB";
        case IROpcode::RETURN_GOSUB: return "RETURN_GOSUB";
        case IROpcode::ON_GOTO: return "ON_GOTO";
        case IROpcode::ON_GOSUB: return "ON_GOSUB";
        case IROpcode::ON_CALL: return "ON_CALL";
        case IROpcode::ON_EVENT: return "ON_EVENT";
        case IROpcode::IF_START: return "IF_START";
        case IROpcode::ELSEIF_START: return "ELSEIF_START";
        case IROpcode::ELSE_START: return "ELSE_START";
        case IROpcode::IF_END: return "IF_END";
        case IROpcode::CALL_BUILTIN: return "CALL_BUILTIN";
        case IROpcode::CALL_USER_FN: return "CALL_USER_FN";
        case IROpcode::CALL_FUNCTION: return "CALL_FUNCTION";
        case IROpcode::CALL_SUB: return "CALL_SUB";
        case IROpcode::DEFINE_FUNCTION: return "DEFINE_FUNCTION";
        case IROpcode::DEFINE_SUB: return "DEFINE_SUB";
        case IROpcode::END_FUNCTION: return "END_FUNCTION";
        case IROpcode::END_SUB: return "END_SUB";
        case IROpcode::RETURN_VALUE: return "RETURN_VALUE";
        case IROpcode::RETURN_VOID: return "RETURN_VOID";
        case IROpcode::DECLARE_LOCAL: return "DECLARE_LOCAL";
        case IROpcode::DECLARE_SHARED: return "DECLARE_SHARED";
        case IROpcode::PARAM_BYREF: return "PARAM_BYREF";
        case IROpcode::EXIT_FOR: return "EXIT_FOR";
        case IROpcode::EXIT_DO: return "EXIT_DO";
        case IROpcode::EXIT_WHILE: return "EXIT_WHILE";
        case IROpcode::EXIT_REPEAT: return "EXIT_REPEAT";
        case IROpcode::EXIT_FUNCTION: return "EXIT_FUNCTION";
        case IROpcode::EXIT_SUB: return "EXIT_SUB";
        case IROpcode::PRINT: return "PRINT";
        case IROpcode::PRINT_NEWLINE: return "PRINT_NEWLINE";
        case IROpcode::PRINT_TAB: return "PRINT_TAB";
        case IROpcode::PRINT_USING: return "PRINT_USING";
        case IROpcode::PRINT_AT: return "PRINT_AT";
        case IROpcode::PRINT_AT_USING: return "PRINT_AT_USING";
        case IROpcode::INPUT_AT: return "INPUT_AT";
        case IROpcode::INPUT: return "INPUT";
        case IROpcode::INPUT_PROMPT: return "INPUT_PROMPT";
        case IROpcode::OPEN_FILE: return "OPEN_FILE";
        case IROpcode::CLOSE_FILE: return "CLOSE_FILE";
        case IROpcode::CLOSE_FILE_ALL: return "CLOSE_FILE_ALL";
        case IROpcode::PRINT_FILE: return "PRINT_FILE";
        case IROpcode::PRINT_FILE_NEWLINE: return "PRINT_FILE_NEWLINE";
        case IROpcode::INPUT_FILE: return "INPUT_FILE";
        case IROpcode::LINE_INPUT_FILE: return "LINE_INPUT_FILE";
        case IROpcode::WRITE_FILE: return "WRITE_FILE";
        case IROpcode::READ_DATA: return "READ_DATA";
        case IROpcode::RESTORE: return "RESTORE";
        case IROpcode::FOR_INIT: return "FOR_INIT";
        case IROpcode::FOR_CHECK: return "FOR_CHECK";
        case IROpcode::FOR_NEXT: return "FOR_NEXT";
        case IROpcode::FOR_IN_INIT: return "FOR_IN_INIT";
        case IROpcode::FOR_IN_CHECK: return "FOR_IN_CHECK";
        case IROpcode::FOR_IN_NEXT: return "FOR_IN_NEXT";
        case IROpcode::WHILE_START: return "WHILE_START";
        case IROpcode::WHILE_END: return "WHILE_END";
        case IROpcode::REPEAT_START: return "REPEAT_START";
        case IROpcode::REPEAT_END: return "REPEAT_END";
        case IROpcode::DO_WHILE_START: return "DO_WHILE_START";
        case IROpcode::DO_UNTIL_START: return "DO_UNTIL_START";
        case IROpcode::DO_START: return "DO_START";
        case IROpcode::DO_LOOP_WHILE: return "DO_LOOP_WHILE";
        case IROpcode::DO_LOOP_UNTIL: return "DO_LOOP_UNTIL";
        case IROpcode::DO_LOOP_END: return "DO_LOOP_END";
        case IROpcode::STR_CONCAT: return "STR_CONCAT";
        case IROpcode::STR_LEFT: return "STR_LEFT";
        case IROpcode::STR_RIGHT: return "STR_RIGHT";
        case IROpcode::STR_MID: return "STR_MID";
        case IROpcode::CONV_TO_INT: return "CONV_TO_INT";
        case IROpcode::CONV_TO_FLOAT: return "CONV_TO_FLOAT";
        case IROpcode::CONV_TO_STRING: return "CONV_TO_STRING";
        case IROpcode::DEFINE_TYPE: return "DEFINE_TYPE";
        case IROpcode::CREATE_RECORD: return "CREATE_RECORD";
        case IROpcode::LOAD_MEMBER: return "LOAD_MEMBER";
        case IROpcode::STORE_MEMBER: return "STORE_MEMBER";
        case IROpcode::LOAD_ARRAY_MEMBER: return "LOAD_ARRAY_MEMBER";
        case IROpcode::STORE_ARRAY_MEMBER: return "STORE_ARRAY_MEMBER";
        case IROpcode::SIMD_PAIR_ARRAY_ADD: return "SIMD_PAIR_ARRAY_ADD";
        case IROpcode::SIMD_PAIR_ARRAY_SUB: return "SIMD_PAIR_ARRAY_SUB";
        case IROpcode::SIMD_PAIR_ARRAY_SCALE: return "SIMD_PAIR_ARRAY_SCALE";
        case IROpcode::SIMD_PAIR_ARRAY_ADD_SCALAR: return "SIMD_PAIR_ARRAY_ADD_SCALAR";
        case IROpcode::SIMD_PAIR_ARRAY_SUB_SCALAR: return "SIMD_PAIR_ARRAY_SUB_SCALAR";
        case IROpcode::SIMD_QUAD_ARRAY_ADD: return "SIMD_QUAD_ARRAY_ADD";
        case IROpcode::SIMD_QUAD_ARRAY_SUB: return "SIMD_QUAD_ARRAY_SUB";
        case IROpcode::SIMD_QUAD_ARRAY_SCALE: return "SIMD_QUAD_ARRAY_SCALE";
        case IROpcode::SIMD_QUAD_ARRAY_ADD_SCALAR: return "SIMD_QUAD_ARRAY_ADD_SCALAR";
        case IROpcode::SIMD_QUAD_ARRAY_SUB_SCALAR: return "SIMD_QUAD_ARRAY_SUB_SCALAR";
        case IROpcode::AFTER_TIMER: return "AFTER_TIMER";
        case IROpcode::EVERY_TIMER: return "EVERY_TIMER";
        case IROpcode::AFTER_FRAMES: return "AFTER_FRAMES";
        case IROpcode::EVERY_FRAMES: return "EVERY_FRAMES";
        case IROpcode::TIMER_STOP: return "TIMER_STOP";
        case IROpcode::TIMER_INTERVAL: return "TIMER_INTERVAL";
        case IROpcode::NOP: return "NOP";
        case IROpcode::HALT: return "HALT";
        case IROpcode::END: return "END";
    }
    return "UNKNOWN";
}

// =============================================================================
// IR Operand (variant type)
// =============================================================================

using IROperand = std::variant<
    std::monostate,     // No operand
    int,                // Integer operand (labels, counts, etc.)
    double,             // Float/double operand
    std::string         // String operand (var names, strings, etc.)
>;

// =============================================================================
// IR Instruction
// =============================================================================

struct IRInstruction {
    IROpcode opcode;
    IROperand operand1;
    IROperand operand2;
    IROperand operand3;

    // Source information for debugging
    int sourceLineNumber;    // BASIC line number (0 if N/A)
    int blockId;             // CFG block ID this came from

    // Array type metadata (may be useful for future optimizations)
    // Used by DIM_ARRAY, LOAD_ARRAY, STORE_ARRAY to carry element type info
    std::string arrayElementTypeSuffix;  // "%", "#", "!", "$", "&", or ""
    
    // User-defined type name for TYPE declarations (empty for built-in types)
    std::string userDefinedType;
    
    // Loop jump flag for GOTO cancellation (used by JUMP opcode)
    bool isLoopJump;  // True if this JUMP creates a loop (backward edge)

    IRInstruction()
        : opcode(IROpcode::NOP)
        , sourceLineNumber(0)
        , blockId(-1)
        , arrayElementTypeSuffix("")
        , userDefinedType("")
        , isLoopJump(false)
    {}

    explicit IRInstruction(IROpcode op)
        : opcode(op)
        , sourceLineNumber(0)
        , blockId(-1)
        , arrayElementTypeSuffix("")
        , userDefinedType("")
        , isLoopJump(false)
    {}

    IRInstruction(IROpcode op, IROperand op1)
        : opcode(op)
        , operand1(op1)
        , sourceLineNumber(0)
        , blockId(-1)
        , arrayElementTypeSuffix("")
        , userDefinedType("")
        , isLoopJump(false)
    {}

    IRInstruction(IROpcode op, IROperand op1, IROperand op2)
        : opcode(op)
        , operand1(op1)
        , operand2(op2)
        , sourceLineNumber(0)
        , blockId(-1)
        , arrayElementTypeSuffix("")
        , userDefinedType("")
        , isLoopJump(false)
    {}

    IRInstruction(IROpcode op, IROperand op1, IROperand op2, IROperand op3)
        : opcode(op)
        , operand1(op1)
        , operand2(op2)
        , operand3(op3)
        , sourceLineNumber(0)
        , blockId(-1)
        , arrayElementTypeSuffix("")
        , userDefinedType("")
        , isLoopJump(false)
    {}

    // Helper to format operand for display
    static std::string formatOperand(const IROperand& op) {
        if (std::holds_alternative<std::monostate>(op)) {
            return "";
        } else if (std::holds_alternative<int>(op)) {
            return std::to_string(std::get<int>(op));
        } else if (std::holds_alternative<double>(op)) {
            return std::to_string(std::get<double>(op));
        } else if (std::holds_alternative<std::string>(op)) {
            const auto& s = std::get<std::string>(op);
            // Quote strings unless they look like identifiers
            if (s.empty() || !isalpha(s[0])) {
                return "\"" + s + "\"";
            }
            return s;
        }
        return "???";
    }

    // Convert instruction to string for debugging
    std::string toString() const {
        std::ostringstream oss;
        oss << opcodeToString(opcode);

        std::string op1 = formatOperand(operand1);
        std::string op2 = formatOperand(operand2);
        std::string op3 = formatOperand(operand3);

        if (!op1.empty()) {
            oss << " " << op1;
            if (!op2.empty()) {
                oss << ", " << op2;
                if (!op3.empty()) {
                    oss << ", " << op3;
                }
            }
        }

        return oss.str();
    }
};

// =============================================================================
// IR Code Container
// =============================================================================

class IRCode {
public:
    std::vector<IRInstruction> instructions;

    // Label resolution: label ID → instruction index
    std::map<int, int> labelToAddress;

    // Line number mapping: BASIC line number → instruction index
    std::map<int, int> lineToAddress;

    // Data segment (for DATA/READ/RESTORE)
    std::vector<std::string> dataValues;
    std::unordered_map<int, size_t> dataLineRestorePoints;      // Line number → index in dataValues
    std::unordered_map<std::string, size_t> dataLabelRestorePoints;  // Label name → index in dataValues

    // Constants (for inlining constant values in generated code)
    const class ConstantsManager* constantsManager;  // Pointer to constants for code generation

    // User-defined types (for constructor generation in codegen)
    std::unordered_map<std::string, struct TypeSymbol> types;  // Type name → type definition

    // Metadata
    int blockCount;
    int labelCount;
    int arrayBase;  // OPTION BASE: 0 or 1 (default 1)
    bool unicodeMode;  // OPTION UNICODE: strings as codepoint arrays
    bool errorTracking;  // OPTION ERROR: emit _LINE tracking for error messages
    bool cancellableLoops;  // OPTION CANCELLABLE: inject script cancellation checks in loops
    bool eventsUsed;  // EVENT DETECTION: if true, program uses ON EVENT statements and needs event processing code
    bool forceYieldEnabled;  // OPTION FORCE_YIELD: enable quasi-preemptive handler yielding
    int forceYieldBudget;  // OPTION FORCE_YIELD budget: instructions before forced yield

    IRCode()
        : blockCount(0)
        , labelCount(0)
        , arrayBase(1)  // Default to 1 (matches Lua arrays)
        , unicodeMode(false)  // Default to standard byte strings
        , errorTracking(true)  // Default to line tracking enabled (better UX - shows BASIC line numbers in errors)
        , cancellableLoops(true)  // Default to cancellation checks enabled (better UX)
        , eventsUsed(false)  // Default to no events (zero overhead when not used)
        , forceYieldEnabled(false)  // Default to cooperative (no forced yielding)
        , forceYieldBudget(10000)  // Default instruction budget
    {}

    // Add an instruction
    int emit(const IRInstruction& instr) {
        int addr = static_cast<int>(instructions.size());
        instructions.push_back(instr);
        return addr;
    }

    // Add a label and record its address
    int emitLabel(int labelId, int blockId = -1) {
        int addr = static_cast<int>(instructions.size());
        IRInstruction instr(IROpcode::LABEL, labelId);
        instr.blockId = blockId;
        instructions.push_back(instr);
        labelToAddress[labelId] = addr;
        return addr;
    }

    // Get instruction count
    size_t size() const {
        return instructions.size();
    }

    // Check if empty
    bool empty() const {
        return instructions.empty();
    }

    // Resolve label to address
    int getLabelAddress(int labelId) const {
        auto it = labelToAddress.find(labelId);
        if (it != labelToAddress.end()) {
            return it->second;
        }
        return -1;
    }

    // Resolve line number to address
    int getLineAddress(int lineNumber) const {
        auto it = lineToAddress.find(lineNumber);
        if (it != lineToAddress.end()) {
            return it->second;
        }
        return -1;
    }

    // Generate human-readable listing
    std::string toString() const {
        std::ostringstream oss;

        for (size_t i = 0; i < instructions.size(); i++) {
            const auto& instr = instructions[i];

            // Address
            oss << std::setw(5) << std::setfill('0') << i << ": ";

            // Instruction
            oss << instr.toString();

            // Source info
            if (instr.sourceLineNumber > 0) {
                oss << "  ; line " << instr.sourceLineNumber;
            }

            oss << "\n";
        }

        return oss.str();
    }
};

// =============================================================================
// IR Generator
// =============================================================================

class IRGenerator {
public:
    IRGenerator();
    ~IRGenerator() = default;

    // Generate IR from CFG
    std::unique_ptr<IRCode> generate(
        const ControlFlowGraph& cfg,
        const SymbolTable& symbols
    );

    // Configuration
    void setTraceEnabled(bool enable) { m_traceEnabled = enable; }

    // Generate report
    std::string generateReport(const IRCode& code) const;

private:
    // Generation state
    const ControlFlowGraph* m_cfg;
    const SymbolTable* m_symbols;
    std::unique_ptr<IRCode> m_code;
    int m_nextLabel;

    // Block to label mapping
    std::map<int, int> m_blockLabels;

    // User-defined function storage (for DEF FN inlining)
    struct UserFunction {
        std::string name;
        std::vector<std::string> parameters;
        const Expression* body;  // AST expression to inline
    };
    std::map<std::string, UserFunction> m_userFunctions;

    // Multi-statement FUNCTION definitions (not inlined)
    struct FunctionDef {
        std::string name;
        std::vector<std::string> parameters;
        std::vector<TokenType> parameterTypes;
        std::vector<bool> parameterIsByRef;
        TokenType returnType;
        const FunctionStatement* astNode;
    };
    std::map<std::string, FunctionDef> m_functions;

    // Multi-statement SUB definitions (not inlined)
    struct SubDef {
        std::string name;
        std::vector<std::string> parameters;
        std::vector<TokenType> parameterTypes;
        std::vector<bool> parameterIsByRef;
        const SubStatement* astNode;
    };
    std::map<std::string, SubDef> m_subs;

    // Function inlining state
    bool m_inFunctionInlining;
    std::map<std::string, std::string> m_parameterMap;  // param name -> temp var name

    // Configuration
    bool m_traceEnabled;

    // Loop label stacks for proper jump-back handling
    std::vector<int> m_whileLoopLabels;  // Stack of WHILE loop start labels

    // === Code Generation Methods ===

    // Generate code for a basic block
    void generateBlock(const BasicBlock& block);

    // Generate code for a statement
    void generateStatement(const Statement* stmt, int lineNumber);

    // Generate code for an expression (leaves result on stack)
    void generateExpression(const Expression* expr);

    // Generate code for specific statement types
    void generatePrint(const PrintStatement* stmt, int lineNumber);
    void generateConsole(const ConsoleStatement* stmt, int lineNumber);
    void generatePrintAt(const PrintAtStatement* stmt, int lineNumber);
    void generatePlay(const PlayStatement* stmt, int lineNumber);
    void generatePlaySound(const PlaySoundStatement* stmt, int lineNumber);
    void generateInputAt(const InputAtStatement* stmt, int lineNumber);
    void generateLet(const LetStatement* stmt, int lineNumber);
    void generateMidAssign(const MidAssignStatement* stmt, int lineNumber);
    void generateIf(const IfStatement* stmt, int lineNumber);
    void generateCase(const CaseStatement* stmt, int lineNumber);
    void generateFor(const ForStatement* stmt, int lineNumber);
    void generateForIn(const ForInStatement* stmt, int lineNumber);
    void generateNext(const NextStatement* stmt, int lineNumber);
    void generateWhile(const WhileStatement* stmt, int lineNumber);
    void generateWend(const WendStatement* stmt, int lineNumber);
    void generateRepeat(const RepeatStatement* stmt, int lineNumber);
    void generateUntil(const UntilStatement* stmt, int lineNumber);
    void generateDo(const DoStatement* stmt, int lineNumber);
    void generateLoop(const LoopStatement* stmt, int lineNumber);
    void generateGoto(const GotoStatement* stmt, int lineNumber);
    void generateGosub(const GosubStatement* stmt, int lineNumber);
    void generateOnGoto(const OnGotoStatement* stmt, int lineNumber);
    void generateOnGosub(const OnGosubStatement* stmt, int lineNumber);
    void generateOnCall(const OnCallStatement* stmt, int lineNumber);
    void generateOnEvent(const OnEventStatement* stmt, int lineNumber);
    void generateConstant(const ConstantStatement* stmt, int lineNumber);
    void generateLabel(const LabelStatement* stmt, int lineNumber);
    void generateReturn(const ReturnStatement* stmt, int lineNumber);
    void generateExit(const ExitStatement* stmt, int lineNumber);
    void generateDim(const DimStatement* stmt, int lineNumber);
    void generateRedim(const RedimStatement* stmt, int lineNumber);
    void generateErase(const EraseStatement* stmt, int lineNumber);
    void generateSwap(const SwapStatement* stmt, int lineNumber);
    void generateInc(const IncStatement* stmt, int lineNumber);
    void generateDec(const DecStatement* stmt, int lineNumber);
    void generateInput(const InputStatement* stmt, int lineNumber);
    void generateRead(const ReadStatement* stmt, int lineNumber);
    void generateRestore(const RestoreStatement* stmt, int lineNumber);
    void generateOpen(const OpenStatement* stmt, int lineNumber);
    void generateClose(const CloseStatement* stmt, int lineNumber);
    void generateEnd(const EndStatement* stmt, int lineNumber);
    void generateRem(const RemStatement* stmt, int lineNumber);
    void generateDef(const DefStatement* stmt, int lineNumber);
    void generateFunction(const FunctionStatement* stmt, int lineNumber);
    void generateSub(const SubStatement* stmt, int lineNumber);
    void generateCall(const CallStatement* stmt, int lineNumber);
    void generateLocal(const LocalStatement* stmt, int lineNumber);
    void generateShared(const SharedStatement* stmt, int lineNumber);
    void generateExpressionStatement(const ExpressionStatement* stmt, int lineNumber);
    void generateSimpleStatement(const SimpleStatement* stmt, int lineNumber);
    
    // Timer statement generation
    void generateAfter(const AfterStatement* stmt, int lineNumber);
    void generateEvery(const EveryStatement* stmt, int lineNumber);
    void generateAfterFrames(const AfterFramesStatement* stmt, int lineNumber);
    void generateEveryFrame(const EveryFrameStatement* stmt, int lineNumber);
    void generateRun(const RunStatement* stmt, int lineNumber);
    void generateTimerStop(const TimerStopStatement* stmt, int lineNumber);
    void generateTimerInterval(const TimerIntervalStatement* stmt, int lineNumber);
    
    // Type declaration statement generation
    void generateTypeDeclaration(const TypeDeclarationStatement* stmt, int lineNumber);

    // === Helper Methods ===

    // Get or create a label for a block
    int getLabelForBlock(int blockId);

    // Get or create a label for a line number
    int getLabelForLineNumber(int lineNumber);

    // Allocate a new label
    int allocateLabel();

    // Emit instruction helper
    void emit(IROpcode opcode);
    void emit(IROpcode opcode, IROperand op1);
    void emit(IROpcode opcode, IROperand op1, IROperand op2);
    void emit(IROpcode opcode, IROperand op1, IROperand op2, IROperand op3);
    
    // Emit jump instruction with loop flag
    void emitLoopJump(IROpcode opcode, IROperand op1, bool isLoop);

    // Function inlining helper
    void generateInlinedFunction(const std::string& funcName,
                                  const std::vector<const Expression*>& arguments);
    
    // Type checking helpers
    bool isStringExpression(const Expression* expr) const;
    
    // Expression serialization helper (for deferred WHILE condition evaluation)
    std::string serializeExpression(const Expression* expr);
    
    // SIMD helper: Try to emit SIMD IR opcodes for whole-array operations
    // Returns true if SIMD operation was emitted, false to fall back to standard codegen
    bool tryEmitSIMDArrayOperation(const LetStatement* stmt,
                                   const ArraySymbol& lhsArray,
                                   const TypeSymbol& lhsType);
    
    // Array helper: Try to emit element-wise array operation IR opcodes for regular arrays
    // Returns true if array operation was emitted, false to fall back to scalar fill
    bool tryEmitArrayOperation(const LetStatement* stmt,
                               const ArraySymbol& lhsArray);

    // Set current source context for emitted instructions
    void setSourceContext(int lineNumber, int blockId);

    // Current context
    int m_currentLineNumber;
    int m_currentBlockId;
};

} // namespace FasterBASIC

#endif // FASTERBASIC_IRCODE_H
