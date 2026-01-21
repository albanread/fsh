//
// basic_formatter_lib.cpp
// FasterBASIC - BASIC Code Formatter Library Implementation
//
// Library implementation for formatting BASIC code with renumbering and indentation.
//

#include "basic_formatter_lib.h"
#include "SourceDocument.h"
#include "../shell/REPLView.h"
#include "modular_commands.h"
#include "../runtime/ConstantsManager.h"
#include <sstream>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <cctype>
#include <climits>

namespace FasterBASIC {

// Structure to hold a parsed BASIC line
struct BasicLine {
    int original_line_number;
    int new_line_number;
    std::string content;
    int indent_level;
};

// Check if a token is a block-opening keyword
static bool isBlockOpener(const std::string& token) {
    static const std::set<std::string> openers = {
        "FOR", "WHILE", "REPEAT", "DO", "IF", "THEN",
        "SELECT", "CASE", "DEF", "FUNCTION", "SUB", "VOICES_START",
        "DRAWINTOSPRITE"
    };
    std::string upper = token;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return openers.find(upper) != openers.end();
}

// Check if a token is a block-closing keyword
static bool isBlockCloser(const std::string& token) {
    static const std::set<std::string> closers = {
        "NEXT", "WEND", "UNTIL", "LOOP", "DONE", "END", "ENDIF", "ENDSUB", "ENDFUNCTION", 
        "ENDWHILE", "ENDCASE", "ENDTYPE", "ENDDRAWINTOSPRITE"
    };
    std::string upper = token;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    // Check for VOICES_END* (VOICES_END_PLUCK, VOICES_END_STRING, etc.)
    if (upper.find("VOICES_END") == 0) {
        return true;
    }
    
    return closers.find(upper) != closers.end();
}

// Check if a token is a middle-block keyword
static bool isMiddleBlock(const std::string& token) {
    static const std::set<std::string> middle = {
        "ELSE", "ELSEIF", "ELSIF", "WHEN"
    };
    std::string upper = token;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return middle.find(upper) != middle.end();
}

// Extract line number from start of line
static int extractLineNumber(const std::string& line, size_t& pos) {
    pos = 0;
    
    // Skip leading whitespace
    while (pos < line.length() && std::isspace(line[pos])) {
        pos++;
    }
    
    // Check if line starts with a number
    if (pos >= line.length() || !std::isdigit(line[pos])) {
        return -1;
    }
    
    // Extract the number
    int line_num = 0;
    while (pos < line.length() && std::isdigit(line[pos])) {
        line_num = line_num * 10 + (line[pos] - '0');
        pos++;
    }
    
    // Skip whitespace after line number
    while (pos < line.length() && std::isspace(line[pos])) {
        pos++;
    }
    
    return line_num;
}

// Tokenize a line into words
static std::vector<std::string> tokenizeLine(const std::string& content) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_string = false;
    bool in_comment = false;
    
    for (size_t i = 0; i < content.length(); i++) {
        char c = content[i];
        
        // Handle string literals
        if (c == '"') {
            in_string = !in_string;
            current += c;
            continue;
        }
        
        if (in_string) {
            current += c;
            continue;
        }
        
        // Handle REM comments
        if (!in_comment && current.empty() && i + 3 <= content.length()) {
            std::string check = content.substr(i, 3);
            std::transform(check.begin(), check.end(), check.begin(), ::toupper);
            if (check == "REM") {
                in_comment = true;
                current = "REM";
                i += 2;
                continue;
            }
        }
        
        if (in_comment) {
            current += c;
            continue;
        }
        
        // Handle single-quote comments
        if (c == '\'') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            current = content.substr(i);
            break;
        }
        
        // Delimiters
        if (std::isspace(c) || c == ':' || c == ',' || c == ';' || c == '(' || c == ')') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            if (c == ':' || c == '(' || c == ')' || c == ',' || c == ';') {
                tokens.push_back(std::string(1, c));
            }
            continue;
        }
        
        current += c;
    }
    
    if (!current.empty()) {
        tokens.push_back(current);
    }
    
    return tokens;
}

