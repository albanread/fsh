//
// command_parser.h
// FasterBASIC Shell - Command Parser
//
// Parses shell commands and identifies different types of user input.
// Handles BASIC line entry, shell commands, and immediate mode expressions.
//

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <vector>
#include <variant>

namespace FasterBASIC {

enum class ShellCommandType {
    // Program line operations
    DIRECT_LINE,        // 10 PRINT "Hello" - add/replace line
    DELETE_LINE,        // 10 - delete line 10
    
    // Deletion commands
    DEL,               // DEL 10 - delete line 10
    DEL_RANGE,         // DEL 10-50 - delete range of lines
    
    // Listing commands
    LIST,              // LIST - show all lines
    LIST_RANGE,        // LIST 10-50 - show range of lines
    LIST_LINE,         // LIST 10 - show single line
    LIST_FROM,         // LIST 10- - show from line 10 to end
    LIST_TO,           // LIST -50 - show from start to line 50
    
    // Execution commands
    RUN,               // RUN - execute program from start
    RUN_FROM,          // RUN 100 - execute from line 100
    CONT,              // CONT - continue execution
    STOP,              // STOP - stop execution
    
    // File operations
    LOAD,              // LOAD "filename" - load program
    SAVE,              // SAVE "filename" - save program
    MERGE,             // MERGE "filename" - merge program
    NEW,               // NEW - clear program
    
    // Development commands
    AUTO,              // AUTO - enable auto line numbering
    AUTO_PARAMS,       // AUTO 1000,10 - auto with start and step
    RENUM,             // RENUM - renumber program
    RENUM_PARAMS,      // RENUM 1000,10 - renumber with parameters
    EDIT,              // EDIT 100 - edit line 100 in full line editor
    EDITOR,            // EDITOR - enter full-screen editor mode
    
    // Search commands
    FIND,              // FIND "text" - find first occurrence of text
    FINDNEXT,          // FINDNEXT - find next occurrence
    REPLACE,           // REPLACE "new" - replace current found text
    REPLACENEXT,       // REPLACENEXT "new" - replace and find next
    
    // Information commands
    VARS,              // VARS - show variables
    CLEAR,             // CLEAR - clear variables
    CHECK,             // CHECK - syntax check
    FORMAT,            // FORMAT - format program
    
    // Shell commands
    CLS,               // CLS - clear screen
    DIR,               // DIR - list .bas files in current directory
    HELP,              // HELP - show help
    QUIT,              // QUIT/EXIT - exit shell
    
    // Immediate mode
    IMMEDIATE,         // Direct expression evaluation
    
    // Invalid/empty
    EMPTY,             // Empty input
    INVALID            // Invalid command
};

struct ParsedCommand {
    ShellCommandType type;
    
    // Parameters for different command types
    int lineNumber;              // For line operations
    std::string code;            // BASIC code for line entry
    std::string filename;        // For file operations
    int startLine;               // For ranges and renumbering
    int endLine;                 // For ranges
    int step;                    // For auto and renumber
    std::string expression;      // For immediate mode
    std::string searchText;      // For FIND commands and HELP searches
    std::string replaceText;     // For REPLACE commands
    int contextLines;            // Number of context lines to show
    std::string helpSubcommand;  // For HELP subcommands (SEARCH, REBUILD, TAG, ARTICLES, ARTICLE)
    
    // Flags
    bool hasLineNumber;
    bool hasRange;
    bool hasFilename;
    bool hasStep;
    
    ParsedCommand() : type(ShellCommandType::EMPTY), lineNumber(0), startLine(0), 
                     endLine(0), step(10), contextLines(0), hasLineNumber(false), hasRange(false), 
                     hasFilename(false), hasStep(false) {}
};

class CommandParser {
public:
    CommandParser();
    ~CommandParser();
    
    // Main parsing function
    ParsedCommand parse(const std::string& input);
    
    // Helper functions for validation
    bool isValidLineNumber(int lineNumber) const;
    bool isValidFilename(const std::string& filename) const;
    
    // Command recognition
    bool isShellCommand(const std::string& word) const;
    bool isBASICKeyword(const std::string& word) const;
    std::string getExpandedCommandName(const std::string& command) const;
    std::string expandCommandAbbreviations(const std::string& input) const;
    std::string formatBasicKeywords(const std::string& line) const;
    
    // Registry synchronization
    void syncKeywordsFromRegistry();
    
    // Utility functions
    std::vector<std::string> tokenize(const std::string& input) const;
    
    // Error information
    std::string getLastError() const;
    bool hasError() const;
    void clearError();

private:
    std::string m_lastError;
    
    // Parsing helper functions
    ParsedCommand parseDirectLine(const std::string& input);
    ParsedCommand parseListCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseDelCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseRunCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseFileCommand(const std::vector<std::string>& tokens, ShellCommandType type);
    ParsedCommand parseAutoCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseRenumCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseEditCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseFindCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseReplaceCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseSimpleCommand(ShellCommandType type);
    ParsedCommand parseHelpCommand(const std::vector<std::string>& tokens);
    ParsedCommand parseImmediate(const std::string& input);
    
    // Utility functions
    std::string trim(const std::string& str) const;
    std::string toUpper(const std::string& str) const;
    bool isNumber(const std::string& str) const;
    int parseNumber(const std::string& str) const;
    bool parseRange(const std::string& rangeStr, int& start, int& end) const;
    std::string extractQuotedString(const std::string& str) const;
    
    // Command lookup
    ShellCommandType getCommandType(const std::string& command) const;
    
    // Error handling
    void setError(const std::string& error);
    
    // Static command mappings
    static const std::vector<std::pair<std::string, ShellCommandType>> s_commands;
    static std::vector<std::string> s_basicKeywords;  // Changed to non-const for dynamic population
    static bool s_keywordsSynced;  // Track if we've synced with registry
};

} // namespace FasterBASIC

#endif // COMMAND_PARSER_H