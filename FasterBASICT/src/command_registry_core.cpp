//
// command_registry_core.cpp
// FasterBASICT - Core BASIC Commands Registry Implementation
//
// Implements core BASIC commands and functions that are available to all
// FasterBASICT applications. These are fundamental BASIC language constructs.
//

#include "command_registry_core.h"
#include "command_registry_plugins.h"
#include <string>

namespace FasterBASIC {
namespace ModularCommands {

// =============================================================================
// Core Command Registration
// =============================================================================

void CoreCommandRegistry::registerCoreCommands(CommandRegistry& registry) {
    registerBasicIOCommands(registry);
    registerMathCommands(registry);
    registerStringCommands(registry);
    registerControlFlowCommands(registry);
    registerDataCommands(registry);
    registerTestCommands(registry);
    registerFileIOCommands(registry);
    
    // Register plugin management commands
    PluginCommandRegistry::registerPluginCommands(registry);
}

void CoreCommandRegistry::registerCoreFunctions(CommandRegistry& registry) {
    registerMathFunctions(registry);
    registerStringFunctions(registry);
    registerSystemFunctions(registry);
    registerFileIOFunctions(registry);
}

// =============================================================================
// Test Commands (for parser debugging - to be removed after investigation)
// =============================================================================

void CoreCommandRegistry::registerTestCommands(CommandRegistry& registry) {
    // TESTCMD - Simple test command for investigating expression parsing
    // This mimics RECTF structure to debug why expressions confuse the parser
    CommandDefinition testcmd("TESTCMD",
                              "Test command for parser expression investigation",
                              "testcmd", "test");
    testcmd.addParameter("x", ParameterType::INT, "X parameter")
           .addParameter("y", ParameterType::INT, "Y parameter")
           .addParameter("w", ParameterType::INT, "Width parameter")
           .addParameter("h", ParameterType::INT, "Height parameter")
           .addParameter("color", ParameterType::COLOR, "Color parameter", true, "0xFFFFFFFF");
    registry.registerCommand(std::move(testcmd));
}

// =============================================================================
// Basic I/O Commands
// =============================================================================

void CoreCommandRegistry::registerBasicIOCommands(CommandRegistry& registry) {
    registerBasicPrint(registry);
    registerBasicInput(registry);
    registerFileIO(registry);
    
    // Note: CLS is platform-specific I/O command, handled by individual applications
    // - Terminal apps: use terminal_clear_screen()  
    // - GUI apps: use st_text_clear() or equivalent
    // Core registry contains only universal BASIC language features
}

void CoreCommandRegistry::registerBasicPrint(CommandRegistry& registry) {
    // PRINT - Basic output command
    CommandDefinition print("PRINT", 
                           "Output text to console",
                           "print", "io");
    print.addParameter("text", ParameterType::STRING, "Text to print", true, "\"\"");
    registry.registerCommand(std::move(print));
    
    // PRINT with separator handling (PRINT A;B;C)
    CommandDefinition printList("PRINT_LIST",
                               "Print multiple values with separators",
                               "print_list", "io");
    printList.addParameter("values", ParameterType::STRING, "Comma or semicolon separated values");
    registry.registerCommand(std::move(printList));
}

void CoreCommandRegistry::registerBasicInput(CommandRegistry& registry) {
    // INPUT - Basic input command
    CommandDefinition input("INPUT",
                           "Read input from user",
                           "input", "io");
    input.addParameter("prompt", ParameterType::STRING, "Input prompt", true, "\"\"")
         .addParameter("variable", ParameterType::STRING, "Variable to store input in");
    registry.registerCommand(std::move(input));
    
    // LINE INPUT - Read entire line including spaces and punctuation
    CommandDefinition lineInput("LINE_INPUT",
                               "Read entire line of input",
                               "line_input", "io");
    lineInput.addParameter("prompt", ParameterType::STRING, "Input prompt", true, "\"\"")
             .addParameter("variable", ParameterType::STRING, "Variable to store input in");
    registry.registerCommand(std::move(lineInput));
}

void CoreCommandRegistry::registerFileIO(CommandRegistry& registry) {
    // OPEN - Open file for I/O
    CommandDefinition open("OPEN",
                          "Open file for input/output",
                          "file_open", "file");
    open.addParameter("filename", ParameterType::STRING, "File path to open")
        .addParameter("mode", ParameterType::STRING, "File mode (\"r\", \"w\", \"a\")")
        .addParameter("channel", ParameterType::INT, "File channel number");
    registry.registerCommand(std::move(open));
    
    // CLOSE - Close file
    CommandDefinition close("CLOSE",
                           "Close file channel",
                           "file_close", "file");
    close.addParameter("channel", ParameterType::INT, "File channel number");
    registry.registerCommand(std::move(close));
    
    // PRINT# - Print to file
    CommandDefinition printFile("PRINT_FILE",
                               "Print text to file channel",
                               "file_print", "file");
    printFile.addParameter("channel", ParameterType::INT, "File channel number")
             .addParameter("text", ParameterType::STRING, "Text to write");
    registry.registerCommand(std::move(printFile));
    
    // INPUT# - Read from file
    CommandDefinition inputFile("INPUT_FILE",
                               "Read from file channel",
                               "file_input", "file");
    inputFile.addParameter("channel", ParameterType::INT, "File channel number")
             .addParameter("variable", ParameterType::STRING, "Variable to store input in");
    registry.registerCommand(std::move(inputFile));
}

// =============================================================================
// Math Commands
// =============================================================================

void CoreCommandRegistry::registerMathCommands(CommandRegistry& registry) {
    // RANDOMIZE - Initialize random number generator
    CommandDefinition randomize("RANDOMIZE",
                               "Initialize random number generator",
                               "randomize", "math");
    randomize.addParameter("seed", ParameterType::INT, "Random seed value", true, "nil");
    registry.registerCommand(std::move(randomize));
}

// =============================================================================
// String Commands  
// =============================================================================

void CoreCommandRegistry::registerStringCommands(CommandRegistry& registry) {
    // No string-specific commands at core level - strings handled by functions
}

// =============================================================================
// Control Flow Commands
// =============================================================================

void CoreCommandRegistry::registerControlFlowCommands(CommandRegistry& registry) {
    // CALL - Call user-defined subroutine
    CommandDefinition call("CALL",
                          "Call user-defined subroutine or function",
                          "call_user_function", "control");
    call.addParameter("name", ParameterType::STRING, "Function/subroutine name")
        .addParameter("args", ParameterType::STRING, "Function arguments", true, "");
    registry.registerCommand(std::move(call));
    
    // SLEEP - Pause execution
    CommandDefinition sleep("SLEEP",
                           "Pause execution for specified time",
                           "basic_sleep", "control");
    sleep.addParameter("seconds", ParameterType::FLOAT, "Sleep duration in seconds");
    registry.registerCommand(std::move(sleep));
}

// =============================================================================
// Data Commands
// =============================================================================

void CoreCommandRegistry::registerDataCommands(CommandRegistry& registry) {
    registerArrayCommands(registry);
    
    // READ - Read from DATA statements
    CommandDefinition read("READ",
                          "Read values from DATA statements",
                          "data_read", "data");
    read.addParameter("variables", ParameterType::STRING, "Comma-separated variable list");
    registry.registerCommand(std::move(read));
    
    // RESTORE - Reset DATA pointer
    CommandDefinition restore("RESTORE",
                             "Reset DATA statement pointer",
                             "data_restore", "data");
    restore.addParameter("label", ParameterType::STRING, "Optional label to restore to", true, "nil");
    registry.registerCommand(std::move(restore));
    
    // DATA - Data storage (handled specially by preprocessor)
    CommandDefinition data("DATA",
                          "Store data values for READ statements",
                          "data_store", "data");
    data.addParameter("values", ParameterType::STRING, "Comma-separated data values");
    registry.registerCommand(std::move(data));
}

void CoreCommandRegistry::registerArrayCommands(CommandRegistry& registry) {
    // DIM - Declare arrays
    CommandDefinition dim("DIM",
                         "Declare array variables",
                         "dim_array", "data");
    dim.addParameter("declarations", ParameterType::STRING, "Array declarations (e.g., \"A(10), B$(20)\")");
    registry.registerCommand(std::move(dim));
    
    // REDIM - Redimension arrays (if supported)
    CommandDefinition redim("REDIM",
                           "Redimension existing arrays",
                           "redim_array", "data");
    redim.addParameter("declarations", ParameterType::STRING, "Array redeclarations");
    registry.registerCommand(std::move(redim));
}

// =============================================================================
// Core Functions
// =============================================================================

void CoreCommandRegistry::registerMathFunctions(CommandRegistry& registry) {
    // ABS - Absolute value
    CommandDefinition abs("ABS", "Return absolute value of number", "math.abs", "math");
    abs.addParameter("x", ParameterType::FLOAT, "Input number")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(abs));
    