// Uppercase BASIC keywords in a line while preserving strings and comments
// First lowercases everything, then uppercases only keywords/commands/functions
static std::string uppercaseKeywords(const std::string& content, const FasterBASIC::ModularCommands::CommandRegistry* registry = nullptr, const ConstantsManager* constants = nullptr) {
    static const std::set<std::string> keywords = {
        "FOR", "TO", "STEP", "NEXT", "WHILE", "WEND", "ENDWHILE",
        "REPEAT", "UNTIL", "DO", "LOOP", "IF", "THEN", "ELSE",
        "ELSEIF", "ELSIF", "ENDIF", "END", "GOTO", "GOSUB", "RETURN",
        "DIM", "REDIM", "ERASE", "PRESERVE", "LOCAL", "SHARED", "LET", 
        "PRINT", "CONSOLE", "INPUT", "READ", "DATA", "WRITE",
        "RESTORE", "REM", "AND", "OR", "NOT", "XOR", "MOD", "EQV", "IMP",
        "SUB", "FUNCTION", "ENDSUB", "ENDFUNCTION", "DEF", "FN", "CALL", "EXIT",
        "SELECT", "CASE", "WHEN", "OTHERWISE", "ENDCASE", "OF",
        "OPTION", "BASE", "EXPLICIT", "UNICODE", "ERROR", "BITWISE", "LOGICAL",
        "OPEN", "CLOSE", "AS", "APPEND", "OUTPUT", "RANDOM", "USING",
        "CLS", "LOCATE", "COLOR", "BEEP", "SOUND", "WAIT", "WAIT_MS", "WAIT_FRAME", "WAIT_FRAMES",
        "BYREF", "BYVAL", "IIF", "ON", "ONEVENT", "CONSTANT", "TYPE", "ENDTYPE",
        "INTEGER", "INT", "DOUBLE", "SINGLE", "FLOAT", "STRING", "LONG",
        "SWAP", "INC", "DEC", "PSET", "LINE", "RECT", "CIRCLE", "CIRCLEF", "GCLS", "CLG", "HLINE",
        "AT", "TEXTPUT", "TEXT_PUT", "PRINT_AT", "INPUT_AT", "TCHAR", "TEXT_PUTCHAR", "TGRID", "TSCROLL", "TEXT_SCROLL", "TCLEAR", "TEXT_CLEAR",
        "SPRLOAD", "SPRFREE", "SPRSHOW", "SPRHIDE", "SPRMOVE", "SPRPOS",
        "SPRTINT", "SPRSCALE", "SPRROT", "SPREXPLODE",
        "PLAY", "VSYNC", "AFTER", "EVERY", "AFTERFRAMES", "EVERYFRAME",
        "TIMER", "STOP", "RUN", "MS", "SECS", "FRAMES",
        "INCLUDE", "ONCE", "CANCELLABLE", "IN",
        "VOICES_START", "VOICES_END", "DRAWINTOSPRITE", "ENDDRAWINTOSPRITE"
    };
    
    // First pass: lowercase everything except strings and comments
    std::string lowercased;
    bool in_string = false;
    bool in_comment = false;
    
    for (size_t i = 0; i < content.length(); i++) {
        char c = content[i];
        
        // Handle string literals
        if (c == '"') {
            in_string = !in_string;
            lowercased += c;
            continue;
        }
        
        // Check for REM comment
        if (!in_string && !in_comment && i + 3 <= content.length()) {
            std::string check = content.substr(i, 3);
            std::string upper_check = check;
            std::transform(upper_check.begin(), upper_check.end(), upper_check.begin(), ::toupper);
            if (upper_check == "REM" && (i + 3 >= content.length() || !std::isalnum(content[i+3]))) {
                lowercased += "rem";  // Lowercase REM for now, will be uppercased later
                i += 2;
                in_comment = true;
                continue;
            }
        }
        
        // In comment or string, preserve case
        if (in_comment || in_string) {
            lowercased += c;
        } else {
            // Lowercase everything outside strings and comments
            lowercased += std::tolower(c);
        }
    }
    
    // Second pass: uppercase keywords from the lowercased version
    std::string result;
    std::string word;
    in_string = false;
    in_comment = false;
    
    for (size_t i = 0; i < lowercased.length(); i++) {
        char c = lowercased[i];
        
        // Handle string literals
        if (c == '"') {
            // Flush word before quote
            if (!word.empty()) {
                std::string upper_word = word;
                std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);
                bool isKeyword = keywords.find(upper_word) != keywords.end();
                bool isRegistryCommand = registry && (registry->hasCommand(upper_word) || registry->hasFunction(upper_word));
                bool isConstant = constants && constants->hasConstant(word);
                if (!in_string && !in_comment && (isKeyword || isRegistryCommand) && !isConstant) {
                    result += upper_word;
                } else {
                    result += word;
                }
                word.clear();
            }
            in_string = !in_string;
            result += c;
            continue;
        }
        
        // Check for REM at start (after skipping spaces)
        if (!in_string && !in_comment && word.empty()) {
            std::string check = lowercased.substr(i, std::min(size_t(3), lowercased.length() - i));
            if (check == "rem" && (i + 3 >= lowercased.length() || !std::isalnum(lowercased[i+3]))) {
                result += "REM";
                i += 2;
                in_comment = true;
                continue;
            }
        }
        
        // In comment or string, just copy
        if (in_comment || in_string) {
            result += c;
            continue;
        }
        
        // Build words from alphanumeric + type suffixes
        if (std::isalnum(c) || c == '_') {
            word += c;
        } else if (!word.empty() && (c == '$' || c == '%' || c == '#' || c == '!' || c == '&')) {
            // Type suffix - check if word+suffix is a command/function (e.g., CHR$, MID$)
            std::string upper_word = word;
            std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);
            std::string upper_word_with_suffix = upper_word + c;
            
            bool isKeyword = keywords.find(upper_word) != keywords.end();
            bool isRegistryCommand = registry && (registry->hasCommand(upper_word) || registry->hasFunction(upper_word));
            bool isKeywordWithSuffix = keywords.find(upper_word_with_suffix) != keywords.end();
            bool isRegistryCommandWithSuffix = registry && (registry->hasCommand(upper_word_with_suffix) || registry->hasFunction(upper_word_with_suffix));
            bool isConstant = constants && constants->hasConstant(word);
            
            if ((isKeywordWithSuffix || isRegistryCommandWithSuffix) && !isConstant) {
                // Command/function ends with suffix (e.g., CHR$, MID$, STR$)
                result += upper_word_with_suffix;
            } else if ((isKeyword || isRegistryCommand) && !isConstant) {
                // Keyword without suffix, suffix is part of variable name
                result += upper_word;
                result += c;
            } else {
                // Regular variable with type suffix
                result += word;
                result += c;
            }
            word.clear();
        } else {
            // Word boundary - flush word
            if (!word.empty()) {
                std::string upper_word = word;
                std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);
                bool isKeyword = keywords.find(upper_word) != keywords.end();
                bool isRegistryCommand = registry && (registry->hasCommand(upper_word) || registry->hasFunction(upper_word));
                bool isConstant = constants && constants->hasConstant(word);
                if ((isKeyword || isRegistryCommand) && !isConstant) {
                    result += upper_word;
                } else {
                    result += word;
                }
                word.clear();
            }
            result += c;
        }
    }
    
    // Flush remaining word
    // At end, flush last word
    if (!word.empty()) {
        std::string upper_word = word;
        std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);
        bool isKeyword = keywords.find(upper_word) != keywords.end();
        bool isRegistryCommand = registry && (registry->hasCommand(upper_word) || registry->hasFunction(upper_word));
        bool isConstant = constants && constants->hasConstant(word);
        if (!in_comment && (isKeyword || isRegistryCommand) && !isConstant) {
            result += upper_word;
        } else {
            result += word;
        }
    }
    
    return result;
}

