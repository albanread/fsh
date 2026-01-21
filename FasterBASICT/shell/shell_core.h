//
// shell_core.h
// FasterBASIC Shell - Core Shell Functionality
//
// Main shell logic that ties together program management, command parsing,
// and program execution. Provides the interactive BASIC shell experience.
//

#ifndef SHELL_CORE_H
#define SHELL_CORE_H

#include "program_manager_v2.h"
#include "command_parser.h"
#include "screen_editor.h"
#include "../runtime/terminal_io.h"
#include "../src/modular_commands.h"
#include <string>
#include <vector>
#include <memory>
#include <signal.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <unordered_map>

// Forward declaration for Lua
struct lua_State;

namespace FasterBASIC {

// Function pointer type for additional Lua bindings (e.g., fbsh_voices-specific)
typedef void (*AdditionalLuaBindingsFunc)(lua_State* L);

// Global function pointer for additional Lua bindings
extern AdditionalLuaBindingsFunc g_additionalLuaBindings;

class ShellCore {
public:
    ShellCore();
    ~ShellCore();

    // Main shell operations
    void run();                                    // Start interactive shell
    void quit();                                   // Exit shell
    bool isRunning() const;                       // Check if shell is active

    // Command execution
    bool executeCommand(const std::string& input); // Execute single command
    void showPrompt();                            // Display shell prompt
    std::string readInput();                      // Read user input

    // Program execution
    bool runProgram(int startLine = -1);          // Run current program
    bool continueExecution();                     // Continue after break
    void stopExecution();                         // Stop program execution

    // Program management
    bool loadProgram(const std::string& filename); // Load program from file
    bool saveProgram(const std::string& filename); // Save program to file
    bool mergeProgram(const std::string& filename); // Merge program from file
    void newProgram();                            // Clear current program
    bool compileToFile(const std::string& filename); // Compile program to Lua file

    // Development tools
    bool checkSyntax();                           // Syntax check without running
    bool formatProgram();                         // Format/indent current program
    void showVariables();                         // Display current variables
    void clearVariables();                        // Clear variable state

    // Information and help
    void showHelp();                              // Display help information
    void showHelpCategories();                    // Display BASIC command categories
    void showHelpForTopicOrCommand(const std::string& topic);  // Display help for topic or command
    void showHelpForCategory(const std::string& category);     // Display commands in category
    void showHelpForCommand(const FasterBASIC::ModularCommands::CommandDefinition* cmd);  // Display detailed command help
    
    // Help database handlers
    void handleHelpRebuild();                     // Rebuild help database
    void handleHelpSearch(const std::string& query);  // Search help database
    void handleHelpTag(const std::string& tag);   // Show items with specific tag
    void handleHelpArticles();                    // List all articles
    void handleHelpArticle(const std::string& articleName);  // Show specific article
    
    void showVersion();                           // Display version info
    void showStatistics();                        // Show program statistics

    // Configuration
    void setVerbose(bool verbose);                // Enable/disable verbose output
    void setDebug(bool debug);                    // Enable/disable debug mode
    bool isVerbose() const;
    bool isDebug() const;

    // Signal handling
    void handleReset();                           // Handle Ctrl+C reset
    void handleTerminalResize();                  // Handle terminal resize (SIGWINCH)
    static void signalHandler(int signal);        // Static signal handler
    static ShellCore* s_instance;                 // Static instance for signal handling

private:
    ProgramManagerV2 m_program;
    CommandParser m_parser;
    TerminalIO* m_terminal;
    
    // Shell state
    bool m_running;
    bool m_verbose;
    bool m_debug;
    bool m_programRunning;
    int m_continueFromLine;
    
    // Auto-continuation state
    bool m_autoContinueMode;
    int m_lastLineNumber;
    int m_suggestedNextLine;
    
    // Execution state
    std::string m_tempFilename;
    std::string m_lastFilename;
    