    // INT - Integer part
    CommandDefinition int_fn("INT", "Return integer part of number", "math.floor", "math");
    int_fn.addParameter("x", ParameterType::FLOAT, "Input number")
          .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(int_fn));
    
    // RND - Random number
    CommandDefinition rnd("RND", "Return random number 0-1", "math.random", "math");
    rnd.addParameter("seed", ParameterType::INT, "Random range (optional)", true, "1")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(rnd));
    
    // SQR - Square root
    CommandDefinition sqr("SQR", "Return square root", "math.sqrt", "math");
    sqr.addParameter("x", ParameterType::FLOAT, "Input number")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(sqr));
    
    // SIN, COS, TAN - Trigonometric functions
    CommandDefinition sin_fn("SIN", "Return sine of angle in radians", "math.sin", "math");
    sin_fn.addParameter("x", ParameterType::FLOAT, "Angle in radians")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(sin_fn));
    
    CommandDefinition cos_fn("COS", "Return cosine of angle in radians", "math.cos", "math");
    cos_fn.addParameter("x", ParameterType::FLOAT, "Angle in radians")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(cos_fn));
    
    CommandDefinition tan_fn("TAN", "Return tangent of angle in radians", "math.tan", "math");
    tan_fn.addParameter("x", ParameterType::FLOAT, "Angle in radians")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(tan_fn));
    
    // ATN - Arctangent
    CommandDefinition atn("ATN", "Return arctangent in radians", "math.atan", "math");
    atn.addParameter("x", ParameterType::FLOAT, "Input value")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(atn));
    
    // EXP - Exponential
    CommandDefinition exp_fn("EXP", "Return e^x", "math.exp", "math");
    exp_fn.addParameter("x", ParameterType::FLOAT, "Exponent")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(exp_fn));
    
    // LOG - Natural logarithm
    CommandDefinition log_fn("LOG", "Return natural logarithm", "math.log", "math");
    log_fn.addParameter("x", ParameterType::FLOAT, "Input value (must be > 0)")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(log_fn));
    
    // ACS - Arc-cosine (inverse cosine)
    CommandDefinition acs("ACS", "Return arc-cosine in radians", "math.acos", "math");
    acs.addParameter("x", ParameterType::FLOAT, "Input value (-1 to 1)")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(acs));
    
    // ASN - Arc-sine (inverse sine)
    CommandDefinition asn("ASN", "Return arc-sine in radians", "math.asin", "math");
    asn.addParameter("x", ParameterType::FLOAT, "Input value (-1 to 1)")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(asn));
    
    // DEG - Convert radians to degrees
    CommandDefinition deg("DEG", "Convert radians to degrees", "math.deg", "math");
    deg.addParameter("x", ParameterType::FLOAT, "Angle in radians")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(deg));
    
    // RAD - Convert degrees to radians
    CommandDefinition rad("RAD", "Convert degrees to radians", "math.rad", "math");
    rad.addParameter("x", ParameterType::FLOAT, "Angle in degrees")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(rad));
    
    // SGN - Sign function (-1, 0, or 1)
    CommandDefinition sgn("SGN", "Return sign of number", "basic_sgn", "math");
    sgn.addParameter("x", ParameterType::FLOAT, "Input number")
       .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(sgn));
    
    // PI - Mathematical constant pi
    CommandDefinition pi("PI", "Mathematical constant pi", "math.pi", "math");
    pi.setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(pi));
    
    // LN - Natural logarithm (alias for LOG for BBC BASIC compatibility)
    CommandDefinition ln("LN", "Return natural logarithm", "math.log", "math");
    ln.addParameter("x", ParameterType::FLOAT, "Input value (must be > 0)")
      .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(ln));
    
    // FIX - Truncate towards zero (different from INT which floors)
    CommandDefinition fix("FIX", "Truncate towards zero", "basic_fix", "math");
    fix.addParameter("x", ParameterType::FLOAT, "Input number")
       .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(fix));
    
    // MOD - Enhanced modulo with vector magnitude support
    CommandDefinition mod("MOD", "Modulo or vector magnitude", "basic_mod", "math");
    mod.addParameter("x", ParameterType::FLOAT, "First operand or array")
       .addParameter("y", ParameterType::FLOAT, "Second operand (optional for arrays)", true, "nil")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(mod));
    
    // =========================================================================
    // BCX-Compatible Extended Math Functions
    // =========================================================================
    
    // POW - Power (x^y)
    CommandDefinition pow_fn("POW", "Return x raised to power y", "math_pow", "math");
    pow_fn.addParameter("x", ParameterType::FLOAT, "Base")
          .addParameter("y", ParameterType::FLOAT, "Exponent")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(pow_fn));
    
    // CEIL - Ceiling (round up)
    CommandDefinition ceil("CEIL", "Round up to nearest integer", "math.ceil", "math");
    ceil.addParameter("x", ParameterType::FLOAT, "Input number")
        .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(ceil));
    
    // FLOOR - Floor (round down)
    CommandDefinition floor_fn("FLOOR", "Round down to nearest integer", "math.floor", "math");
    floor_fn.addParameter("x", ParameterType::FLOAT, "Input number")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(floor_fn));
    
    // ROUND - Round to n decimal places
    CommandDefinition round_fn("ROUND", "Round to n decimal places", "math_round", "math");
    round_fn.addParameter("x", ParameterType::FLOAT, "Input number")
            .addParameter("places", ParameterType::INT, "Decimal places", true, "0")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(round_fn));
    
    // TRUNC - Truncate (alias for FIX)
    CommandDefinition trunc("TRUNC", "Truncate towards zero", "basic_fix", "math");
    trunc.addParameter("x", ParameterType::FLOAT, "Input number")
         .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(trunc));
    
    // FRAC - Fractional part
    CommandDefinition frac("FRAC", "Return fractional part of number", "math_frac", "math");
    frac.addParameter("x", ParameterType::FLOAT, "Input number")
        .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(frac));
    
    // Hyperbolic Trig Functions
    CommandDefinition sinh_fn("SINH", "Hyperbolic sine", "math_sinh", "math");
    sinh_fn.addParameter("x", ParameterType::FLOAT, "Input value")
           .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(sinh_fn));
    
    CommandDefinition cosh_fn("COSH", "Hyperbolic cosine", "math_cosh", "math");
    cosh_fn.addParameter("x", ParameterType::FLOAT, "Input value")
           .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(cosh_fn));
    
    CommandDefinition tanh_fn("TANH", "Hyperbolic tangent", "math_tanh", "math");
    tanh_fn.addParameter("x", ParameterType::FLOAT, "Input value")
           .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(tanh_fn));
    
    // Inverse Hyperbolic Functions
    CommandDefinition asinh_fn("ASINH", "Inverse hyperbolic sine", "math_asinh", "math");
    asinh_fn.addParameter("x", ParameterType::FLOAT, "Input value")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(asinh_fn));
    
    CommandDefinition acosh_fn("ACOSH", "Inverse hyperbolic cosine", "math_acosh", "math");
    acosh_fn.addParameter("x", ParameterType::FLOAT, "Input value (must be >= 1)")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(acosh_fn));
    
    CommandDefinition atanh_fn("ATANH", "Inverse hyperbolic tangent", "math_atanh", "math");
    atanh_fn.addParameter("x", ParameterType::FLOAT, "Input value (-1 < x < 1)")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(atanh_fn));
    
    // MIN - Minimum of two numbers
    CommandDefinition min_fn("MIN", "Return minimum of two numbers", "math.min", "math");
    min_fn.addParameter("a", ParameterType::FLOAT, "First number")
          .addParameter("b", ParameterType::FLOAT, "Second number")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(min_fn));
    
    // MAX - Maximum of two numbers
    CommandDefinition max_fn("MAX", "Return maximum of two numbers", "math.max", "math");
    max_fn.addParameter("a", ParameterType::FLOAT, "First number")
          .addParameter("b", ParameterType::FLOAT, "Second number")
          .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(max_fn));
    
    // ATAN2 - Two-argument arctangent
    CommandDefinition atan2_fn("ATAN2", "Return atan2(y, x) in radians", "math.atan2", "math");
    atan2_fn.addParameter("y", ParameterType::FLOAT, "Y coordinate")
            .addParameter("x", ParameterType::FLOAT, "X coordinate")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(atan2_fn));
    
    // LOG10 - Base-10 logarithm
    CommandDefinition log10_fn("LOG10", "Return base-10 logarithm", "math_log10", "math");
    log10_fn.addParameter("x", ParameterType::FLOAT, "Input value (must be > 0)")
            .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(log10_fn));
    
    // Number Conversion Functions
    CommandDefinition bin2dec("BIN2DEC", "Convert binary string to decimal", "math_bin2dec", "math");
    bin2dec.addParameter("binStr", ParameterType::STRING, "Binary string")
           .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(bin2dec));
    
    CommandDefinition hex2dec("HEX2DEC", "Convert hexadecimal string to decimal", "math_hex2dec", "math");
    hex2dec.addParameter("hexStr", ParameterType::STRING, "Hexadecimal string")
           .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(hex2dec));
    
    CommandDefinition oct2dec("OCT2DEC", "Convert octal string to decimal", "math_oct2dec", "math");
    oct2dec.addParameter("octStr", ParameterType::STRING, "Octal string")
           .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(oct2dec));
    
    // Type Conversion Functions
    CommandDefinition cdbl("CDBL", "Convert to double precision", "tonumber", "math");
    cdbl.addParameter("x", ParameterType::FLOAT, "Value to convert")
        .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(cdbl));
    
    CommandDefinition cint("CINT", "Convert to integer (rounded)", "math_cint", "math");
    cint.addParameter("x", ParameterType::FLOAT, "Value to convert")
        .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(cint));
    
    CommandDefinition clng("CLNG", "Convert to long integer", "math_clng", "math");
    clng.addParameter("x", ParameterType::FLOAT, "Value to convert")
        .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(clng));
    
    CommandDefinition csng("CSNG", "Convert to single precision", "tonumber", "math");
    csng.addParameter("x", ParameterType::FLOAT, "Value to convert")
        .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(csng));
    
    // NTH - Check if counter is at every Nth occurrence
    CommandDefinition nth("NTH", "Return true if count is divisible by n (every Nth item)", "math_nth", "math");
    nth.addParameter("count", ParameterType::INT, "Current counter value")
       .addParameter("n", ParameterType::INT, "Interval to check")
       .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(nth));
}