// Calculate indent level change for a line
static void calculateIndent(const std::string& content, int& indent_before, int& indent_after) {
    std::vector<std::string> tokens = tokenizeLine(content);
    
    indent_before = 0;
    indent_after = 0;
    
    for (size_t i = 0; i < tokens.size(); i++) {
        std::string token = tokens[i];
        std::string upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        // Handle EXIT statements - they should be indentation-neutral
        // EXIT FOR, EXIT DO, EXIT WHILE, etc. don't affect indentation
        if (upper == "EXIT" && i + 1 < tokens.size()) {
            std::string next = tokens[i + 1];
            std::transform(next.begin(), next.end(), next.begin(), ::toupper);
            if (next == "FOR" || next == "DO" || next == "WHILE" || 
                next == "REPEAT" || next == "FUNCTION" || next == "SUB") {
                // Skip both EXIT and the following keyword - no indentation change
                i++;
                continue;
            }
        }
        
        // Handle DO WHILE/DO UNTIL as a single construct (not double-indent)
        if (upper == "DO" && i + 1 < tokens.size()) {
            std::string next = tokens[i + 1];
            std::transform(next.begin(), next.end(), next.begin(), ::toupper);
            if (next == "WHILE" || next == "UNTIL") {
                // DO WHILE/DO UNTIL: indent once, skip the WHILE/UNTIL token
                indent_after++;
                i++; // Skip the WHILE/UNTIL token
                continue;
            }
        }
        
        // Handle END followed by IF/SELECT/etc
        if (upper == "END" && i + 1 < tokens.size()) {
            std::string next = tokens[i + 1];
            std::transform(next.begin(), next.end(), next.begin(), ::toupper);
            if (next == "IF" || next == "SELECT" || next == "CASE" || 
                next == "FUNCTION" || next == "SUB") {
                indent_before--;
                i++;
                continue;
            }
        }
        
        // Handle block closers
        if (isBlockCloser(upper)) {
            indent_before--;
            indent_after--;
            continue;
        }
        
        // Handle middle blocks
        if (isMiddleBlock(upper)) {
            indent_before--;
            indent_after++;
            continue;
        }
        
        // Handle IF...THEN blocks
        if (upper == "IF") {
            bool has_then = false;
            bool has_statement_after_then = false;
            
            for (size_t j = i + 1; j < tokens.size(); j++) {
                std::string t = tokens[j];
                std::transform(t.begin(), t.end(), t.begin(), ::toupper);
                if (t == "THEN") {
                    has_then = true;
                    for (size_t k = j + 1; k < tokens.size(); k++) {
                        if (tokens[k] != ":" && !tokens[k].empty()) {
                            std::string stmt = tokens[k];
                            std::transform(stmt.begin(), stmt.end(), stmt.begin(), ::toupper);
                            // If there's any statement after THEN, it's single-line
                            has_statement_after_then = true;
                            break;
                        }
                    }
                    break;
                }
                if (t == ":") break;
            }
            
            // Only indent if it's a multi-line IF (has THEN but no statement after)
            if (has_then && !has_statement_after_then) {
                indent_after++;
            }
            continue;
        }
        
        // Handle other block openers
        if (isBlockOpener(upper) && upper != "IF" && upper != "THEN") {
            // Skip WHILE and REPEAT if they're standalone (not after DO)
            // - Standalone WHILE...WEND is handled here
            // - DO WHILE was already handled above
            // - REPEAT...UNTIL is handled here
            // - DO indents in both contexts (DO...LOOP and AFTER/EVERY...DO...DONE)
            indent_after++;
            continue;
        }
    }
}