    // Command handlers
    bool handleDirectLine(const ParsedCommand& cmd);
    bool handleDeleteLine(const ParsedCommand& cmd);
    bool handleList(const ParsedCommand& cmd);
    bool handleRun(const ParsedCommand& cmd);
    bool handleLoad(const ParsedCommand& cmd);
    bool handleSave(const ParsedCommand& cmd);
    bool handleMerge(const ParsedCommand& cmd);
    bool handleNew(const ParsedCommand& cmd);
    bool handleAuto(const ParsedCommand& cmd);
    bool handleRenum(const ParsedCommand& cmd);
    bool handleEdit(const ParsedCommand& cmd);
    bool handleFind(const ParsedCommand& cmd);
    bool handleFindNext(const ParsedCommand& cmd);
    bool handleReplace(const ParsedCommand& cmd);
    bool handleReplaceNext(const ParsedCommand& cmd);
    bool handleVars(const ParsedCommand& cmd);
    bool handleClear(const ParsedCommand& cmd);
    bool handleCheck(const ParsedCommand& cmd);
    bool handleFormat(const ParsedCommand& cmd);
    bool handleCls(const ParsedCommand& cmd);
    bool handleDir(const ParsedCommand& cmd);
    bool handleHelp(const ParsedCommand& cmd);
    bool handleQuit(const ParsedCommand& cmd);
    bool handleImmediate(const ParsedCommand& cmd);
    bool handleEditor(const ParsedCommand& cmd);

    // List command variants
    void listAll();
    void listRange(int start, int end);
    void listFrom(int start);
    void listTo(int end);
    void listLine(int line);

    // Utility functions
    bool executeCompiledProgram(const std::string& program, int startLine = -1);
    std::string generateTempFilename();
    bool fileExists(const std::string& filename) const;
    std::string readFileContent(const std::string& filename) const;
    bool writeFileContent(const std::string& filename, const std::string& content);
    void showError(const std::string& error);
    void showMessage(const std::string& message);
    void showSuccess(const std::string& message);
    
    // Program compilation and execution
    bool compileProgram(const std::string& program, std::string& luaCode);
    bool executeLuaCode(const std::string& luaCode);
    
    // File operations helpers
    std::string getDefaultExtension(const std::string& filename) const;
    std::string addExtensionIfNeeded(const std::string& filename) const;
    bool isValidBasicFile(const std::string& content) const;
    
    // BASIC directory helpers
    std::string getBasicScriptsDir() const;
    std::string getBasicLibDir() const;
    void ensureBasicDirectories() const;
    std::string resolveFilePath(const std::string& filename) const;
    
    // Display helpers
    void printProgramLine(int lineNumber, const std::string& code);
    void printHeader(const std::string& title);
    void printSeparator();
    
    // Help system helpers
    std::string formatCommandSignature(const FasterBASIC::ModularCommands::CommandDefinition* cmd);
    std::string formatFunctionSignature(const FasterBASIC::ModularCommands::CommandDefinition* func);
    
    // Interactive line editor
    std::string editLineInteractive(int lineNumber, const std::string& initialContent);
    void redrawLine(int lineNumber, const std::string& buffer, size_t cursorPos);
    
    // Line editor helper functions
    size_t findWordStart(const std::string& buffer, size_t pos);
    size_t findWordEnd(const std::string& buffer, size_t pos);
    size_t findNextWord(const std::string& buffer, size_t pos);
    size_t findPrevWord(const std::string& buffer, size_t pos);
    void clearToEndOfLine(std::string& buffer, size_t pos);
    void clearToStartOfLine(std::string& buffer, size_t& pos);
    void deleteWordBackward(std::string& buffer, size_t& pos);
    
    // Auto-continuation helpers
    int findNextAvailableLineNumber(int currentLine);
    int findPreviousLineNumber(int currentLine);
    std::string readInputWithInlineEditing();
    
    // Search helpers
    void showSearchResult(int foundLine, const std::string& foundContent, int contextLines);
    
    // Search state
    std::string m_lastSearchText;
    int m_lastSearchLine;
    int m_lastContextLines;
    bool m_hasActiveSearch;
    
    // Command history
    std::vector<std::string> m_commandHistory;
    int m_historyIndex;
    static const int MAX_HISTORY_SIZE = 10;
    
    // Command history helpers
    void addToHistory(const std::string& command);
    std::string readInputWithHistory();
    
    // Lua state tracking for interruption
    lua_State* m_currentLuaState;
    
    // Constants
    static const std::string SHELL_VERSION;
    static const std::string SHELL_PROMPT;
    static const std::string TEMP_FILE_PREFIX;
    static const int MAX_LINE_LENGTH;
};

} // namespace FasterBASIC

#endif // SHELL_CORE_H