void CoreCommandRegistry::registerStringFunctions(CommandRegistry& registry) {
    // LEN - String length (handled in code generator for Unicode awareness)
    // Not registered here to allow proper unicode.len vs string.len selection
    
    // LEFT$ - Left substring
    CommandDefinition left("LEFT$", "Return leftmost characters of string", "string_left", "string");
    left.addParameter("str", ParameterType::STRING, "Input string")
        .addParameter("count", ParameterType::INT, "Number of characters")
        .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(left));
    
    // RIGHT$ - Right substring
    CommandDefinition right("RIGHT$", "Return rightmost characters of string", "string_right", "string");
    right.addParameter("str", ParameterType::STRING, "Input string")
         .addParameter("count", ParameterType::INT, "Number of characters")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(right));
    
    // MID$ - Middle substring
    CommandDefinition mid("MID$", "Return substring from middle of string", "string_mid", "string");
    mid.addParameter("str", ParameterType::STRING, "Input string")
       .addParameter("start", ParameterType::INT, "Starting position (1-based)")
       .addParameter("length", ParameterType::INT, "Length of substring", true, "nil")
       .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(mid));
    
    // CHR$ - Character from ASCII code
    CommandDefinition chr("CHR$", "Return character from ASCII code", "string.char", "string");
    chr.addParameter("code", ParameterType::INT, "ASCII character code (0-255)")
       .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(chr));
    
    // ASC - ASCII code from character
    CommandDefinition asc("ASC", "Return ASCII code of first character", "string.byte", "string");
    asc.addParameter("str", ParameterType::STRING, "Input string (uses first character)")
       .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(asc));
    
    // STR$ - Convert number to string
    CommandDefinition str("STR$", "Convert number to string", "tostring", "string");
    str.addParameter("num", ParameterType::FLOAT, "Number to convert")
       .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(str));
    
    // VAL - Convert string to number
    CommandDefinition val("VAL", "Convert string to number", "tonumber", "string");
    val.addParameter("str", ParameterType::STRING, "String to convert")
       .setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(val));
    
    // INSTR - Find substring (handled in code generator for Unicode awareness)
    // Not registered here to allow proper unicode.instr vs string.find selection
    
    // JOIN$ - Join string array elements with separator
    CommandDefinition join("JOIN$", "Join string array elements with separator", "string_join", "string");
    join.addParameter("array", ParameterType::STRING, "String array to join")
        .addParameter("separator", ParameterType::STRING, "Separator string")
        .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(join));
    
    // SPLIT$ - Split string into array
    CommandDefinition split("SPLIT$", "Split string into string array", "string_split", "string");
    split.addParameter("str", ParameterType::STRING, "String to split")
         .addParameter("delimiter", ParameterType::STRING, "Delimiter string")
         .setReturnType(ReturnType::STRING);  // Returns array as table
    registry.registerFunction(std::move(split));
    
    // BUFFER$ - Create mutable string buffer for efficient MID$ operations
    CommandDefinition buffer("BUFFER$", "Create mutable string buffer", "create_string_buffer", "string");
    buffer.addParameter("str", ParameterType::STRING, "Initial string content")
          .setReturnType(ReturnType::STRING);  // Returns buffer object
    registry.registerFunction(std::move(buffer));
    
    // TOSTR$ - Convert string buffer back to regular string
    CommandDefinition tostr("TOSTR$", "Convert string buffer to string", "buffer_to_string", "string");
    tostr.addParameter("buffer", ParameterType::STRING, "String buffer to convert")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(tostr));
    
    // INPUT$ - Read fixed number of characters from file
    CommandDefinition inputStr("INPUT$", "Read fixed number of characters from file", "basic_input_string_file", "string");
    inputStr.addParameter("count", ParameterType::INT, "Number of characters to read")
           .addParameter("fileNumber", ParameterType::INT, "File number")
           .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(inputStr));
    
    // =========================================================================
    // BCX-Compatible String Functions
    // =========================================================================
    
    // UCASE$ - Convert to uppercase
    CommandDefinition ucase("UCASE$", "Convert string to uppercase", "string_ucase", "string");
    ucase.addParameter("str", ParameterType::STRING, "Input string")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(ucase));
    
    // LCASE$ - Convert to lowercase
    CommandDefinition lcase("LCASE$", "Convert string to lowercase", "string_lcase", "string");
    lcase.addParameter("str", ParameterType::STRING, "Input string")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(lcase));
    
    // LTRIM$ - Trim left whitespace
    CommandDefinition ltrim("LTRIM$", "Remove leading whitespace", "string_ltrim", "string");
    ltrim.addParameter("str", ParameterType::STRING, "Input string")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(ltrim));
    
    // RTRIM$ - Trim right whitespace
    CommandDefinition rtrim("RTRIM$", "Remove trailing whitespace", "string_rtrim", "string");
    rtrim.addParameter("str", ParameterType::STRING, "Input string")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(rtrim));
    
    // TRIM$ - Trim both sides whitespace
    CommandDefinition trim("TRIM$", "Remove leading and trailing whitespace", "string_trim", "string");
    trim.addParameter("str", ParameterType::STRING, "Input string")
        .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(trim));
    
    // SPACE$ - Create string of spaces
    CommandDefinition space("SPACE$", "Create string of spaces", "string_space", "string");
    space.addParameter("count", ParameterType::INT, "Number of spaces")
         .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(space));
    
    // STRING$ - Repeat character
    CommandDefinition stringFunc("STRING$", "Create string of repeated character", "string_repeat", "string");
    stringFunc.addParameter("count", ParameterType::INT, "Number of repetitions")
              .addParameter("char", ParameterType::STRING, "Character or string to repeat")
              .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(stringFunc));
    
    // REPLACE$ - Replace substring
    CommandDefinition replace("REPLACE$", "Replace all occurrences of substring", "string_replace", "string");
    replace.addParameter("str", ParameterType::STRING, "Input string")
           .addParameter("oldStr", ParameterType::STRING, "String to find")
           .addParameter("newStr", ParameterType::STRING, "Replacement string")
           .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(replace));
    
    // REVERSE$ - Reverse string
    CommandDefinition reverse("REVERSE$", "Reverse string", "string_reverse", "string");
    reverse.addParameter("str", ParameterType::STRING, "Input string")
           .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(reverse));
    
    // TALLY - Count occurrences
    CommandDefinition tally("TALLY", "Count occurrences of substring", "string_tally", "string");
    tally.addParameter("str", ParameterType::STRING, "String to search in")
         .addParameter("pattern", ParameterType::STRING, "Pattern to count")
         .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(tally));
    
    // HEX$ - Convert number to hexadecimal string
    CommandDefinition hex("HEX$", "Convert number to hexadecimal string", "HEX_STRING", "string");
    hex.addParameter("num", ParameterType::INT, "Number to convert")
       .addParameter("digits", ParameterType::INT, "Minimum digits (padding)", true, "0")
       .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(hex));
    
    // BIN$ - Convert number to binary string
    CommandDefinition bin("BIN$", "Convert number to binary string", "BIN_STRING", "string");
    bin.addParameter("num", ParameterType::INT, "Number to convert")
       .addParameter("digits", ParameterType::INT, "Minimum digits (padding)", true, "0")
       .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(bin));
    
    // OCT$ - Convert number to octal string
    CommandDefinition oct("OCT$", "Convert number to octal string", "OCT_STRING", "string");
    oct.addParameter("num", ParameterType::INT, "Number to convert")
       .addParameter("digits", ParameterType::INT, "Minimum digits (padding)", true, "0")
       .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(oct));
    
    // INSERT$ - Insert substring at position
    CommandDefinition insert("INSERT$", "Insert substring at position", "string_insert", "string");
    insert.addParameter("str", ParameterType::STRING, "Original string")
          .addParameter("pos", ParameterType::INT, "Position to insert at (1-based)")
          .addParameter("insertStr", ParameterType::STRING, "String to insert")
          .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(insert));
    
    // DELETE$ - Delete substring
    CommandDefinition deleteStr("DELETE$", "Delete substring from string", "string_delete", "string");
    deleteStr.addParameter("str", ParameterType::STRING, "Original string")
             .addParameter("pos", ParameterType::INT, "Starting position (1-based)")
             .addParameter("length", ParameterType::INT, "Number of characters to delete")
             .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(deleteStr));
    
    // INSTRREV - Find substring from right
    CommandDefinition instrrev("INSTRREV", "Find position of substring from right", "string_instrrev", "string");
    instrrev.addParameter("haystack", ParameterType::STRING, "String to search in")
            .addParameter("needle", ParameterType::STRING, "String to search for")
            .addParameter("start", ParameterType::INT, "Starting position from left", true, "-1")
            .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(instrrev));
    
    // LPAD$ - Left pad string
    CommandDefinition lpad("LPAD$", "Left pad string to width", "string_lpad", "string");
    lpad.addParameter("str", ParameterType::STRING, "Input string")
        .addParameter("width", ParameterType::INT, "Target width")
        .addParameter("padChar", ParameterType::STRING, "Padding character", true, "\" \"")
        .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(lpad));
    
    // RPAD$ - Right pad string
    CommandDefinition rpad("RPAD$", "Right pad string to width", "string_rpad", "string");
    rpad.addParameter("str", ParameterType::STRING, "Input string")
        .addParameter("width", ParameterType::INT, "Target width")
        .addParameter("padChar", ParameterType::STRING, "Padding character", true, "\" \"")
        .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(rpad));
    
    // CENTER$ - Center string
    CommandDefinition center("CENTER$", "Center string in field", "string_center", "string");
    center.addParameter("str", ParameterType::STRING, "Input string")
          .addParameter("width", ParameterType::INT, "Target width")
          .addParameter("padChar", ParameterType::STRING, "Padding character", true, "\" \"")
          .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(center));
    
    // EXTRACT$ - Extract range from string
    CommandDefinition extract("EXTRACT$", "Extract substring by start and end positions", "string_extract", "string");
    extract.addParameter("str", ParameterType::STRING, "Input string")
           .addParameter("startPos", ParameterType::INT, "Start position (1-based)")
           .addParameter("endPos", ParameterType::INT, "End position (1-based)")
           .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(extract));
    
    // REMOVE$ - Remove all occurrences of pattern
    CommandDefinition remove("REMOVE$", "Remove all occurrences of pattern", "string_remove", "string");
    remove.addParameter("str", ParameterType::STRING, "Input string")
          .addParameter("pattern", ParameterType::STRING, "Pattern to remove")
          .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(remove));
    
    // STRREV$ - Reverse string (alias for REVERSE$)
    CommandDefinition strrev("STRREV$", "Reverse string (alias)", "string_reverse", "string");
    strrev.addParameter("str", ParameterType::STRING, "Input string")
          .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(strrev));
}