// Parse program into lines
static std::vector<BasicLine> parseProgram(const std::string& source, bool add_indentation) {
    std::vector<BasicLine> lines;
    std::istringstream iss(source);
    std::string line;
    int current_indent = 0;
    int auto_line_num = 1000;  // Auto-number starting at 1000 for unnumbered lines
    
    while (std::getline(iss, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        
        size_t pos = 0;
        int line_num = extractLineNumber(line, pos);
        
        std::string content;
        
        if (line_num < 0) {
            // No line number found - auto-number this line
            line_num = auto_line_num;
            auto_line_num += 10;  // Step by 10
            content = line;  // Use entire line as content
        } else {
            // Line has a number - extract content after the number
            content = line.substr(pos);
        }
        
        int indent_before = 0;
        int indent_after = 0;
        
        if (add_indentation) {
            calculateIndent(content, indent_before, indent_after);
            current_indent += indent_before;
            if (current_indent < 0) current_indent = 0;
        }
        
        BasicLine bl;
        bl.original_line_number = line_num;
        bl.new_line_number = 0;
        bl.content = content;
        bl.indent_level = current_indent;
        
        lines.push_back(bl);
        
        if (add_indentation) {
            current_indent += indent_after;
            if (current_indent < 0) current_indent = 0;
        }
    }
    
    return lines;
}

// Build line number mapping
static std::map<int, int> buildLineMapping(std::vector<BasicLine>& lines, int start_line, int step) {
    std::map<int, int> mapping;
    int new_num = start_line;
    
    for (auto& line : lines) {
        mapping[line.original_line_number] = new_num;
        line.new_line_number = new_num;
        new_num += step;
    }
    
    return mapping;
}

// Replace line number references
static std::string replaceLineRefs(const std::string& content, const std::map<int, int>& mapping) {
    std::vector<std::string> tokens = tokenizeLine(content);
    std::ostringstream result;
    
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0 && !tokens[i-1].empty() && tokens[i-1].back() != ':' && 
            !result.str().empty() && result.str().back() != ' ' &&
            result.str().back() != '(' && tokens[i] != ":" && tokens[i] != "," && 
            tokens[i] != ")" && tokens[i] != "(" && tokens[i-1] != "(") {
            result << " ";
        }
        
        std::string token = tokens[i];
        std::string upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if ((upper == "GOTO" || upper == "GOSUB" || upper == "RESTORE" ||
             upper == "THEN" || upper == "ELSE") && i + 1 < tokens.size()) {
            
            result << token;
            i++;
            while (i < tokens.size() && (tokens[i] == ":" || tokens[i] == " ")) {
                result << tokens[i];
                i++;
            }
            
            if (i < tokens.size()) {
                // Check if next token is GOTO/GOSUB (for THEN GOTO / ELSE GOTO patterns)
                std::string nextUpper = tokens[i];
                std::transform(nextUpper.begin(), nextUpper.end(), nextUpper.begin(), ::toupper);
                
                if ((upper == "THEN" || upper == "ELSE") && 
                    (nextUpper == "GOTO" || nextUpper == "GOSUB")) {
                    // Don't consume GOTO/GOSUB - let it be processed in next iteration
                    i--;
                } else {
                    bool is_number = true;
                    for (char c : tokens[i]) {
                        if (!std::isdigit(c)) {
                            is_number = false;
                            break;
                        }
                    }
                    
                    if (is_number) {
                        int old_line = std::stoi(tokens[i]);
                        auto it = mapping.find(old_line);
                        if (it != mapping.end()) {
                            result << " " << it->second;
                        } else {
                            result << " " << tokens[i];
                        }
                    } else {
                        result << " " << tokens[i];
                    }
                }
            }
        } else if (upper == "ON" && i + 1 < tokens.size()) {
            result << token << " ";
            i++;
            
            while (i < tokens.size()) {
                std::string t = tokens[i];
                std::string tu = t;
                std::transform(tu.begin(), tu.end(), tu.begin(), ::toupper);
                
                if (tu == "GOTO" || tu == "GOSUB") {
                    result << t;
                    i++;
                    
                    while (i < tokens.size()) {
                        if (tokens[i] == "," || tokens[i] == " ") {
                            result << tokens[i];
                            i++;
                            continue;
                        }
                        
                        bool is_number = true;
                        for (char c : tokens[i]) {
                            if (!std::isdigit(c)) {
                                is_number = false;
                                break;
                            }
                        }
                        
                        if (is_number) {
                            int old_line = std::stoi(tokens[i]);
                            auto it = mapping.find(old_line);
                            if (it != mapping.end()) {
                                result << " " << it->second;
                            } else {
                                result << " " << tokens[i];
                            }
                        } else {
                            result << " " << tokens[i];
                            break;
                        }
                        i++;
                    }
                    break;
                } else {
                    result << t << " ";
                    i++;
                }
            }
            i--;
        } else {
            result << token;
        }
    }
    
    return result.str();
}

