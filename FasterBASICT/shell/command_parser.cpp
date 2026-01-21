//
// command_parser.cpp
// FasterBASIC Shell - Command Parser
//
// Parses shell commands and identifies different types of user input.
// Handles BASIC line entry, shell commands, and immediate mode expressions.
//

#include "command_parser.h"
#include "../src/modular_commands.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>

namespace FasterBASIC {

// Static command mappings
const std::vector<std::pair<std::string, ShellCommandType>> CommandParser::s_commands = {
    {"LIST", ShellCommandType::LIST},
    {"RUN", ShellCommandType::RUN},
    {"LOAD", ShellCommandType::LOAD},
    {"SAVE", ShellCommandType::SAVE},
    {"MERGE", ShellCommandType::MERGE},
    {"NEW", ShellCommandType::NEW},
    {"AUTO", ShellCommandType::AUTO},
    {"RENUM", ShellCommandType::RENUM},
    {"EDIT", ShellCommandType::EDIT},
    {"EDITOR", ShellCommandType::EDITOR},
    {"DEL", ShellCommandType::DEL},
    {"DELETE", ShellCommandType::DEL},
    {"VARS", ShellCommandType::VARS},
    {"CLEAR", ShellCommandType::CLEAR},
    {"CHECK", ShellCommandType::CHECK},
    {"FORMAT", ShellCommandType::FORMAT},
    {"CLS", ShellCommandType::CLS},
    {"DIR", ShellCommandType::DIR},
    {"FIND", ShellCommandType::FIND},
    {"FINDNEXT", ShellCommandType::FINDNEXT},
    {"REPLACE", ShellCommandType::REPLACE},
    {"REPLACENEXT", ShellCommandType::REPLACENEXT},
    {"HELP", ShellCommandType::HELP},
    {"QUIT", ShellCommandType::QUIT},
    {"EXIT", ShellCommandType::QUIT},
    {"BYE", ShellCommandType::QUIT},
    {"CONT", ShellCommandType::CONT},
    {"STOP", ShellCommandType::STOP}
};

std::vector<std::string> CommandParser::s_basicKeywords = {
    "FOR", "TO", "STEP", "NEXT", "WHILE", "WEND", "ENDWHILE",
    "REPEAT", "UNTIL", "DO", "LOOP", "DONE", "IF", "THEN", "ELSE",
    "ELSEIF", "ELSIF", "END", "GOTO", "GOSUB", "RETURN",
    "DIM", "LOCAL", "LET", "PRINT", "INPUT", "READ", "DATA",
    "RESTORE", "REM", "AND", "OR", "NOT", "XOR", "MOD",
    "SUB", "FUNCTION", "DEF", "FN", "CALL", "EXIT",
    "SELECT", "CASE", "WHEN", "OTHERWISE", "ENDCASE",
    "OPTION", "BASE", "EXPLICIT", "UNICODE", "ERROR",
    "OPEN", "CLOSE", "AS", "APPEND", "OUTPUT", "RANDOM",
    "CLS", "LOCATE", "COLOR", "BEEP", "SOUND", "WAIT", "WAIT_MS", "CONST",
    "AFTER", "EVERY", "AFTERFRAMES", "EVERYFRAME", "MS", "SECS", "FRAMES", "RUN",
    // String functions with $ suffix
    "CHR$", "STR$", "LEFT$", "RIGHT$", "MID$", "UCASE$", "LCASE$",
    "INKEY$", "INPUT$", "SPACE$", "STRING$", "HEX$", "BIN$", "OCT$"
};

bool CommandParser::s_keywordsSynced = false;

CommandParser::CommandParser() {
    // Sync keywords from registry on first instantiation
    if (!s_keywordsSynced) {
        syncKeywordsFromRegistry();
    }
}

CommandParser::~CommandParser() {
}

ParsedCommand CommandParser::parse(const std::string& input) {
    clearError();
    
    std::string trimmedInput = trim(input);
    
    // Empty input
    if (trimmedInput.empty()) {
        return ParsedCommand();
    }
    
    // Expand command abbreviations before parsing
    trimmedInput = expandCommandAbbreviations(trimmedInput);
    
    // Check if it starts with a number (potential line number)
    if (std::isdigit(trimmedInput[0])) {
        return parseDirectLine(trimmedInput);
    }
    
    // Tokenize the input
    std::vector<std::string> tokens = tokenize(trimmedInput);
    if (tokens.empty()) {
        return ParsedCommand();
    }
    
    std::string firstToken = toUpper(tokens[0]);
    ShellCommandType cmdType = getCommandType(firstToken);
    
    switch (cmdType) {
        case ShellCommandType::LIST:
            return parseListCommand(tokens);
            
        case ShellCommandType::RUN:
            return parseRunCommand(tokens);
            
        case ShellCommandType::LOAD:
            return parseFileCommand(tokens, ShellCommandType::LOAD);
            
        case ShellCommandType::SAVE:
            return parseFileCommand(tokens, ShellCommandType::SAVE);
            
        case ShellCommandType::MERGE:
            return parseFileCommand(tokens, ShellCommandType::MERGE);
            
        case ShellCommandType::AUTO:
            return parseAutoCommand(tokens);
            
        case ShellCommandType::RENUM:
            return parseRenumCommand(tokens);
            
        case ShellCommandType::DEL:
            return parseDelCommand(tokens);
            
        case ShellCommandType::EDIT:
            return parseEditCommand(tokens);
            
        case ShellCommandType::FIND:
            return parseFindCommand(tokens);
            
        case ShellCommandType::FINDNEXT:
            return parseSimpleCommand(ShellCommandType::FINDNEXT);
            
        case ShellCommandType::REPLACE:
            return parseReplaceCommand(tokens);
            
        case ShellCommandType::REPLACENEXT:
            return parseReplaceCommand(tokens);
            
        case ShellCommandType::NEW:
        case ShellCommandType::VARS:
        case ShellCommandType::CLEAR:
        case ShellCommandType::CHECK:
        case ShellCommandType::FORMAT:
        case ShellCommandType::CLS:
        case ShellCommandType::DIR:
        case ShellCommandType::QUIT:
        case ShellCommandType::CONT:
        case ShellCommandType::STOP:
        case ShellCommandType::EDITOR:
            return parseSimpleCommand(cmdType);
        
        case ShellCommandType::HELP:
            return parseHelpCommand(tokens);
            
        default:
            // Check if it's a BASIC statement for immediate execution
            if (isBASICKeyword(firstToken)) {
                return parseImmediate(trimmedInput);
            }
            
            // Check if it's a variable assignment or expression
            if (trimmedInput.find('=') != std::string::npos || 
                trimmedInput.find('(') != std::string::npos) {
                return parseImmediate(trimmedInput);
            }
            
            // Invalid command
            setError("Invalid command: " + firstToken);
            ParsedCommand cmd;
            cmd.type = ShellCommandType::INVALID;
            return cmd;
    }
}

ParsedCommand CommandParser::parseDirectLine(const std::string& input) {
    ParsedCommand cmd;
    
    // Find the first space to separate line number from code
    size_t spacePos = input.find(' ');
    
    if (spacePos == std::string::npos) {
        // Just a line number - this means delete the line
        if (isNumber(input)) {
            cmd.type = ShellCommandType::DELETE_LINE;
            cmd.lineNumber = parseNumber(input);
            cmd.hasLineNumber = true;
        } else {
            setError("Invalid line number: " + input);
            cmd.type = ShellCommandType::INVALID;
        }
        return cmd;
    }
    
    // Extract line number and code
    std::string lineNumStr = input.substr(0, spacePos);
    std::string code = trim(input.substr(spacePos + 1));
    
    if (!isNumber(lineNumStr)) {
        setError("Invalid line number: " + lineNumStr);
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    int lineNum = parseNumber(lineNumStr);
    if (!isValidLineNumber(lineNum)) {
        setError("Line number out of range: " + lineNumStr);
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    cmd.type = ShellCommandType::DIRECT_LINE;
    cmd.lineNumber = lineNum;
    cmd.code = code;
    cmd.hasLineNumber = true;
    
    return cmd;
}

ParsedCommand CommandParser::parseListCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::LIST;
    
    if (tokens.size() == 1) {
        // Simple LIST command
        return cmd;
    }
    
    if (tokens.size() == 2) {
        std::string param = tokens[1];
        
        // Check for range formats
        if (param.find('-') != std::string::npos) {
            int start, end;
            if (parseRange(param, start, end)) {
                if (start == -1) {
                    // Format: -50 (from beginning to line 50)
                    cmd.type = ShellCommandType::LIST_TO;
                    cmd.endLine = end;
                    cmd.hasRange = true;
                } else if (end == -1) {
                    // Format: 50- (from line 50 to end)
                    cmd.type = ShellCommandType::LIST_FROM;
                    cmd.startLine = start;
                    cmd.hasRange = true;
                } else {
                    // Format: 10-50 (range)
                    cmd.type = ShellCommandType::LIST_RANGE;
                    cmd.startLine = start;
                    cmd.endLine = end;
                    cmd.hasRange = true;
                }
            } else {
                setError("Invalid range format: " + param);
                cmd.type = ShellCommandType::INVALID;
            }
        } else if (isNumber(param)) {
            // Single line number
            cmd.type = ShellCommandType::LIST_LINE;
            cmd.lineNumber = parseNumber(param);
            cmd.hasLineNumber = true;
        } else {
            setError("Invalid LIST parameter: " + param);
            cmd.type = ShellCommandType::INVALID;
        }
    } else {
        setError("Too many parameters for LIST command");
        cmd.type = ShellCommandType::INVALID;
    }
    
    return cmd;
}

ParsedCommand CommandParser::parseDelCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::DEL;
    
    if (tokens.size() == 1) {
        setError("DEL requires a line number or range");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    if (tokens.size() == 2) {
        std::string param = tokens[1];
        
        // Check for range format: 10-50
        if (param.find('-') != std::string::npos) {
            int start, end;
            if (parseRange(param, start, end)) {
                if (start == -1 || end == -1) {
                    setError("DEL range must specify both start and end: DEL 10-50");
                    cmd.type = ShellCommandType::INVALID;
                } else {
                    // Valid range
                    cmd.type = ShellCommandType::DEL_RANGE;
                    cmd.startLine = start;
                    cmd.endLine = end;
                    cmd.hasRange = true;
                }
            } else {
                setError("Invalid range format: " + param);
                cmd.type = ShellCommandType::INVALID;
            }
        } else if (isNumber(param)) {
            // Single line number
            cmd.type = ShellCommandType::DEL;
            cmd.lineNumber = parseNumber(param);
            cmd.hasLineNumber = true;
        } else {
            setError("Invalid DEL parameter: " + param);
            cmd.type = ShellCommandType::INVALID;
        }
    } else {
        setError("Too many parameters for DEL command");
        cmd.type = ShellCommandType::INVALID;
    }
    
    return cmd;
}

ParsedCommand CommandParser::parseRunCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::RUN;
    
    if (tokens.size() == 1) {
        // Simple RUN command
        return cmd;
    }
    
    if (tokens.size() == 2) {
        if (isNumber(tokens[1])) {
            cmd.type = ShellCommandType::RUN_FROM;
            cmd.lineNumber = parseNumber(tokens[1]);
            cmd.hasLineNumber = true;
        } else {
            setError("Invalid line number for RUN: " + tokens[1]);
            cmd.type = ShellCommandType::INVALID;
        }
    } else {
        setError("Too many parameters for RUN command");
        cmd.type = ShellCommandType::INVALID;
    }
    
    return cmd;
}

ParsedCommand CommandParser::parseFileCommand(const std::vector<std::string>& tokens, ShellCommandType type) {
    ParsedCommand cmd;
    cmd.type = type;
    
    // SAVE can be called with no filename (will use current filename)
    if (tokens.size() == 1 && type == ShellCommandType::SAVE) {
        cmd.hasFilename = false;
        return cmd;
    }
    
    if (tokens.size() != 2) {
        std::string cmdName;
        switch (type) {
            case ShellCommandType::LOAD: cmdName = "LOAD"; break;
            case ShellCommandType::SAVE: cmdName = "SAVE"; break;
            case ShellCommandType::MERGE: cmdName = "MERGE"; break;
            default: cmdName = "FILE"; break;
        }
        setError(cmdName + " requires a filename");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    std::string filename = extractQuotedString(tokens[1]);
    if (filename.empty()) {
        setError("Invalid filename format");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    cmd.filename = filename;
    cmd.hasFilename = true;
    
    return cmd;
}

ParsedCommand CommandParser::parseAutoCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::AUTO;
    cmd.startLine = 10;  // Default start
    cmd.step = 10;       // Default step
    
    if (tokens.size() == 1) {
        // Simple AUTO command with defaults
        return cmd;
    }
    
    if (tokens.size() == 2) {
        // AUTO start,step format
        std::string params = tokens[1];
        size_t commaPos = params.find(',');
        
        if (commaPos != std::string::npos) {
            std::string startStr = trim(params.substr(0, commaPos));
            std::string stepStr = trim(params.substr(commaPos + 1));
            
            if (isNumber(startStr) && isNumber(stepStr)) {
                cmd.type = ShellCommandType::AUTO_PARAMS;
                cmd.startLine = parseNumber(startStr);
                cmd.step = parseNumber(stepStr);
                cmd.hasStep = true;
            } else {
                setError("Invalid AUTO parameters");
                cmd.type = ShellCommandType::INVALID;
            }
        } else if (isNumber(params)) {
            // Just start line
            cmd.type = ShellCommandType::AUTO_PARAMS;
            cmd.startLine = parseNumber(params);
        } else {
            setError("Invalid AUTO parameter: " + params);
            cmd.type = ShellCommandType::INVALID;
        }
    } else {
        setError("Too many parameters for AUTO command");
        cmd.type = ShellCommandType::INVALID;
    }
    
    return cmd;
}

ParsedCommand CommandParser::parseRenumCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::RENUM;
    cmd.startLine = 10;  // Default start
    cmd.step = 10;       // Default step
    
    if (tokens.size() == 1) {
        // Simple RENUM command with defaults
        return cmd;
    }
    
    if (tokens.size() == 2) {
        // RENUM start,step format
        std::string params = tokens[1];
        size_t commaPos = params.find(',');
        
        if (commaPos != std::string::npos) {
            std::string startStr = trim(params.substr(0, commaPos));
            std::string stepStr = trim(params.substr(commaPos + 1));
            
            if (isNumber(startStr) && isNumber(stepStr)) {
                cmd.type = ShellCommandType::RENUM_PARAMS;
                cmd.startLine = parseNumber(startStr);
                cmd.step = parseNumber(stepStr);
                cmd.hasStep = true;
            } else {
                setError("Invalid RENUM parameters");
                cmd.type = ShellCommandType::INVALID;
            }
        } else if (isNumber(params)) {
            // Just start line
            cmd.type = ShellCommandType::RENUM_PARAMS;
            cmd.startLine = parseNumber(params);
        } else {
            setError("Invalid RENUM parameter: " + params);
            cmd.type = ShellCommandType::INVALID;
        }
    } else {
        setError("Too many parameters for RENUM command");
        cmd.type = ShellCommandType::INVALID;
    }
    
    return cmd;
}

ParsedCommand CommandParser::parseEditCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::EDIT;
    