void CoreCommandRegistry::registerFileIOFunctions(CommandRegistry& registry) {
    // BBC BASIC file opening functions
    CommandDefinition openin("OPENIN", "Open file for input only", "basic_openin", "file");
    openin.addParameter("filename", ParameterType::STRING, "File path to open")
          .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(openin));
    
    CommandDefinition openout("OPENOUT", "Open file for output only", "basic_openout", "file");
    openout.addParameter("filename", ParameterType::STRING, "File path to open")
           .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(openout));
    
    CommandDefinition openup("OPENUP", "Open file for read/write", "basic_openup", "file");
    openup.addParameter("filename", ParameterType::STRING, "File path to open")
          .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(openup));
    
    // BBC BASIC binary file I/O functions
    CommandDefinition bget("BGET", "Read single byte from file", "basic_bget", "file");
    bget.addParameter("fileNumber", ParameterType::INT, "File number")
        .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(bget));
    
    // BBC BASIC file status functions (register without # to avoid type suffix conflicts)
    CommandDefinition eofFunc("EOF", "Test for end of file", "basic_eof_hash", "file");
    eofFunc.addParameter("fileNumber", ParameterType::INT, "File number")
           .setReturnType(ReturnType::BOOL);
    registry.registerFunction(std::move(eofFunc));
    
    CommandDefinition extFunc("EXT", "Get file length", "basic_ext_hash", "file");
    extFunc.addParameter("fileNumber", ParameterType::INT, "File number")
           .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(extFunc));
    
    CommandDefinition ptrFunc("PTR", "Get file pointer position", "basic_ptr_hash", "file");
    ptrFunc.addParameter("fileNumber", ParameterType::INT, "File number")
           .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(ptrFunc));
    
    // BBC BASIC advanced file reading functions
    CommandDefinition getString("GETS", "Read line from file until CR/LF/NUL", "basic_get_string_line", "file");
    getString.addParameter("fileNumber", ParameterType::INT, "File number")
             .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(getString));
    
    CommandDefinition getStringTo("GETSTO", "Read from file until specific character", "basic_get_string_to_char", "file");
    getStringTo.addParameter("fileNumber", ParameterType::INT, "File number")
               .addParameter("terminator", ParameterType::STRING, "Terminator character")
               .setReturnType(ReturnType::STRING);
    registry.registerFunction(std::move(getStringTo));
}