// Format program
static std::string formatProgram(std::vector<BasicLine>& lines, 
                                const std::map<int, int>& mapping,
                                const FormatterOptions& options,
                                const FasterBASIC::ModularCommands::CommandRegistry* registry,
                                const ConstantsManager* constants) {
    std::ostringstream result;
    
    for (const auto& line : lines) {
        result << line.new_line_number << " ";
        
        if (options.add_indentation) {
            for (int i = 0; i < line.indent_level; i++) {
                for (int j = 0; j < options.indent_spaces; j++) {
                    result << " ";
                }
            }
        }
        
        std::string formatted_content;
        if (options.update_references) {
            formatted_content = replaceLineRefs(line.content, mapping);
        } else {
            formatted_content = line.content;
        }
        
        // Uppercase BASIC keywords and registry commands (but not constants)
        formatted_content = uppercaseKeywords(formatted_content, registry, constants);
        
        result << formatted_content << "\n";
    }
    
    return result.str();
}

// =============================================================================
// Public API Implementation
// =============================================================================

FormatterResult formatBasicCode(const std::string& source_code, const FormatterOptions& options, const FasterBASIC::ModularCommands::CommandRegistry* registry, const ConstantsManager* constants) {
    FormatterResult result;
    
    try {
        // Parse program
        std::vector<BasicLine> lines = parseProgram(source_code, options.add_indentation);
        
        if (lines.empty()) {
            result.success = false;
            result.error_message = "No valid BASIC lines found in source code";
            return result;
        }
        
        // Build line mapping
        std::map<int, int> mapping;
        if (options.start_line > 0) {
            mapping = buildLineMapping(lines, options.start_line, options.step);
        } else {
            // Keep original line numbers
            for (auto& line : lines) {
                mapping[line.original_line_number] = line.original_line_number;
                line.new_line_number = line.original_line_number;
            }
        }
        
        // Format program
        std::string formatted = formatProgram(lines, mapping, options, registry, constants);
        
        result.success = true;
        result.formatted_code = formatted;
        result.lines_processed = static_cast<int>(lines.size());
        result.line_number_map = mapping;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Formatting error: ") + e.what();
    }
    
    return result;
}