    if (tokens.size() != 2) {
        setError("EDIT command requires a line number (e.g., EDIT 100)");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    std::string lineStr = tokens[1];
    if (!isNumber(lineStr)) {
        setError("Invalid line number for EDIT command: " + lineStr);
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    int lineNumber = parseNumber(lineStr);
    if (!isValidLineNumber(lineNumber)) {
        setError("Line number out of range: " + lineStr);
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    cmd.lineNumber = lineNumber;
    cmd.hasLineNumber = true;
    return cmd;
}

ParsedCommand CommandParser::parseSimpleCommand(ShellCommandType type) {
    ParsedCommand cmd;
    cmd.type = type;
    return cmd;
}

ParsedCommand CommandParser::parseHelpCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::HELP;
    
    // HELP with no arguments - show general help
    if (tokens.size() == 1) {
        cmd.searchText = "";  // Use searchText field for topic/command name
        return cmd;
    }
    
    // Check for subcommands (REBUILD, SEARCH, TAG, ARTICLES, ARTICLE)
    std::string firstArg = toUpper(tokens[1]);
    
    if (firstArg == "REBUILD") {
        cmd.helpSubcommand = "REBUILD";
        return cmd;
    }
    
    if (firstArg == "SEARCH") {
        cmd.helpSubcommand = "SEARCH";
        // Join remaining tokens as search query
        if (tokens.size() > 2) {
            for (size_t i = 2; i < tokens.size(); ++i) {
                if (i > 2) cmd.searchText += " ";
                cmd.searchText += tokens[i];
            }
        }
        return cmd;
    }
    
    if (firstArg == "TAG") {
        cmd.helpSubcommand = "TAG";
        // Get tag name (if provided)
        if (tokens.size() > 2) {
            cmd.searchText = tokens[2];
        }
        return cmd;
    }
    
    if (firstArg == "ARTICLES") {
        cmd.helpSubcommand = "ARTICLES";
        return cmd;
    }
    
    if (firstArg == "ARTICLE") {
        cmd.helpSubcommand = "ARTICLE";
        // Get article name
        if (tokens.size() > 2) {
            for (size_t i = 2; i < tokens.size(); ++i) {
                if (i > 2) cmd.searchText += " ";
                cmd.searchText += tokens[i];
            }
        }
        return cmd;
    }
    
    // HELP <topic> or HELP <command>
    // Join remaining tokens as the topic/command name (handles multi-word if needed)
    std::string topic;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (i > 1) topic += " ";
        topic += toUpper(tokens[i]);
    }
    
    cmd.searchText = topic;
    return cmd;
}

ParsedCommand CommandParser::parseImmediate(const std::string& input) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::IMMEDIATE;
    cmd.expression = input;
    return cmd;
}