void CoreCommandRegistry::registerFileIOCommands(CommandRegistry& registry) {
    // BPUT - Write single byte to file (statement, not function)
    CommandDefinition bput("BPUT", "Write single byte to file", "basic_bput", "file");
    bput.addParameter("fileNumber", ParameterType::INT, "File number")
        .addParameter("byte", ParameterType::INT, "Byte value (0-255)");
    registry.registerCommand(std::move(bput));
    
    // PTRSET - Set file pointer position (statement)
    CommandDefinition ptrSet("PTRSET", "Set file pointer position", "basic_ptr_set", "file");
    ptrSet.addParameter("fileNumber", ParameterType::INT, "File number")
          .addParameter("position", ParameterType::INT, "Position to seek to");
    registry.registerCommand(std::move(ptrSet));
}

void CoreCommandRegistry::registerSystemFunctions(CommandRegistry& registry) {
    // PEEK - Read memory byte (implementation-dependent)
    CommandDefinition peek("PEEK", "Read byte from memory address", "system_peek", "system");
    peek.addParameter("address", ParameterType::INT, "Memory address")
        .setReturnType(ReturnType::INT);
    registry.registerFunction(std::move(peek));
    
    // GETTICKS - Get elapsed time in milliseconds since program start
    CommandDefinition getticks("GETTICKS", "Get elapsed time in milliseconds since program start", "system_getticks", "system");
    getticks.setReturnType(ReturnType::FLOAT);
    registry.registerFunction(std::move(getticks));
}

// =============================================================================
// Convenience Functions
// =============================================================================

void initializeCoreRegistry(CommandRegistry& registry) {
    registry.clear();
    CoreCommandRegistry::registerCoreCommands(registry);
    CoreCommandRegistry::registerCoreFunctions(registry);
}

// createCoreRegistry removed - use initializeCoreRegistry() directly instead
// (CommandRegistry is no longer copyable/movable due to thread safety mutex)

} // namespace ModularCommands
} // namespace FasterBASIC