bool formatBasicCodeInPlace(std::string& source_code, const FormatterOptions& options, const FasterBASIC::ModularCommands::CommandRegistry* registry, const ConstantsManager* constants) {
    FormatterResult result = formatBasicCode(source_code, options, registry, constants);
    if (result.success) {
        source_code = result.formatted_code;
        return true;
    }
    return false;
}

FormatterResult formatClassic(const std::string& source_code) {
    return formatBasicCode(source_code, FormatterOptions::Classic());
}

FormatterResult formatModern(const std::string& source_code) {
    return formatBasicCode(source_code, FormatterOptions::Modern());
}

FormatterResult formatTight(const std::string& source_code) {
    return formatBasicCode(source_code, FormatterOptions::Tight());
}

FormatterResult renumberBasicCode(const std::string& source_code, int start_line, int step) {
    FormatterOptions opts = FormatterOptions::RenumberOnly();
    opts.start_line = start_line;
    opts.step = step;
    return formatBasicCode(source_code, opts);
}

FormatterResult removeLineNumbers(const std::string& source_code) {
    FormatterResult result;
    result.success = true;
    result.lines_processed = 0;
    
    std::istringstream input(source_code);
    std::ostringstream output;
    std::string line;
    
    while (std::getline(input, line)) {
        size_t pos = 0;
        int line_num = extractLineNumber(line, pos);
        
        if (line_num > 0) {
            // Line has a line number - strip it and output the rest
            std::string content = line.substr(pos);
            output << content;
            result.lines_processed++;
        } else {
            // Line has no line number - output as is
            output << line;
        }
        
        // Add newline (except for last line if input didn't have one)
        if (!input.eof() || line.empty()) {
            output << "\n";
        }
    }
    
    result.formatted_code = output.str();
    
    // Remove trailing newline if original didn't have one
    if (!source_code.empty() && source_code.back() != '\n' && 
        !result.formatted_code.empty() && result.formatted_code.back() == '\n') {
        result.formatted_code.pop_back();
    }
    
    return result;
}