ParsedCommand CommandParser::parseFindCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = ShellCommandType::FIND;
    cmd.contextLines = 3;  // Default context lines
    
    if (tokens.size() < 2) {
        setError("FIND command requires search text (e.g., FIND \"text\" or FIND text)");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    // Parse search text - can be quoted or unquoted
    std::string searchText;
    size_t contextLines = 3;
    
    if (tokens.size() == 2) {
        // FIND "text" or FIND text
        searchText = tokens[1];
        // Remove quotes if present
        if (searchText.length() >= 2 && 
            searchText.front() == '"' && searchText.back() == '"') {
            searchText = searchText.substr(1, searchText.length() - 2);
        }
    } else if (tokens.size() == 3) {
        // FIND "text",5 or FIND text 5
        searchText = tokens[1];
        // Remove quotes if present
        if (searchText.length() >= 2 && 
            searchText.front() == '"' && searchText.back() == '"') {
            searchText = searchText.substr(1, searchText.length() - 2);
        }
        
        // Parse context lines
        std::string contextStr = tokens[2];
        // Remove comma if present
        if (!contextStr.empty() && contextStr.front() == ',') {
            contextStr = contextStr.substr(1);
        }
        
        if (isNumber(contextStr)) {
            contextLines = parseNumber(contextStr);
            if (contextLines > 20) {
                contextLines = 20;  // Reasonable limit
            }
        } else {
            setError("Invalid context line count: " + contextStr);
            cmd.type = ShellCommandType::INVALID;
            return cmd;
        }
    } else {
        setError("Too many parameters for FIND command");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    if (searchText.empty()) {
        setError("Search text cannot be empty");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    cmd.searchText = searchText;
    cmd.contextLines = static_cast<int>(contextLines);
    return cmd;
}

ParsedCommand CommandParser::parseReplaceCommand(const std::vector<std::string>& tokens) {
    ParsedCommand cmd;
    cmd.type = (tokens[0] == "REPLACE") ? ShellCommandType::REPLACE : ShellCommandType::REPLACENEXT;
    
    if (tokens.size() != 2) {
        std::string cmdName = (cmd.type == ShellCommandType::REPLACE) ? "REPLACE" : "REPLACENEXT";
        setError(cmdName + " command requires replacement text (e.g., " + cmdName + " \"new text\")");
        cmd.type = ShellCommandType::INVALID;
        return cmd;
    }
    
    // Parse replacement text - can be quoted or unquoted
    std::string replaceText = tokens[1];
    // Remove quotes if present
    if (replaceText.length() >= 2 && 
        replaceText.front() == '"' && replaceText.back() == '"') {
        replaceText = replaceText.substr(1, replaceText.length() - 2);
    }
    
    cmd.replaceText = replaceText;
    return cmd;
}

// Utility functions

std::vector<std::string> CommandParser::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string CommandParser::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string CommandParser::toUpper(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

bool CommandParser::isNumber(const std::string& str) const {
    if (str.empty()) return false;
    
    size_t start = 0;
    if (str[0] == '-' || str[0] == '+') {
        start = 1;
    }
    
    for (size_t i = start; i < str.length(); ++i) {
        if (!std::isdigit(str[i])) {
            return false;
        }
    }
    
    return true;
}

int CommandParser::parseNumber(const std::string& str) const {
    try {
        return std::stoi(str);
    } catch (...) {
        return 0;
    }
}

bool CommandParser::parseRange(const std::string& rangeStr, int& start, int& end) const {
    size_t dashPos = rangeStr.find('-');
    if (dashPos == std::string::npos) {
        return false;
    }
    
    std::string startStr = rangeStr.substr(0, dashPos);
    std::string endStr = rangeStr.substr(dashPos + 1);
    
    // Handle formats like "-50" (to line 50) and "50-" (from line 50)
    if (startStr.empty()) {
        start = -1;  // Indicates "from beginning"
    } else if (isNumber(startStr)) {
        start = parseNumber(startStr);
    } else {
        return false;
    }
    
    if (endStr.empty()) {
        end = -1;  // Indicates "to end"
    } else if (isNumber(endStr)) {
        end = parseNumber(endStr);
    } else {
        return false;
    }
    
    return true;
}

std::string CommandParser::extractQuotedString(const std::string& str) const {
    std::string trimmed = trim(str);
    
    // Remove surrounding quotes if present
    if (trimmed.length() >= 2) {
        if ((trimmed[0] == '"' && trimmed.back() == '"') ||
            (trimmed[0] == '\'' && trimmed.back() == '\'')) {
            return trimmed.substr(1, trimmed.length() - 2);
        }
    }
    
    return trimmed;
}

ShellCommandType CommandParser::getCommandType(const std::string& command) const {
    // Check for exact match first
    for (const auto& pair : s_commands) {
        if (pair.first == command) {
            return pair.second;
        }
    }
    
    // Check for dot abbreviation (e.g., "L." for "LIST", "RU." for "RUN")
    if (command.length() > 1 && command.back() == '.') {
        std::string prefix = command.substr(0, command.length() - 1);
        std::vector<std::string> matches;
        
        // Find all commands that start with the prefix
        for (const auto& pair : s_commands) {
            if (pair.first.length() >= prefix.length() && 
                pair.first.substr(0, prefix.length()) == prefix) {
                matches.push_back(pair.first);
            }
        }
        
        // If exactly one match, return it
        if (matches.size() == 1) {
            for (const auto& pair : s_commands) {
                if (pair.first == matches[0]) {
                    return pair.second;
                }
            }
        }
        // If multiple matches, it's ambiguous - return INVALID
        // If no matches, also return INVALID
    }
    
    return ShellCommandType::INVALID;
}

bool CommandParser::isValidLineNumber(int lineNumber) const {
    return lineNumber > 0 && lineNumber <= 65535;
}

bool CommandParser::isValidFilename(const std::string& filename) const {
    return !filename.empty() && filename.length() < 256;
}

bool CommandParser::isShellCommand(const std::string& word) const {
    std::string upper = toUpper(word);
    return getCommandType(upper) != ShellCommandType::INVALID;
}

bool CommandParser::isBASICKeyword(const std::string& word) const {
    // Ensure keywords are synced
    if (!s_keywordsSynced) {
        const_cast<CommandParser*>(this)->syncKeywordsFromRegistry();
    }
    
    std::string upper = toUpper(word);
    return std::find(s_basicKeywords.begin(), s_basicKeywords.end(), upper) != s_basicKeywords.end();
}

void CommandParser::syncKeywordsFromRegistry() {
    if (s_keywordsSynced) {
        return;
    }
    
    // Get the command registry
    auto& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    
    // Add all registered function names to keywords
    auto functionNames = registry.getFunctionNames();
    for (const auto& name : functionNames) {
        std::string upper = toUpper(name);
        // Only add if not already in the list
        if (std::find(s_basicKeywords.begin(), s_basicKeywords.end(), upper) == s_basicKeywords.end()) {
            s_basicKeywords.push_back(upper);
        }
    }
    
    // Add all registered command names to keywords
    auto commandNames = registry.getCommandNames();
    for (const auto& name : commandNames) {
        std::string upper = toUpper(name);
        // Only add if not already in the list
        if (std::find(s_basicKeywords.begin(), s_basicKeywords.end(), upper) == s_basicKeywords.end()) {
            s_basicKeywords.push_back(upper);
        }
    }
    
    s_keywordsSynced = true;
}

std::string CommandParser::expandCommandAbbreviations(const std::string& input) const {
    std::string result = input;
    std::vector<std::string> tokens = tokenize(input);
    
    if (!tokens.empty()) {
        std::string firstToken = toUpper(tokens[0]);
        std::string expandedCommand;
        
        // Check if token ends with dot (expansion request)
        if (firstToken.length() > 0 && firstToken.back() == '.') {
            std::string prefix = firstToken.substr(0, firstToken.length() - 1);
            std::vector<std::string> matches;
            
            // Special case: just "." means RUN (no valid prefix match so we cheat)
            if (prefix.empty()) {
                expandedCommand = "RUN";
            } else {
                // Find all commands that start with the prefix
                for (const auto& pair : s_commands) {
                    if (pair.first.length() >= prefix.length() && 
                        pair.first.substr(0, prefix.length()) == prefix) {
                        matches.push_back(pair.first);
                    }
                }
                
                // Use first match (closest alphabetically) or shortest match
                if (!matches.empty()) {
                    expandedCommand = matches[0];  // First alphabetical match
                }
            }
            
            // Replace the first token if we found a match
            if (!expandedCommand.empty()) {
                size_t firstTokenEnd = tokens[0].length();
                result = expandedCommand + input.substr(firstTokenEnd);
            }
        }
    }
    
    return result;
}

std::string CommandParser::getExpandedCommandName(const std::string& command) const {
    // Check for exact match first
    for (const auto& pair : s_commands) {
        if (pair.first == command) {
            return pair.first;
        }
    }
    
    // Check for dot abbreviation
    if (command.length() > 1 && command.back() == '.') {
        std::string prefix = command.substr(0, command.length() - 1);
        std::vector<std::string> matches;
        
        // Find all commands that start with the prefix
        for (const auto& pair : s_commands) {
            if (pair.first.length() >= prefix.length() && 
                pair.first.substr(0, prefix.length()) == prefix) {
                matches.push_back(pair.first);
            }
        }
        
        // If exactly one match, return it
        if (matches.size() == 1) {
            return matches[0];
        }
        // If multiple matches or no matches, return original
    }
    
    return command;
}

std::string CommandParser::formatBasicKeywords(const std::string& line) const {
    std::string result = line;
    
    // Process the line character by character to identify and uppercase keywords
    size_t i = 0;
    while (i < result.length()) {
        // Skip whitespace
        if (std::isspace(result[i])) {
            i++;
            continue;
        }
        
        // Skip strings (don't uppercase keywords inside strings)
        if (result[i] == '"') {
            i++;
            while (i < result.length() && result[i] != '"') {
                i++;
            }
            if (i < result.length()) i++; // Skip closing quote
            continue;
        }
        
        // Check if we're at the start of a potential keyword
        if (std::isalpha(result[i])) {
            size_t wordStart = i;
            while (i < result.length() && (std::isalnum(result[i]) || result[i] == '_' || result[i] == '$')) {
                i++;
            }
            size_t wordLen = i - wordStart;
            std::string word = result.substr(wordStart, wordLen);
            std::string upperWord = toUpper(word);
            
            // Check if it's a BASIC keyword
            if (isBASICKeyword(upperWord)) {
                result.replace(wordStart, wordLen, upperWord);
            }
        } else {
            i++;
        }
    }
    
    return result;
}

std::string CommandParser::getLastError() const {
    return m_lastError;
}

bool CommandParser::hasError() const {
    return !m_lastError.empty();
}

void CommandParser::clearError() {
    m_lastError.clear();
}

void CommandParser::setError(const std::string& error) {
    m_lastError = error;
}

} // namespace FasterBASIC