FormatterResult indentBasicCode(const std::string& source_code) {
    return formatBasicCode(source_code, FormatterOptions::IndentOnly());
}

// =============================================================================
// SourceDocument/REPLView Convenience Functions
// =============================================================================

bool formatDocument(SourceDocument& document, const FormatterOptions& options) {
    // Generate source code from document
    std::string source = document.generateSourceForCompiler();
    
    // Format the code
    FormatterResult result = formatBasicCode(source, options);
    
    if (!result.success) {
        return false;
    }
    
    // Parse formatted code back into document
    document.clear();
    document.setText(result.formatted_code);
    
    return true;
}

bool formatREPLView(REPLView& view, const FormatterOptions& options) {
    // Generate source code from view
    std::string source = view.generateSource();
    
    // Format the code
    FormatterResult result = formatBasicCode(source, options);
    
    if (!result.success) {
        return false;
    }
    
    // Parse formatted code back into view
    view.clear();
    
    std::istringstream stream(result.formatted_code);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Extract line number and code
        size_t pos = 0;
        while (pos < line.length() && std::isspace(line[pos])) pos++;
        
        if (pos < line.length() && std::isdigit(line[pos])) {
            size_t endPos = pos;
            while (endPos < line.length() && std::isdigit(line[endPos])) endPos++;
            
            int lineNum = std::stoi(line.substr(pos, endPos - pos));
            
            // Skip whitespace after line number
            pos = endPos;
            while (pos < line.length() && std::isspace(line[pos])) pos++;
            
            std::string code = line.substr(pos);
            if (!code.empty()) {
                view.setLine(lineNum, code);
            }
        }
    }
    
    return true;
}

bool renumberDocument(SourceDocument& document, int start_line, int step) {
    FormatterOptions options = FormatterOptions::RenumberOnly();
    options.start_line = start_line;
    options.step = step;
    options.update_references = true;
    
    return formatDocument(document, options);
}

bool renumberREPLView(REPLView& view, int start_line, int step) {
    FormatterOptions options = FormatterOptions::RenumberOnly();
    options.start_line = start_line;
    options.step = step;
    options.update_references = true;
    
    return formatREPLView(view, options);
}

bool hasValidLineNumbers(const std::string& source_code) {
    std::istringstream iss(source_code);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        
        size_t pos = 0;
        int line_num = extractLineNumber(line, pos);
        if (line_num >= 0) {
            return true;
        }
    }
    
    return false;
}

int countNumberedLines(const std::string& source_code) {
    std::istringstream iss(source_code);
    std::string line;
    int count = 0;
    
    while (std::getline(iss, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        
        size_t pos = 0;
        int line_num = extractLineNumber(line, pos);
        if (line_num >= 0) {
            count++;
        }
    }
    
    return count;
}

bool detectLineNumberRange(const std::string& source_code, int& out_min, int& out_max) {
    std::istringstream iss(source_code);
    std::string line;
    bool found_any = false;
    int min_line = INT_MAX;
    int max_line = INT_MIN;
    
    while (std::getline(iss, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }
        
        size_t pos = 0;
        int line_num = extractLineNumber(line, pos);
        if (line_num >= 0) {
            found_any = true;
            if (line_num < min_line) min_line = line_num;
            if (line_num > max_line) max_line = line_num;
        }
    }
    
    if (found_any) {
        out_min = min_line;
        out_max = max_line;
    }
    
    return found_any;
}

} // namespace FasterBASIC