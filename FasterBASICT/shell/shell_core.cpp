//
// shell_core.cpp
// FasterBASIC Shell - Core Shell Functionality
//
// Main shell logic that ties together program management, command parsing,
// and program execution. Provides the interactive BASIC shell experience.
//

#include "shell_core.h"
#include "help_database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../src/basic_formatter_lib.h"
#include "../src/fasterbasic_lexer.h"
#include "../src/fasterbasic_parser.h"
#include "../src/modular_commands.h"
#include "../src/plugin_loader.h"
#include "../src/fasterbasic_semantic.h"
#include "../src/fasterbasic_optimizer.h"
#include "../runtime/timer_lua_bindings_terminal.h"
#include "../src/fasterbasic_peephole.h"

// Global flag for Lua interruption
static volatile bool g_shouldStopLua = false;

// Global flag for terminal resize
volatile bool g_terminalResized = false;
#include "../src/fasterbasic_cfg.h"
#include "../src/fasterbasic_ircode.h"
#include "../src/fasterbasic_lua_codegen.h"
#include "../runtime/data_lua_bindings.h"
#include "../runtime/terminal_lua_bindings.h"
#include "../runtime/timer_lua_bindings_terminal.h"
#include "../runtime/DataManager.h"

#ifdef VOICE_CONTROLLER_ENABLED
#include "../../FBRunner3/FBTBindings.h"
#include "../../FBRunner3/register_voice.h"
#endif

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Runtime module registration functions
extern "C" void register_unicode_module(lua_State* L);
extern "C" void register_bitwise_module(lua_State* L);
extern "C" void register_constants_module(lua_State* L);
extern "C" void set_constants_manager(FasterBASIC::ConstantsManager* manager);

// Forward declare file I/O bindings
namespace FasterBASIC {
    void register_fileio_functions(lua_State* L);
    void clear_fileio_state();
    void registerDataBindings(lua_State* L);
    void registerTerminalBindings(lua_State* L);
    void initializeDataManager(const std::vector<DataValue>& values);
    void addDataRestorePoint(int lineNumber, size_t dataIndex);
    void addDataRestorePointByLabel(const std::string& label, size_t dataIndex);
}

// Plugin Lua bindings registration
extern "C" void registerPluginLuaBindings(lua_State* L);

// Lua C function to check for interruption
static int lua_check_should_stop(lua_State* L) {
    if (g_shouldStopLua) {
        luaL_error(L, "Program interrupted by user (Ctrl+C)");
    }
    return 0;
}

// Use terminal-specific timer bindings (no frame-based events)
namespace FasterBASIC {
namespace Terminal {
    void registerTimerBindings(lua_State* L);
}
}

namespace FasterBASIC {

// Global function pointer for additional Lua bindings (e.g., fbsh_voices-specific)
AdditionalLuaBindingsFunc g_additionalLuaBindings = nullptr;

// Static constants
const std::string ShellCore::SHELL_VERSION = "1.0";
const std::string ShellCore::SHELL_PROMPT = "Ready.";
const std::string ShellCore::TEMP_FILE_PREFIX = "/tmp/fasterbasic_";
const int ShellCore::MAX_LINE_LENGTH = 1024;

// Static instance for signal handling
ShellCore* ShellCore::s_instance = nullptr;

ShellCore::ShellCore()
    : m_terminal(&g_terminal)
    , m_running(false)
    , m_verbose(false)
    , m_debug(false)
    , m_programRunning(false)
    , m_continueFromLine(-1)
    , m_autoContinueMode(false)
    , m_lastLineNumber(0)
    , m_suggestedNextLine(0)
    , m_lastSearchLine(0)
    , m_lastContextLines(3)
    , m_hasActiveSearch(false)
    , m_historyIndex(-1)
    , m_currentLuaState(nullptr)
{
    // Set up signal handler for Ctrl+C
    s_instance = this;
    signal(SIGINT, signalHandler);
    
    // Set up signal handler for terminal resize
    signal(SIGWINCH, signalHandler);

    // Ensure BASIC directories exist
    ensureBasicDirectories();
}

ShellCore::~ShellCore() {
    // Clean up signal handlers
    s_instance = nullptr;
    signal(SIGINT, SIG_DFL);
    signal(SIGWINCH, SIG_DFL);
}

void ShellCore::run() {
    m_running = true;

    while (m_running) {
        showPrompt();
        std::string input = readInput();

        if (!input.empty()) {
            executeCommand(input);
        }
    }
}

void ShellCore::handleReset() {
    // Terminate any running program/script immediately
    stopExecution();

    // Stop all active timers
    FasterBASIC::Terminal::stopAllTimersFromShell();
    
    // Set flag to interrupt Lua execution
    g_shouldStopLua = true;

    // Reset shell state completely
    m_programRunning = false;
    m_continueFromLine = -1;
    m_autoContinueMode = false;
    m_lastLineNumber = 0;
    m_suggestedNextLine = 0;

    // Clear program manager auto mode
    m_program.setAutoMode(false);

    // Clear any temporary files or execution state
    if (!m_tempFilename.empty()) {
        std::remove(m_tempFilename.c_str());
        m_tempFilename.clear();
    }

    // Reset terminal to normal state (in case we were in raw mode)
    if (m_terminal) {
        // Ensure terminal is back to normal state
        std::cout << "\x1B[0m";  // Reset all terminal attributes
        std::cout.flush();
    }

    // Clear any pending input
    while (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.get();
    }

    // Show reset message and prompt
    std::cout << "\n\nRESET (use QUIT to exit)\n\nReady.\n";
    std::cout.flush();
}

void ShellCore::handleTerminalResize() {
    // Get new terminal size
    auto size = m_terminal->getScreenSize();
    
    // Optionally print a message (can be disabled for cleaner output)
    // std::cout << "\n[Terminal resized to " << size.first << "x" << size.second << "]\n";
    // std::cout.flush();
}

void ShellCore::signalHandler(int signal) {
    if (signal == SIGINT && s_instance) {
        // Flush any pending output
        std::cout.flush();
        std::cerr.flush();

        // Reset terminal state if we were in raw mode
        struct termios oldTermios;
        tcgetattr(STDIN_FILENO, &oldTermios);
        oldTermios.c_lflag |= (ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

        s_instance->handleReset();
    } else if (signal == SIGWINCH && s_instance) {
        // Terminal size changed
        g_terminalResized = true;
    }
}

void ShellCore::quit() {
    m_running = false;
}

bool ShellCore::isRunning() const {
    return m_running;
}

bool ShellCore::executeCommand(const std::string& input) {
    ParsedCommand cmd = m_parser.parse(input);

    if (m_parser.hasError()) {
        showError(m_parser.getLastError());
        return false;
    }

    // Add recognized commands to history (not line entries)
    bool isCommand = false;
    switch (cmd.type) {
        case ShellCommandType::LIST:
        case ShellCommandType::LIST_RANGE:
        case ShellCommandType::LIST_LINE:
        case ShellCommandType::LIST_FROM:
        case ShellCommandType::LIST_TO:
        case ShellCommandType::RUN:
        case ShellCommandType::RUN_FROM:
        case ShellCommandType::LOAD:
        case ShellCommandType::SAVE:
        case ShellCommandType::MERGE:
        case ShellCommandType::NEW:
        case ShellCommandType::AUTO:
        case ShellCommandType::AUTO_PARAMS:
        case ShellCommandType::RENUM:
        case ShellCommandType::RENUM_PARAMS:
        case ShellCommandType::EDIT:
        case ShellCommandType::FIND:
        case ShellCommandType::FINDNEXT:
        case ShellCommandType::REPLACE:
        case ShellCommandType::REPLACENEXT:
        case ShellCommandType::VARS:
        case ShellCommandType::CLEAR:
        case ShellCommandType::CHECK:
        case ShellCommandType::FORMAT:
        case ShellCommandType::CLS:
        case ShellCommandType::DIR:
        case ShellCommandType::HELP:
        case ShellCommandType::QUIT:
            isCommand = true;
            addToHistory(input);
            break;
        default:
            break;
    }

    switch (cmd.type) {
        case ShellCommandType::DIRECT_LINE:
            return handleDirectLine(cmd);

        case ShellCommandType::DELETE_LINE:
            return handleDeleteLine(cmd);

        case ShellCommandType::LIST:
        case ShellCommandType::LIST_RANGE:
        case ShellCommandType::LIST_LINE:
        case ShellCommandType::LIST_FROM:
        case ShellCommandType::LIST_TO:
            return handleList(cmd);

        case ShellCommandType::RUN:
        case ShellCommandType::RUN_FROM:
            return handleRun(cmd);

        case ShellCommandType::LOAD:
            return handleLoad(cmd);

        case ShellCommandType::SAVE:
            return handleSave(cmd);

        case ShellCommandType::MERGE:
            return handleMerge(cmd);

        case ShellCommandType::NEW:
            return handleNew(cmd);

        case ShellCommandType::AUTO:
        case ShellCommandType::AUTO_PARAMS:
            return handleAuto(cmd);

        case ShellCommandType::RENUM:
        case ShellCommandType::RENUM_PARAMS:
            return handleRenum(cmd);

        case ShellCommandType::EDIT:
            return handleEdit(cmd);

        case ShellCommandType::EDITOR:
            return handleEditor(cmd);

        case ShellCommandType::FIND:
            return handleFind(cmd);

        case ShellCommandType::FINDNEXT:
            return handleFindNext(cmd);

        case ShellCommandType::REPLACE:
            return handleReplace(cmd);

        case ShellCommandType::REPLACENEXT:
            return handleReplaceNext(cmd);

        case ShellCommandType::VARS:
            return handleVars(cmd);

        case ShellCommandType::CLEAR:
            return handleClear(cmd);

        case ShellCommandType::CHECK:
            return handleCheck(cmd);

        case ShellCommandType::FORMAT:
            return handleFormat(cmd);

        case ShellCommandType::CLS:
            return handleCls(cmd);

        case ShellCommandType::DIR:
            return handleDir(cmd);

        case ShellCommandType::HELP:
            return handleHelp(cmd);

        case ShellCommandType::QUIT:
            return handleQuit(cmd);

        case ShellCommandType::IMMEDIATE:
            return handleImmediate(cmd);

        case ShellCommandType::EMPTY:
            return true;  // Just show prompt again

        default:
            showError("Unknown or invalid command");
            return false;
    }
}

void ShellCore::showPrompt() {
    if (m_program.isAutoMode()) {
        int nextLine = m_program.getNextAutoLine();
        std::cout << nextLine << " ";
        std::cout.flush();
    } else if (m_autoContinueMode) {
        // Use inline editing for auto-continuation
        std::string result = readInputWithInlineEditing();
        if (result.empty()) {
            // Empty - exit auto-continue mode
            m_autoContinueMode = false;
            m_suggestedNextLine = 0;
            std::cout << "\nReady.\n";
        } else {
            // Check if user typed their own line number (starts with digit)
            if (!result.empty() && result[0] >= '0' && result[0] <= '9') {
                // User entered their own line number, exit auto-continue
                m_autoContinueMode = false;
                m_suggestedNextLine = 0;
                executeCommand(result);
            } else {
                // Process the line with our suggested line number
                std::string fullLine = std::to_string(m_suggestedNextLine) + " " + result;
                executeCommand(fullLine);
            }
        }
        return;
    }
}

std::string ShellCore::readInput() {
    if (m_autoContinueMode) {
        // Skip normal input when in auto-continue mode
        // (handled by showPrompt now)
        return "";
    }

    // Use history-aware input reading
    return readInputWithHistory();
}

void ShellCore::addToHistory(const std::string& command) {
    // Don't add empty commands or duplicates of the last command
    if (command.empty()) {
        return;
    }
    if (!m_commandHistory.empty() && m_commandHistory.back() == command) {
        return;
    }

    // Add to history
    m_commandHistory.push_back(command);

    // Keep only last MAX_HISTORY_SIZE commands
    if (m_commandHistory.size() > MAX_HISTORY_SIZE) {
        m_commandHistory.erase(m_commandHistory.begin());
    }

    // Reset history index
    m_historyIndex = -1;
}

std::string ShellCore::readInputWithHistory() {
    std::string buffer;
    size_t cursorPos = 0;
    bool done = false;

    // Save current terminal settings
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;

    // Enable raw mode for character-by-character input
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // Start with empty buffer at prompt
    std::cout.flush();

    while (!done) {
        char ch = std::cin.get();

        if (ch == '\n' || ch == '\r') {
            // Enter - accept input
            done = true;
            std::cout << std::endl;
        } else if (ch == '\x1B') {  // ESC key
            // Check for arrow key sequences
            if (std::cin.peek() == '[') {
                std::cin.get(); // consume '['
                char seq2 = std::cin.get();
                switch (seq2) {
                    case 'A':  // Up arrow - previous command in history
                        if (!m_commandHistory.empty()) {
                            if (m_historyIndex == -1) {
                                m_historyIndex = m_commandHistory.size() - 1;
                            } else if (m_historyIndex > 0) {
                                m_historyIndex--;
                            }
                            buffer = m_commandHistory[m_historyIndex];
                            cursorPos = buffer.length();
                            // Redraw line
                            std::cout << "\r\x1B[K" << buffer;
                            std::cout.flush();
                        }
                        break;
                    case 'B':  // Down arrow - next command in history
                        if (!m_commandHistory.empty() && m_historyIndex != -1) {
                            if (m_historyIndex < (int)m_commandHistory.size() - 1) {
                                m_historyIndex++;
                                buffer = m_commandHistory[m_historyIndex];
                                cursorPos = buffer.length();
                            } else {
                                // Go past end of history - clear line
                                m_historyIndex = -1;
                                buffer.clear();
                                cursorPos = 0;
                            }
                            // Redraw line
                            std::cout << "\r\x1B[K" << buffer;
                            std::cout.flush();
                        }
                        break;
                    case 'C':  // Right arrow
                        if (cursorPos < buffer.length()) {
                            cursorPos++;
                            std::cout << "\r\x1B[K" << buffer;
                            // Move cursor to position (column is 1-based)
                            std::cout << "\x1B[" << (cursorPos + 1) << "G";
                            std::cout.flush();
                        }
                        break;
                    case 'D':  // Left arrow
                        if (cursorPos > 0) {
                            cursorPos--;
                            std::cout << "\r\x1B[K" << buffer;
                            // Move cursor to position (column is 1-based)
                            std::cout << "\x1B[" << (cursorPos + 1) << "G";
                            std::cout.flush();
                        }
                        break;
                    case 'H':  // Home key
                        cursorPos = 0;
                        std::cout << "\r\x1B[K" << buffer << "\r";
                        std::cout.flush();
                        break;
                    case 'F':  // End key
                        cursorPos = buffer.length();
                        std::cout << "\r\x1B[K" << buffer;
                        std::cout.flush();
                        break;
                }
            }
        } else if (ch == '\x7F' || ch == '\b') {  // Backspace
            if (cursorPos > 0) {
                buffer.erase(cursorPos - 1, 1);
                cursorPos--;
                // Redraw line
                std::cout << "\r\x1B[K" << buffer;
                // Move cursor to position (column is 1-based)
                std::cout << "\x1B[" << (cursorPos + 1) << "G";
                std::cout.flush();
            }
        } else if (ch == '\x03') {  // Ctrl+C
            // Cancel input
            buffer.clear();
            done = true;
            std::cout << "^C\n";
        } else if (ch == '\x04') {  // Ctrl+D - EOF
            if (buffer.empty()) {
                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                quit();
                return "";
            }
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            buffer.insert(cursorPos, 1, ch);
            cursorPos++;
            // Redraw line
            std::cout << "\r\x1B[K" << buffer;
            // Move cursor to position (column is 1-based)
            std::cout << "\x1B[" << (cursorPos + 1) << "G";
            std::cout.flush();
        }
    }

    // Restore normal terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    return buffer;
}

bool ShellCore::runProgram(int startLine) {
    if (m_program.isEmpty()) {
        showError("No program in memory");
        return false;
    }

    // Generate program text
    std::string programText;
    if (startLine <= 0) {
        programText = m_program.generateProgram();
    } else {
        programText = m_program.generateProgramRange(startLine, -1);
    }

    return executeCompiledProgram(programText, startLine);
}

bool ShellCore::continueExecution() {
    // TODO: Implement continue execution functionality
    showError("Continue execution not yet implemented");
    return false;
}

void ShellCore::stopExecution() {
    // Force stop any running program
    m_programRunning = false;
    m_continueFromLine = -1;

    // Note: With embedded compiler, no subprocesses to kill
    // Lua state is in-process

    // Clear execution state
    if (!m_tempFilename.empty()) {
        std::remove(m_tempFilename.c_str());
    }
}

// Command handlers

bool ShellCore::handleDirectLine(const ParsedCommand& cmd) {
    // Format BASIC keywords in the code before storing
    std::string formattedCode = m_parser.formatBasicKeywords(cmd.code);
    m_program.setLine(cmd.lineNumber, formattedCode);

    if (m_program.isAutoMode()) {
        m_program.incrementAutoLine();
    } else {
        // Check if we should suggest the next line for auto-continuation
        m_lastLineNumber = cmd.lineNumber;
        int nextLine = findNextAvailableLineNumber(cmd.lineNumber);
        if (nextLine > 0) {
            m_autoContinueMode = true;
            m_suggestedNextLine = nextLine;
        }
    }

    // Don't print Ready after line entry - stay out of user's way
    return true;
}

bool ShellCore::handleDeleteLine(const ParsedCommand& cmd) {
    m_program.deleteLine(cmd.lineNumber);
    return true;
}

bool ShellCore::handleList(const ParsedCommand& cmd) {
    std::cout << "\n";
    switch (cmd.type) {
        case ShellCommandType::LIST:
            listAll();
            break;

        case ShellCommandType::LIST_RANGE:
            listRange(cmd.startLine, cmd.endLine);
            break;

        case ShellCommandType::LIST_LINE:
            listLine(cmd.lineNumber);
            break;

        case ShellCommandType::LIST_FROM:
            listFrom(cmd.startLine);
            break;

        case ShellCommandType::LIST_TO:
            listTo(cmd.endLine);
            break;

        default:
            return false;
    }

    std::cout << "\nReady.\n";
    return true;
}

bool ShellCore::handleRun(const ParsedCommand& cmd) {
    int startLine = -1;
    if (cmd.type == ShellCommandType::RUN_FROM) {
        startLine = cmd.lineNumber;
    }

    return runProgram(startLine);
}

bool ShellCore::handleLoad(const ParsedCommand& cmd) {
    return loadProgram(cmd.filename);
}

bool ShellCore::handleSave(const ParsedCommand& cmd) {
    // If no filename provided, use the current program's filename
    if (!cmd.hasFilename || cmd.filename.empty()) {
        std::string currentFilename = m_program.getFilename();
        if (currentFilename.empty()) {
            showError("No filename specified and no file loaded");
            return false;
        }
        return saveProgram(currentFilename);
    }
    return saveProgram(cmd.filename);
}

bool ShellCore::handleMerge(const ParsedCommand& cmd) {
    return mergeProgram(cmd.filename);
}

bool ShellCore::handleNew(const ParsedCommand& cmd) {
    newProgram();
    return true;
}

bool ShellCore::handleAuto(const ParsedCommand& cmd) {
    if (cmd.type == ShellCommandType::AUTO_PARAMS) {
        m_program.setAutoMode(true, cmd.startLine, cmd.step);
    } else {
        m_program.setAutoMode(true);
    }

    showMessage("Automatic line numbering enabled");
    return true;
}

bool ShellCore::handleRenum(const ParsedCommand& cmd) {
    if (m_program.isEmpty()) {
        showError("No program to renumber");
        return false;
    }

    m_program.renumber(cmd.startLine, cmd.step);
    showMessage("Program renumbered");
    return true;
}

bool ShellCore::handleEdit(const ParsedCommand& cmd) {
    // Get the current content of the line (if it exists)
    std::string currentContent = m_program.getLine(cmd.lineNumber);

    // Call the interactive line editor with pre-filled content
    std::string editedContent = editLineInteractive(cmd.lineNumber, currentContent);

    // Check if edit was cancelled or navigation occurred
    if (editedContent == "\x1B") {  // ESC character indicates cancel or navigation
        // Line was already saved during navigation
        std::cout << "\nReady.\n";
        return true;
    }

    // If empty input, check if line still exists (navigation may have saved it)
    if (editedContent.empty()) {
        std::string existingContent = m_program.getLine(cmd.lineNumber);
        if (existingContent.empty() && !currentContent.empty()) {
            // Line was cleared during edit
            m_program.deleteLine(cmd.lineNumber);
        }
        // If line exists with content, it was already saved during navigation
    } else {
        // Format BASIC keywords and set the new line content
        std::string formattedContent = m_parser.formatBasicKeywords(editedContent);
        m_program.setLine(cmd.lineNumber, formattedContent);
    }

    std::cout << "\nReady.\n";
    return true;
}

bool ShellCore::handleFind(const ParsedCommand& cmd) {
    if (m_program.isEmpty()) {
        showError("No program in memory");
        return false;
    }

    // Store search parameters for FINDNEXT and REPLACE
    m_lastSearchText = cmd.searchText;
    m_lastContextLines = cmd.contextLines;
    m_lastSearchLine = 0;  // Start from beginning
    m_hasActiveSearch = false;

    // Get all line numbers
    std::vector<int> lineNumbers = m_program.getLineNumbers();
    if (lineNumbers.empty()) {
        showError("No program lines to search");
        return false;
    }

    // Search for the text (case-insensitive)
    std::string searchLower = m_lastSearchText;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (int lineNum : lineNumbers) {
        std::string lineContent = m_program.getLine(lineNum);
        std::string contentLower = lineContent;
        std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);

        if (contentLower.find(searchLower) != std::string::npos) {
            // Found it! Display the line and context
            m_lastSearchLine = lineNum;
            m_hasActiveSearch = true;
            showSearchResult(lineNum, lineContent, m_lastContextLines);
            return true;
        }
    }

    // Not found
    showError("\"" + m_lastSearchText + "\" not found");
    return false;
}

bool ShellCore::handleFindNext(const ParsedCommand& cmd) {
    if (m_lastSearchText.empty()) {
        showError("No previous search. Use FIND first.");
        return false;
    }

    if (m_program.isEmpty()) {
        showError("No program in memory");
        return false;
    }

    // Get all line numbers
    std::vector<int> lineNumbers = m_program.getLineNumbers();
    if (lineNumbers.empty()) {
        showError("No program lines to search");
        return false;
    }

    // Find starting position (after last found line)
    std::string searchLower = m_lastSearchText;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (int lineNum : lineNumbers) {
        if (lineNum <= m_lastSearchLine) {
            continue;  // Skip lines we've already searched
        }

        std::string lineContent = m_program.getLine(lineNum);
        std::string contentLower = lineContent;
        std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);

        if (contentLower.find(searchLower) != std::string::npos) {
            // Found next occurrence!
            m_lastSearchLine = lineNum;
            m_hasActiveSearch = true;
            showSearchResult(lineNum, lineContent, m_lastContextLines);
            return true;
        }
    }

    // Not found - wrap around or end
    showError("\"" + m_lastSearchText + "\" not found (end of program)");
    return false;
}

bool ShellCore::handleReplace(const ParsedCommand& cmd) {
    if (!m_hasActiveSearch || m_lastSearchText.empty()) {
        showError("No active search. Use FIND first, then REPLACE.");
        return false;
    }

    if (m_lastSearchLine <= 0) {
        showError("No current search result to replace.");
        return false;
    }

    // Get the current line content
    std::string currentContent = m_program.getLine(m_lastSearchLine);
    if (currentContent.empty()) {
        showError("Search result line no longer exists.");
        m_hasActiveSearch = false;
        return false;
    }

    // Perform case-insensitive replacement of first occurrence
    std::string searchLower = m_lastSearchText;
    std::string contentLower = currentContent;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);

    size_t pos = contentLower.find(searchLower);
    if (pos == std::string::npos) {
        showError("Search text no longer found in line " + std::to_string(m_lastSearchLine));
        m_hasActiveSearch = false;
        return false;
    }

    // Replace the text (preserve original case context)
    std::string newContent = currentContent;
    newContent.replace(pos, m_lastSearchText.length(), cmd.replaceText);

    // Update the line
    m_program.setLine(m_lastSearchLine, newContent);

    // Show the result
    std::cout << "\nReplaced \"" << m_lastSearchText << "\" with \"" << cmd.replaceText
              << "\" in line " << m_lastSearchLine << ":\n";
    std::cout << m_lastSearchLine << " " << newContent << "\n\n";

    // Clear active search since we've modified the content
    m_hasActiveSearch = false;

    return true;
}

bool ShellCore::handleReplaceNext(const ParsedCommand& cmd) {
    // First perform the replace on current found item
    if (!handleReplace(cmd)) {
        return false;
    }

    // Then find the next occurrence
    ParsedCommand findCmd;
    findCmd.type = ShellCommandType::FINDNEXT;
    if (handleFindNext(findCmd)) {
        std::cout << "Ready for next replace. Use REPLACE \"" << cmd.replaceText
                  << "\" or REPLACENEXT \"" << cmd.replaceText << "\"\n\n";
    }

    return true;
}

bool ShellCore::handleVars(const ParsedCommand& cmd) {
    showVariables();
    return true;
}

bool ShellCore::handleClear(const ParsedCommand& cmd) {
    clearVariables();
    return true;
}

bool ShellCore::handleCheck(const ParsedCommand& cmd) {
    return checkSyntax();
}

bool ShellCore::handleFormat(const ParsedCommand& cmd) {
    return formatProgram();
}

bool ShellCore::handleCls(const ParsedCommand& cmd) {
    m_terminal->clearScreen();
    return true;
}

bool ShellCore::handleDir(const ParsedCommand& cmd) {
    std::string scriptsDir = getBasicScriptsDir();
    std::string libDir = getBasicLibDir();

    std::vector<std::pair<std::string, std::string>> basFiles; // filename, full path

    // Scan scripts directory
    DIR* dir = opendir(scriptsDir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 &&
                filename.substr(filename.length() - 4) == ".bas") {
                basFiles.push_back({filename, scriptsDir + filename});
            }
        }
        closedir(dir);
    }

    // Scan lib directory
    dir = opendir(libDir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 &&
                filename.substr(filename.length() - 4) == ".bas") {
                basFiles.push_back({"lib/" + filename, libDir + filename});
            }
        }
        closedir(dir);
    }

    // Sort files alphabetically by display name
    std::sort(basFiles.begin(), basFiles.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    if (basFiles.empty()) {
        showMessage("No .bas files found");
        std::cout << "Scripts directory: " << scriptsDir << "\n";
        std::cout << "Library directory: " << libDir << "\n";
        return true;
    }

    // Display files
    std::cout << "\nBASIC files:\n";
    for (const auto& file : basFiles) {
        // Get file size
        struct stat st;
        if (stat(file.second.c_str(), &st) == 0) {
            std::cout << "  " << file.first;
            // Pad to 40 characters
            if (file.first.length() < 40) {
                std::cout << std::string(40 - file.first.length(), ' ');
            }
            std::cout << " (" << st.st_size << " bytes)\n";
        } else {
            std::cout << "  " << file.first << "\n";
        }
    }
    std::cout << "\n" << basFiles.size() << " file(s)\n";
    std::cout << "Scripts: " << scriptsDir << "\n";
    std::cout << "Library: " << libDir << "\n";

    return true;
}

bool ShellCore::handleHelp(const ParsedCommand& cmd) {
    // Check for help database subcommands
    if (!cmd.helpSubcommand.empty()) {
        if (cmd.helpSubcommand == "REBUILD") {
            handleHelpRebuild();
            return true;
        }

        if (cmd.helpSubcommand == "SEARCH") {
            if (cmd.searchText.empty()) {
                showError("HELP SEARCH requires a search query");
                std::cout << "Usage: HELP SEARCH <query>\n";
                return true;
            }
            handleHelpSearch(cmd.searchText);
            return true;
        }

        if (cmd.helpSubcommand == "TAG") {
            handleHelpTag(cmd.searchText);
            return true;
        }

        if (cmd.helpSubcommand == "ARTICLES") {
            handleHelpArticles();
            return true;
        }

        if (cmd.helpSubcommand == "ARTICLE") {
            if (cmd.searchText.empty()) {
                showError("HELP ARTICLE requires an article name");
                std::cout << "Usage: HELP ARTICLE <name>\n";
                return true;
            }
            handleHelpArticle(cmd.searchText);
            return true;
        }
    }

    // Check if a topic or command was specified
    if (!cmd.searchText.empty()) {
        showHelpForTopicOrCommand(cmd.searchText);
    } else {
        showHelp();
    }
    return true;
}

bool ShellCore::handleQuit(const ParsedCommand& cmd) {
    quit();
    return true;
}

bool ShellCore::handleEditor(const ParsedCommand& cmd) {
    if (m_program.isEmpty()) {
        // Create an empty starter line
        m_program.setLine(1000, "");
    }
    
    showMessage("Entering screen editor mode...");
    std::cout << std::flush;
    
    // Create and run the screen editor
    const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    ScreenEditor editor(m_program, &registry);
    bool modified = editor.run();
    
    // Check if user wants to run the program
    bool shouldRun = editor.shouldRunProgram();
    
    // Refresh screen after exiting editor
    if (m_terminal) {
        m_terminal->clearScreen();
        m_terminal->locate(1, 1);
    }
    
    if (shouldRun) {
        // Run the program
        showMessage("Running program...");
        std::cout << std::flush;
        
        if (m_program.isEmpty()) {
            showError("No program to run");
            return true;
        }
        
        // Execute the program
        runProgram(-1);  // -1 means start from beginning
        
        // Wait for user to press a key
        std::cout << "\n";
        std::cout << "Program finished - press any key to return to editor";
        std::cout << std::flush;
        
        // Wait for key press
        if (m_terminal) {
            m_terminal->enableRawMode();
            m_terminal->waitForKey();
            m_terminal->disableRawMode();
        } else {
            std::cin.get();
        }
        
        // Return to editor
        return handleEditor(cmd);
    }
    
    if (modified) {
        showSuccess("Program modified in editor");
    } else {
        showMessage("Editor exited");
    }
    
    return true;
}

bool ShellCore::handleImmediate(const ParsedCommand& cmd) {
    // For Phase 1, just show a message
    showMessage("Immediate mode not yet implemented");
    return true;
}

// List command variants

void ShellCore::listAll() {
    if (m_program.isEmpty()) {
        showMessage("No program in memory");
        return;
    }

    // Get formatted program for display
    std::string programText = m_program.generateProgram();
    FormatterOptions options;
    options.start_line = -1; // Don't renumber for listing, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    FormatterResult result = formatBasicCode(programText, options, &registry, getGlobalPredefinedConstants());

    if (result.success && !result.formatted_code.empty()) {
        // Display the formatted program
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        auto lines = m_program.getAllLines();
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listRange(int start, int end) {
    ProgramManagerV2::ListRange range(start, end);
    auto lines = m_program.getLines(range);

    if (lines.empty()) {
        showMessage("No lines in specified range");
        return;
    }

    // Generate program text for range
    std::string programText;
    for (const auto& line : lines) {
        programText += std::to_string(line.first) + " " + line.second + "\n";
    }

    // Format the range
    FormatterOptions options;
    options.start_line = -1; // Don't renumber, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    FormatterResult result = formatBasicCode(programText, options, &registry, getGlobalPredefinedConstants());

    if (result.success && !result.formatted_code.empty()) {
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listFrom(int start) {
    ProgramManagerV2::ListRange range;
    range.startLine = start;
    range.hasStart = true;
    range.hasEnd = false;

    auto lines = m_program.getLines(range);

    if (lines.empty()) {
        showMessage("No lines from line " + std::to_string(start));
        return;
    }

    // Generate program text for range
    std::string programText;
    for (const auto& line : lines) {
        programText += std::to_string(line.first) + " " + line.second + "\n";
    }

    // Format the range
    FormatterOptions options;
    options.start_line = -1; // Don't renumber, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    FormatterResult result = formatBasicCode(programText, options, &registry, getGlobalPredefinedConstants());

    if (result.success && !result.formatted_code.empty()) {
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listTo(int end) {
    ProgramManagerV2::ListRange range;
    range.endLine = end;
    range.hasStart = false;
    range.hasEnd = true;

    auto lines = m_program.getLines(range);

    if (lines.empty()) {
        showMessage("No lines up to line " + std::to_string(end));
        return;
    }

    // Generate program text for range
    std::string programText;
    for (const auto& line : lines) {
        programText += std::to_string(line.first) + " " + line.second + "\n";
    }

    // Format the range
    FormatterOptions options;
    options.start_line = -1; // Don't renumber, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    FormatterResult result = formatBasicCode(programText, options, &registry, getGlobalPredefinedConstants());

    if (result.success && !result.formatted_code.empty()) {
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listLine(int line) {
    if (m_program.hasLine(line)) {
        std::string code = m_program.getLine(line);

        // Format single line
        std::string programText = std::to_string(line) + " " + code + "\n";

        FormatterOptions options;
        options.start_line = -1; // Don't renumber, just format
        options.step = 10;
        options.indent_spaces = 2;
        options.update_references = false;
        options.add_indentation = true;

        const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
        FormatterResult result = formatBasicCode(programText, options, &registry, getGlobalPredefinedConstants());

        if (result.success && !result.formatted_code.empty()) {
            std::cout << result.formatted_code;
        } else {
            printProgramLine(line, code);
        }
    } else {
        showError("Line " + std::to_string(line) + " not found");
    }
}

// Program compilation

bool ShellCore::compileToFile(const std::string& filename) {
    if (m_program.isEmpty()) {
        showError("No program to compile");
        return false;
    }

    // Generate program text
    std::string programText = m_program.generateProgram();

    try {
        // Lexical analysis
        if (m_verbose) {
            std::cout << "Lexing...\n";
        }

        Lexer lexer;
        lexer.tokenize(programText);
        auto tokens = lexer.getTokens();

        if (tokens.empty()) {
            showError("No tokens generated from program");
            return false;
        }

        // Parsing
        if (m_verbose) {
            std::cout << "Parsing...\n";
        }

        // Create semantic analyzer early to get ConstantsManager
        SemanticAnalyzer semantic;
        
        // Ensure constants are loaded before parsing (for fast constant lookup)
        semantic.ensureConstantsLoaded();

        Parser parser;
        parser.setConstantsManager(&semantic.getConstantsManager());
        auto ast = parser.parse(tokens, "<shell>");

        if (!ast || parser.hasErrors()) {
            showError("Parsing failed");
            for (const auto& error : parser.getErrors()) {
                std::cerr << "  " << error.toString() << "\n";
            }
            return false;
        }

        // Get compiler options
        const auto& compilerOptions = parser.getOptions();

        // Semantic analysis (semantic analyzer already created earlier)
        if (m_verbose) {
            std::cout << "Semantic analysis...\n";
        }

        #ifdef VOICE_CONTROLLER_ENABLED
        FBRunner3::VoiceRegistration::registerVoiceConstants(semantic.getConstantsManager());
        #endif

        semantic.analyze(*ast, compilerOptions);

        // Build control flow graph
        CFGBuilder cfgBuilder;
        auto cfg = cfgBuilder.build(*ast, semantic.getSymbolTable());

        // Generate IR
        if (m_verbose) {
            std::cout << "Generating IR...\n";
        }

        IRGenerator irGen;
        auto irCode = irGen.generate(*cfg, semantic.getSymbolTable());

        if (!irCode) {
            showError("Failed to generate IR code");
            return false;
        }

        // Generate Lua code
        if (m_verbose) {
            std::cout << "Generating Lua code...\n";
        }

        LuaCodeGenConfig config;
        config.emitComments = false;
        config.exitOnError = false;
        LuaCodeGenerator luaGen(config);
        std::string luaCode = luaGen.generate(*irCode);

        // Write to file
        std::ofstream outFile(filename);
        if (!outFile) {
            showError("Cannot write to file: " + filename);
            return false;
        }

        outFile << luaCode;
        outFile.close();

        if (m_verbose) {
            std::cout << "Successfully compiled to: " << filename << "\n";
        }

        return true;

    } catch (const std::exception& e) {
        showError(std::string("Compilation error: ") + e.what());
        return false;
    }
}

// Program execution

bool ShellCore::executeCompiledProgram(const std::string& program, int startLine) {
    try {
        // Start timing
        auto compileStartTime = std::chrono::high_resolution_clock::now();

        // Lexical analysis
        if (m_verbose) {
            std::cout << "Lexing...\n";
        }

        Lexer lexer;
        lexer.tokenize(program);
        auto tokens = lexer.getTokens();

        if (tokens.empty()) {
            showError("No tokens generated from program");
            return false;
        }

        // Parsing
        if (m_verbose) {
            std::cout << "Parsing...\n";
        }

        // Create semantic analyzer early to get ConstantsManager
        SemanticAnalyzer semantic;
        
        // Ensure constants are loaded before parsing (for fast constant lookup)
        semantic.ensureConstantsLoaded();

        Parser parser;
        parser.setConstantsManager(&semantic.getConstantsManager());
        auto ast = parser.parse(tokens, "<shell>");

        if (!ast || parser.hasErrors()) {
            showError("Parsing failed");
            for (const auto& error : parser.getErrors()) {
                std::cerr << "  " << error.toString() << "\n";
            }
            return false;
        }

        // Get compiler options
        const auto& compilerOptions = parser.getOptions();

        // Semantic analysis (semantic analyzer already created earlier)
        if (m_verbose) {
            std::cout << "Semantic analysis...\n";
        }

        // Register voice constants if voice controller is enabled
        #ifdef VOICE_CONTROLLER_ENABLED
        FBRunner3::VoiceRegistration::registerVoiceConstants(semantic.getConstantsManager());
        #endif

        semantic.analyze(*ast, compilerOptions);

        // Display warnings from semantic analysis
        const auto& warnings = semantic.getWarnings();
        if (!warnings.empty()) {
            for (const auto& warning : warnings) {
                std::cerr << "\nWARNING";
                if (warning.location.line > 0) {
                    std::cerr << " (line " << warning.location.line << ")";
                }
                std::cerr << ": " << warning.message << "\n";
            }
            std::cerr << "\n";
        }

        // Build control flow graph (not strictly needed for execution but follows fbc pattern)
        CFGBuilder cfgBuilder;
        auto cfg = cfgBuilder.build(*ast, semantic.getSymbolTable());

        // Generate IR
        if (m_verbose) {
            std::cout << "Generating IR...\n";
        }

        IRGenerator irGen;
        auto irCode = irGen.generate(*cfg, semantic.getSymbolTable());

        if (!irCode) {
            showError("Failed to generate IR code");
            return false;
        }

        // Generate Lua code
        if (m_verbose) {
            std::cout << "Generating Lua code...\n";
        }

        LuaCodeGenConfig config;
        config.emitComments = false;
        config.exitOnError = false;  // Don't exit on error in interactive shell
        LuaCodeGenerator luaGen(config);
        std::string luaCode = luaGen.generate(*irCode);

        // Always save generated Lua code for debugging
        {
            std::ofstream debugLua("/tmp/generated.lua");
            debugLua << luaCode;
            debugLua.close();
            if (m_verbose) {
                std::cout << "Generated Lua saved to /tmp/generated.lua\n";
            }
        }

        // Create Lua state
        lua_State* L = luaL_newstate();
        m_currentLuaState = L;  // Track for interruption
        g_shouldStopLua = false;  // Reset interruption flag
        if (!L) {
            showError("Cannot create Lua state");
            m_currentLuaState = nullptr;
            return false;
        }

        // Open standard libraries
        luaL_openlibs(L);
        
        // Register interruption check function
        lua_register(L, "check_should_stop", lua_check_should_stop);

        // Register runtime modules
        register_unicode_module(L);
        register_bitwise_module(L);
        register_constants_module(L);
        set_constants_manager(&semantic.getConstantsManager());

        FasterBASIC::register_fileio_functions(L);
        FasterBASIC::registerDataBindings(L);
        FasterBASIC::registerTerminalBindings(L);
        FasterBASIC::Terminal::registerTimerBindings(L);
        
        // Register plugin management commands (LOADPLUGINS, LOADPLUGIN)
        registerPluginLuaBindings(L);

        // Load plugin runtime files into Lua state (from cached contents)
        const auto& cachedRuntimes = FasterBASIC::PluginSystem::getGlobalPluginLoader().getCachedRuntimeContents();
        for (const auto& entry : cachedRuntimes) {
            const std::string& filename = entry.first;
            const std::string& content = entry.second;
            
            // Load the cached Lua code into the state
            if (luaL_loadbuffer(L, content.c_str(), content.size(), filename.c_str()) != 0 ||
                lua_pcall(L, 0, 0, 0) != 0) {
                std::cerr << "Warning: Failed to load plugin runtime: " << filename << std::endl;
                if (lua_isstring(L, -1)) {
                    std::cerr << "  Error: " << lua_tostring(L, -1) << std::endl;
                    lua_pop(L, 1);
                }
            }
        }

        // Register basic string conversion functions (CHR$, ASC, STR$, VAL, HEX$, BIN$, OCT$)
        // These are needed by generated code but not in the C++ bindings
        const char* string_funcs = R"(
            function CHR_STRING(code)
                return string.char(math.floor(code))
            end
            function chr(code)
                return string.char(math.floor(code))
            end
            function ASC(str)
                str = tostring(str or "")
                if #str == 0 then return 0 end
                return string.byte(str, 1)
            end
            function STR_STRING(num)
                return tostring(num)
            end
            function str(num)
                return tostring(num)
            end
            function VAL(str)
                return tonumber(tostring(str)) or 0
            end
            function HEX_STRING(num, digits)
                num = math.floor(num)
                digits = digits and math.floor(digits) or 0
                if num < 0 then num = num + 0x100000000 end
                local hex = string.format("%X", num)
                if digits > 0 and #hex < digits then
                    hex = string.rep("0", digits - #hex) .. hex
                end
                return hex
            end
            function BIN_STRING(num, digits)
                num = math.floor(num)
                digits = digits and math.floor(digits) or 0
                if num < 0 then num = num + 0x100000000 end
                local bin = ""
                local n = num
                if n == 0 then
                    bin = "0"
                else
                    while n > 0 do
                        bin = tostring(n % 2) .. bin
                        n = math.floor(n / 2)
                    end
                end
                if digits > 0 and #bin < digits then
                    bin = string.rep("0", digits - #bin) .. bin
                end
                return bin
            end
            function OCT_STRING(num, digits)
                num = math.floor(num)
                digits = digits and math.floor(digits) or 0
                if num < 0 then num = num + 0x100000000 end
                local oct = string.format("%o", num)
                if digits > 0 and #oct < digits then
                    oct = string.rep("0", digits - #oct) .. oct
                end
                return oct
            end
        )";
        if (luaL_dostring(L, string_funcs) != 0) {
            showError(std::string("Error loading string functions: ") + lua_tostring(L, -1));
            lua_close(L);
            return false;
        }

        // Register wait_frame and WAIT_FRAMES for terminal mode
        // In terminal mode, these just yield the coroutine
        // The event loop handles the actual timing via usleep
        const char* wait_frames_func = R"(
            function wait_frame()
                -- Check for interruption before yielding
                check_should_stop()
                -- In terminal mode, just yield - event loop handles timing
                if coroutine.running() then
                    coroutine.yield()
                end
            end
            
            function WAIT_FRAMES(n)
                -- In terminal mode, yield for n frames
                if coroutine.running() then
                    for i = 1, (n or 1) do
                        coroutine.yield()
                    end
                end
            end
            
            function wait_frames(n)
                WAIT_FRAMES(n)
            end
            
            function wait_ms(ms)
                -- Convert milliseconds to approximate frame count and yield
                if coroutine.running() then
                    local frames = math.max(1, math.floor((ms or 16.67) / 16.67))
                    for i = 1, frames do
                        coroutine.yield()
                    end
                end
            end
            
            function basic_sleep(seconds)
                -- Convert seconds to milliseconds and yield
                if coroutine.running() then
                    local ms = seconds * 1000
                    local frames = math.max(1, math.floor(ms / 16.67))
                    for i = 1, frames do
                        coroutine.yield()
                    end
                end
            end
            
            function sleep(seconds)
                -- Alias for compatibility
                basic_sleep(seconds)
            end
        )";
        if (luaL_dostring(L, wait_frames_func) != 0) {
            showError(std::string("Error loading wait frame functions: ") + lua_tostring(L, -1));
            lua_close(L);
            return false;
        }

        // Register voice bindings if available (terminal-only, no GUI)
        #ifdef VOICE_CONTROLLER_ENABLED
        if (m_verbose) {
            fprintf(stderr, "Registering voice Lua bindings\n");
            fflush(stderr);
        }
        FBRunner3::VoiceRegistration::registerVoiceLuaBindings(L);
        if (m_verbose) {
            fprintf(stderr, "Voice Lua bindings registered\n");
            fflush(stderr);
        }
        #endif

        // Register additional Lua bindings if set (e.g., fbsh_voices-specific functions)
        if (g_additionalLuaBindings) {
            g_additionalLuaBindings(L);
        }

        // Initialize DATA segment
        if (!irCode->dataValues.empty()) {
            FasterBASIC::initializeDataManager(irCode->dataValues);

            for (const auto& entry : irCode->dataLineRestorePoints) {
                FasterBASIC::addDataRestorePoint(entry.first, entry.second);
            }

            for (const auto& entry : irCode->dataLabelRestorePoints) {
                FasterBASIC::addDataRestorePointByLabel(entry.first, entry.second);
            }
        }

        // Execute the program with event loop support
        auto startTime = std::chrono::high_resolution_clock::now();

        // Initialize timer system
        lua_getglobal(L, "basic_timer_init");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 1, 0) != 0) {
                showError(std::string("Timer init error: ") + lua_tostring(L, -1));
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1); // Pop result
            }
        } else {
            lua_pop(L, 1); // Pop non-function
        }

        // Wrap program in coroutine with event loop
        const char* event_loop_wrapper = R"(
            -- Load the program code into a coroutine
            local program_func = ...
            local main_coro = coroutine.create(program_func)
            
            -- Get FFI for sleep
            local ffi = require("ffi")
            ffi.cdef[[
                int usleep(unsigned int usec);
            ]]
            
            -- Start timing using os.clock() which measures CPU time
            local start_time = os.clock()
            local program_finished = false
            
            -- Run event loop
            local running = true
            while running do
                -- Process any pending timer events
                if basic_timer_check then
                    basic_timer_check()
                end
                
                -- Resume the main coroutine
                if coroutine.status(main_coro) ~= "dead" then
                    local success, err = coroutine.resume(main_coro)
                    if not success then
                        error("Program error: " .. tostring(err))
                    end
                else
                    -- Main program finished - record time immediately
                    if not program_finished then
                        local end_time = os.clock()
                        _G._program_execution_time_us = math.floor((end_time - start_time) * 1000000)
                        program_finished = true
                    end
                    running = false
                end
                
                -- Sleep for one frame (~16.67ms at 60 FPS)
                ffi.C.usleep(16670)
            end
            
            -- Shutdown timer system
            if basic_timer_shutdown then
                basic_timer_shutdown()
            end
        )";

        // Load the program code
        if (luaL_loadstring(L, luaCode.c_str()) != 0) {
            showError(std::string("Error loading Lua code: ") + lua_tostring(L, -1));
            lua_close(L);
            return false;
        }
        
        // Load and execute the event loop wrapper with the program as argument
        if (luaL_loadstring(L, event_loop_wrapper) != 0) {
            showError(std::string("Error loading event loop: ") + lua_tostring(L, -1));
            lua_close(L);
            return false;
        }
        
        // Push the program function as argument to event loop
        lua_pushvalue(L, -2); // Copy program function
        
        // Execute event loop with program
        int result = lua_pcall(L, 1, 0, 0);

        // Clear the Lua state tracking
        m_currentLuaState = nullptr;

        // Check for errors BEFORE closing Lua state
        if (result != 0) {
            std::string errorMsg = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Unknown error";
            lua_close(L);
            
            // Don't show error if it was a user interruption
            if (errorMsg.find("interrupted by user") == std::string::npos) {
                showError(std::string("Execution error: ") + errorMsg);
            }
            return false;
        }

        // Get actual program execution time from Lua (excluding event loop overhead)
        long long totalUs = 0;
        lua_getglobal(L, "_program_execution_time_us");
        if (lua_isnumber(L, -1)) {
            totalUs = static_cast<long long>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        // Clean up Lua state
        lua_close(L);

        // Show timing
        // Format time in human-friendly way
        long long totalMs = totalUs / 1000;

        std::cout << "\nTime taken: ";

        if (totalMs < 10) {
            // Less than 10ms (1 centisecond) - show milliseconds
            std::cout << totalMs << "ms\n";
        } else if (totalMs < 1000) {
            // Less than 1 second - show centiseconds
            long long centiseconds = totalMs / 10;
            std::cout << centiseconds << "cs\n";
        } else {
            // 1 second or more - show seconds with hundredths
            long long minutes = totalMs / 60000;
            long long seconds = (totalMs % 60000) / 1000;
            long long hundredths = (totalMs % 1000) / 10;

            if (minutes > 0) {
                std::cout << minutes << "m ";
            }
            std::cout << seconds << "." << std::setfill('0') << std::setw(2) << hundredths << "s\n";
        }

        std::cout << "Ready.\n";
        return true;

    } catch (const std::exception& e) {
        showError(std::string("Execution error: ") + e.what());
        return false;
    }
}

// File operations

bool ShellCore::loadProgram(const std::string& filename) {
    std::string fullFilename = addExtensionIfNeeded(filename);
    fullFilename = resolveFilePath(fullFilename);

    if (!fileExists(fullFilename)) {
        showError("File not found: " + fullFilename);
        return false;
    }

    std::string content = readFileContent(fullFilename);
    if (content.empty()) {
        showError("Failed to read file or file is empty: " + fullFilename);
        return false;
    }

    // Check if current program is modified and warn user
    if (!m_program.isEmpty() && m_program.isModified()) {
        std::cout << "Warning: Current program has unsaved changes.\n";
        std::cout << "Continue loading? (Y/N): ";
        std::string response = readInput();
        if (response.empty() || (response[0] != 'Y' && response[0] != 'y')) {
            showMessage("Load cancelled");
            return false;
        }
    }

    // Parse the file content and load into program manager
    m_program.clear();

    std::istringstream iss(content);
    std::string line;
    int lineCount = 0;
    int errorCount = 0;
    int autoLineNum = 1000; // Start auto-numbering at 1000

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        // Parse line number and code
        std::istringstream lineStream(line);
        int lineNum;
        if (lineStream >> lineNum) {
            // Line has a number
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            if (!code.empty()) {
                m_program.setLine(lineNum, code);
                lineCount++;
            }
        } else {
            // Line has no number - auto-add one
            // Trim leading/trailing whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                size_t end = line.find_last_not_of(" \t\r\n");
                std::string code = line.substr(start, end - start + 1);
                if (!code.empty()) {
                    m_program.setLine(autoLineNum, code);
                    autoLineNum += 10;
                    lineCount++;
                }
            }
        }
    }

    m_program.setFilename(fullFilename);
    m_program.setModified(false);

    std::string message = "Loaded " + std::to_string(lineCount) + " lines from " + fullFilename;
    if (errorCount > 0) {
        message += " (" + std::to_string(errorCount) + " lines skipped)";
    }
    showSuccess(message);
    return true;
}

bool ShellCore::saveProgram(const std::string& filename) {
    if (m_program.isEmpty()) {
        showError("No program to save");
        return false;
    }

    std::string fullFilename = addExtensionIfNeeded(filename);
    fullFilename = resolveFilePath(fullFilename);

    // Check if file exists and warn user
    if (fileExists(fullFilename)) {
        std::cout << "File '" << fullFilename << "' already exists.\n";
        std::cout << "Overwrite? (Y/N): ";
        std::string response = readInput();
        if (response.empty() || (response[0] != 'Y' && response[0] != 'y')) {
            showMessage("Save cancelled");
            return false;
        }
    }

    // Get formatted program content
    std::string content = m_program.generateProgram();

    // Optionally format before saving
    if (m_verbose) {
        FormatterOptions options;
        options.start_line = -1; // Don't renumber, just format
        options.step = 10;
        options.indent_spaces = 2;
        options.update_references = false;
        options.add_indentation = true;

        const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
        FormatterResult result = formatBasicCode(content, options, &registry, getGlobalPredefinedConstants());
        if (result.success && !result.formatted_code.empty()) {
            content = result.formatted_code;
        }
    }

    if (writeFileContent(fullFilename, content)) {
        m_program.setFilename(fullFilename);
        m_program.setModified(false);
        auto stats = m_program.getStatistics();
        showSuccess("Program saved to " + fullFilename + " (" + std::to_string(stats.lineCount) + " lines, " + std::to_string(stats.totalCharacters) + " chars)");
        return true;
    } else {
        showError("Failed to save program to " + fullFilename);
        return false;
    }
}

bool ShellCore::mergeProgram(const std::string& filename) {
    std::string fullFilename = addExtensionIfNeeded(filename);

    if (!fileExists(fullFilename)) {
        showError("File not found: " + fullFilename);
        return false;
    }

    std::string content = readFileContent(fullFilename);
    if (content.empty()) {
        showError("Failed to read file or file is empty: " + fullFilename);
        return false;
    }

    // Parse the file content and merge into current program
    std::istringstream iss(content);
    std::string line;
    int lineCount = 0;
    int replacedCount = 0;
    int errorCount = 0;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        // Parse line number and code
        std::istringstream lineStream(line);
        int lineNum;
        if (lineStream >> lineNum) {
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            if (!code.empty()) {
                if (m_program.hasLine(lineNum)) {
                    replacedCount++;
                }
                m_program.setLine(lineNum, code);
                lineCount++;
            }
        } else {
            errorCount++;
            if (m_verbose) {
                showError("Skipped invalid line: " + line);
            }
        }
    }

    std::string message = "Merged " + std::to_string(lineCount) + " lines from " + fullFilename;
    if (replacedCount > 0) {
        message += " (" + std::to_string(replacedCount) + " lines replaced)";
    }
    if (errorCount > 0) {
        message += " (" + std::to_string(errorCount) + " lines skipped)";
    }
    showSuccess(message);
    return true;
}

void ShellCore::newProgram() {
    m_program.clear();
    showMessage("Program cleared");
}

// Development tools

bool ShellCore::checkSyntax() {
    if (m_program.isEmpty()) {
        showMessage("No program to check");
        return false;
    }
    
    // Get program text
    std::string programText = m_program.generateProgram();
    
    try {
        // Tokenize
        FasterBASIC::Lexer lexer;
        if (!lexer.tokenize(programText)) {
            std::cout << "Lexer errors:\n";
            for (const auto& error : lexer.getErrors()) {
                std::cout << "  " << error.toString() << "\n";
            }
            return false;
        }
        
        // Parse
        const auto& tokens = lexer.getTokens();
        
        // Create semantic analyzer early to get ConstantsManager
        FasterBASIC::SemanticAnalyzer semantic;
        
        // Ensure constants are loaded before parsing (for fast constant lookup)
        semantic.ensureConstantsLoaded();
        
        FasterBASIC::Parser parser;
        parser.setConstantsManager(&semantic.getConstantsManager());
        auto ast = parser.parse(tokens);
        
        if (parser.hasErrors()) {
            std::cout << "Parser errors:\n";
            const auto& lineMapping = parser.getLineNumberMapping();
            for (const auto& error : parser.getErrors()) {
                const int* basicLineNum = lineMapping.getBasicLineNumber(error.location.line);
                if (basicLineNum) {
                    std::cout << "  Line " << *basicLineNum << ": " << error.what() << "\n";
                } else {
                    std::cout << "  " << error.toString() << "\n";
                }
            }
            return false;
        }
        
        // Semantic analysis
        FasterBASIC::SemanticAnalyzer analyzer;
        FasterBASIC::CompilerOptions options = parser.getOptions();
        analyzer.analyze(*ast, options);
        
        const auto& lineMapping = parser.getLineNumberMapping();
        bool hasErrors = false;
        
        // Show errors
        if (analyzer.hasErrors()) {
            std::cout << "Errors:\n";
            for (const auto& error : analyzer.getErrors()) {
                // DEBUG: Show what we're looking up
                std::cerr << "[DEBUG] Error at source line " << error.location.line << std::endl;
                const int* basicLineNum = lineMapping.getBasicLineNumber(error.location.line);
                if (basicLineNum) {
                    std::cerr << "[DEBUG] Mapped to BASIC line " << *basicLineNum << std::endl;
                    std::cout << "  Line " << *basicLineNum << ": " << error.message << "\n";
                } else {
                    std::cerr << "[DEBUG] No mapping found, using source line" << std::endl;
                    std::cout << "  Line " << error.location.line << ": " << error.message << "\n";
                }
            }
            hasErrors = true;
        }
        
        // Show warnings
        const auto& warnings = analyzer.getWarnings();
        if (!warnings.empty()) {
            std::cout << "Warnings:\n";
            for (const auto& warning : warnings) {
                // DEBUG: Show what we're looking up
                std::cerr << "[DEBUG] Warning at source line " << warning.location.line << std::endl;
                const int* basicLineNum = lineMapping.getBasicLineNumber(warning.location.line);
                if (basicLineNum) {
                    std::cerr << "[DEBUG] Mapped to BASIC line " << *basicLineNum << std::endl;
                    std::cout << "  Line " << *basicLineNum << ": " << warning.message << "\n";
                } else {
                    std::cerr << "[DEBUG] No mapping found, using source line" << std::endl;
                    std::cout << "  Line " << warning.location.line << ": " << warning.message << "\n";
                }
            }
        }
        
        if (!hasErrors && warnings.empty()) {
            showMessage("No errors or warnings found");
        }
        
        return !hasErrors;
        
    } catch (const std::exception& e) {
        std::cout << "Error during syntax check: " << e.what() << "\n";
        return false;
    }
}

bool ShellCore::formatProgram() {
    if (m_program.isEmpty()) {
        showError("No program to format");
        return false;
    }

    // Get current program
    std::string programText = m_program.generateProgram();

    // Use the basic formatter to format and renumber the program
    FormatterOptions options;
    options.start_line = 10;
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = true;
    options.add_indentation = true;

    const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    FormatterResult result = formatBasicCode(programText, options, &registry, getGlobalPredefinedConstants());

    if (!result.success || result.formatted_code.empty()) {
        showError("Failed to format program: " + result.error_message);
        return false;
    }

    // Clear current program and reload the formatted version
    m_program.clear();

    // Parse the formatted program back into the program manager
    std::istringstream iss(result.formatted_code);
    std::string line;
    int lineCount = 0;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Parse line number and code
        std::istringstream lineStream(line);
        int lineNum;
        if (lineStream >> lineNum) {
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            m_program.setLine(lineNum, code);
            lineCount++;
        }
    }

    showSuccess("Program formatted and renumbered (" + std::to_string(lineCount) + " lines)");
    return true;
}

void ShellCore::showVariables() {
    showMessage("Variable display not yet implemented");
}

void ShellCore::clearVariables() {
    showMessage("Variable clearing not yet implemented");
}

// Information and help

void ShellCore::showHelp() {
    std::cout << "\nFasterBASIC Shell Commands:\n";
    std::cout << "===========================\n";
    std::cout << "\nProgram Entry:\n";
    std::cout << "  10 PRINT \"Hello\"   Add or replace line 10\n";
    std::cout << "  10               Delete line 10\n";
    std::cout << "  Note: Use up/down arrow to navigate, <ESC> to return\n";
    std::cout << "\nListing:\n";
    std::cout << "  LIST             List entire program\n";
    std::cout << "  LIST 10          List line 10\n";
    std::cout << "  LIST 10-50       List lines 10 through 50\n";
    std::cout << "  LIST 10-         List from line 10 to end\n";
    std::cout << "  LIST -50         List from start to line 50\n";
    std::cout << "\nExecution:\n";
    std::cout << "  RUN              Run program from beginning\n";
    std::cout << "  RUN 100          Run program starting from line 100\n";
    std::cout << "\nFile Operations:\n";
    std::cout << "  NEW              Clear program from memory\n";
    std::cout << "  LOAD \"file\"       Load program from file\n";
    std::cout << "  SAVE \"file\"       Save program to file\n";
    std::cout << "  DIR              List .bas files in current directory\n";
    std::cout << "\nProgram Management:\n";
    std::cout << "  AUTO             Enable auto line numbering\n";
    std::cout << "  AUTO 1000,10     Auto numbering starting at 1000, step 10\n";
    std::cout << "  RENUM            Renumber program (start=10, step=10)\n";
    std::cout << "  RENUM 100,5      Renumber starting at 100, step 5\n";
    std::cout << "  EDIT 100         Edit line 100 with full line editor\n";
    std::cout << "  EDITOR           Enter full screen editor, Ctrl+Q (twice) to return\n";
    std::cout << "  FORMAT           Format and indent program with proper structure\n";
    std::cout << "  FIND \"text\"       Find first occurrence of text\n";
    std::cout << "  FIND text,5      Find text with 5 context lines\n";
    std::cout << "  FINDNEXT         Find next occurrence of last search\n";
    std::cout << "  REPLACE \"new\"     Replace found text with new text\n";
    std::cout << "  REPLACENEXT \"new\" Replace and find next occurrence\n";
    std::cout << "\nOther:\n";
    std::cout << "  CLS              Clear screen\n";
    std::cout << "  HELP             Show this help\n";
    std::cout << "  HELP <category>  Show commands in a category\n";
    std::cout << "  HELP <command>   Show detailed help for a command\n";
    std::cout << "  HELP SEARCH <query>  Search help database\n";
    std::cout << "  HELP TAG <tag>   Show commands with specific tag\n";
    std::cout << "  HELP REBUILD     Rebuild help database\n";
    std::cout << "  HELP ARTICLES    List all tutorial articles\n";
    std::cout << "  HELP ARTICLE <name>  Show specific article\n";
    std::cout << "  QUIT/EXIT/BYE    Exit shell\n";
    std::cout << "\n";

    // Show available BASIC command categories
    showHelpCategories();
}

void ShellCore::showHelpCategories() {
    using namespace FasterBASIC::ModularCommands;
    auto& registry = getGlobalCommandRegistry();
    auto categories = registry.getCategories();

    if (categories.empty()) {
        return;
    }

    std::cout << "BASIC Command Categories:\n";
    std::cout << "========================\n";
    std::cout << "Type HELP <category> to see commands in that category:\n\n";

    // Sort categories alphabetically
    std::sort(categories.begin(), categories.end());

    // Get category descriptions
    std::unordered_map<std::string, std::string> categoryDesc = {
        {"audio", "Music and sound playback"},
        {"cart", "Cart/cartridge system"},
        {"circle", "Circle ID system"},
        {"control", "Control flow commands"},
        {"data", "Data storage commands"},
        {"file", "File I/O operations"},
        {"graphics", "Graphics primitives"},
        {"input", "Keyboard and mouse input"},
        {"line", "Line ID system"},
        {"math", "Mathematical functions"},
        {"particle", "Particle effects"},
        {"rectangle", "Rectangle ID system"},
        {"sprite", "Sprite management"},
        {"string", "String manipulation"},
        {"system", "System utilities"},
        {"text", "Text display and manipulation"},
        {"tilemap", "Tilemap operations"},
        {"voice", "Voice synthesis"}
    };

    for (const auto& cat : categories) {
        std::string catUpper = cat;
        std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);

        std::string desc = "Commands";
        auto it = categoryDesc.find(cat);
        if (it != categoryDesc.end()) {
            desc = it->second;
        }

        std::cout << "  " << std::setw(12) << std::left << catUpper << " - " << desc << "\n";
    }

    std::cout << "\n";
}

void ShellCore::showHelpForTopicOrCommand(const std::string& topic) {
    using namespace FasterBASIC::ModularCommands;
    auto& registry = getGlobalCommandRegistry();

    // Convert to uppercase for comparison
    std::string topicUpper = topic;
    std::transform(topicUpper.begin(), topicUpper.end(), topicUpper.begin(), ::toupper);

    // Check if it's a category
    std::string topicLower = topic;
    std::transform(topicLower.begin(), topicLower.end(), topicLower.begin(), ::tolower);

    auto categories = registry.getCategories();
    bool isCategory = std::find(categories.begin(), categories.end(), topicLower) != categories.end();

    if (isCategory) {
        showHelpForCategory(topicLower);
        return;
    }

    // Check if it's a command or function
    const auto* cmd = registry.getCommandOrFunction(topicUpper);
    if (cmd) {
        showHelpForCommand(cmd);
        return;
    }

    // Not found - show error and suggestions
    std::cout << "\nUnknown command or category: " << topic << "\n\n";

    // Try to find partial matches
    std::vector<std::string> matches;
    auto allCommands = registry.getAllNames();
    for (const auto& cmdName : allCommands) {
        if (cmdName.find(topicUpper) != std::string::npos) {
            matches.push_back(cmdName);
        }
    }

    if (!matches.empty()) {
        std::cout << "Did you mean one of these commands?\n";
        for (const auto& match : matches) {
            const auto* matchCmd = registry.getCommandOrFunction(match);
            if (matchCmd) {
                std::cout << "  " << std::setw(25) << std::left << match
                         << matchCmd->description << "\n";
            }
        }
        std::cout << "\n";
    }

    std::cout << "Type HELP to see all categories\n";
    std::cout << "Type HELP <category> to see commands in a category\n";
    std::cout << "\n";
}

void ShellCore::showHelpForCategory(const std::string& category) {
    using namespace FasterBASIC::ModularCommands;
    auto& registry = getGlobalCommandRegistry();

    auto commands = registry.getCommandsByCategory(category);
    auto functions = registry.getFunctionsByCategory(category);

    if (commands.empty() && functions.empty()) {
        std::cout << "\nNo commands found in category: " << category << "\n\n";
        return;
    }

    std::string catUpper = category;
    std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);

    std::cout << "\n" << catUpper << " Commands\n";
    std::cout << std::string(catUpper.length() + 9, '=') << "\n\n";

    // Display commands
    if (!commands.empty()) {
        std::cout << "Commands:\n";
        for (const auto& cmdName : commands) {
            const auto* cmd = registry.getCommand(cmdName);
            if (cmd) {
                std::string signature = formatCommandSignature(cmd);
                std::cout << "  " << std::setw(40) << std::left << signature
                         << cmd->description << "\n";
            }
        }
        std::cout << "\n";
    }

    // Display functions
    if (!functions.empty()) {
        std::cout << "Functions:\n";
        for (const auto& funcName : functions) {
            const auto* func = registry.getFunction(funcName);
            if (func) {
                std::string signature = formatFunctionSignature(func);
                std::cout << "  " << std::setw(40) << std::left << signature
                         << func->description << "\n";
            }
        }
        std::cout << "\n";
    }

    std::cout << "Type HELP <command> for detailed help on a specific command\n\n";
}

void ShellCore::showHelpForCommand(const FasterBASIC::ModularCommands::CommandDefinition* cmd) {
    using namespace FasterBASIC::ModularCommands;

    // Header
    std::cout << "\n" << cmd->commandName << " - " << cmd->description << "\n";
    std::cout << std::string(cmd->commandName.length() + cmd->description.length() + 3, '=') << "\n\n";

    // Category
    std::string catUpper = cmd->category;
    std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);
    std::cout << "Category: " << catUpper << "\n\n";

    // Syntax
    std::cout << "Syntax:\n";
    if (cmd->isFunction) {
        std::cout << "  result = " << formatFunctionSignature(cmd) << "\n\n";
    } else {
        std::cout << "  " << formatCommandSignature(cmd) << "\n\n";
    }

    // Parameters
    if (!cmd->parameters.empty()) {
        std::cout << "Parameters:\n";
        for (const auto& param : cmd->parameters) {
            std::cout << "  " << param.name << " ("
                     << parameterTypeToString(param.type);
            if (param.isOptional) {
                std::cout << ", optional";
                if (!param.defaultValue.empty()) {
                    std::cout << ", default: " << param.defaultValue;
                }
            } else {
                std::cout << ", required";
            }
            std::cout << ")\n";

            if (!param.description.empty()) {
                std::cout << "    " << param.description << "\n";
            }
            std::cout << "\n";
        }
    }

    // Return type for functions
    if (cmd->isFunction && cmd->returnType != ReturnType::VOID) {
        std::cout << "Returns:\n";
        std::cout << "  " << returnTypeToString(cmd->returnType) << "\n\n";
    }

    // See also
    std::cout << "See Also:\n";
    std::cout << "  HELP " << catUpper << " for all " << cmd->category << " commands\n\n";
}

std::string ShellCore::formatCommandSignature(const FasterBASIC::ModularCommands::CommandDefinition* cmd) {
    std::ostringstream oss;
    oss << cmd->commandName;

    for (size_t i = 0; i < cmd->parameters.size(); ++i) {
        const auto& param = cmd->parameters[i];
        if (i == 0) oss << " ";
        else oss << ", ";

        if (param.isOptional) oss << "[";
        oss << param.name;
        if (param.isOptional) oss << "]";
    }

    return oss.str();
}

std::string ShellCore::formatFunctionSignature(const FasterBASIC::ModularCommands::CommandDefinition* func) {
    std::ostringstream oss;
    oss << func->commandName << "(";

    for (size_t i = 0; i < func->parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        const auto& param = func->parameters[i];
        if (param.isOptional) oss << "[";
        oss << param.name;
        if (param.isOptional) oss << "]";
    }

    oss << ")";
    return oss.str();
}

void ShellCore::showVersion() {
    std::cout << "FasterBASIC Shell v" << SHELL_VERSION << std::endl;
}

void ShellCore::showStatistics() {
    auto stats = m_program.getStatistics();
    std::cout << "\nProgram Statistics:\n";
    std::cout << "==================\n";
    std::cout << "Lines: " << stats.lineCount << std::endl;
    std::cout << "Characters: " << stats.totalCharacters << std::endl;
    if (stats.lineCount > 0) {
        std::cout << "Range: " << stats.minLineNumber << "-" << stats.maxLineNumber << std::endl;
        std::cout << "Gaps in numbering: " << (stats.hasGaps ? "Yes" : "No") << std::endl;
    }
    std::cout << "Modified: " << (m_program.isModified() ? "Yes" : "No") << std::endl;
    if (m_program.hasFilename()) {
        std::cout << "File: " << m_program.getFilename() << std::endl;
    }
    std::cout << std::endl;
}

std::string ShellCore::editLineInteractive(int lineNumber, const std::string& initialContent) {
    std::string buffer = initialContent;
    size_t cursorPos = buffer.length();  // Start cursor at end
    bool done = false;

    // Save current terminal settings
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;

    // Enable raw mode for character-by-character input
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // Display initial content
    std::cout << lineNumber << " " << buffer;
    std::cout.flush();

    while (!done) {
        char ch = std::cin.get();

        if (ch == '\n' || ch == '\r') {
            // Enter - accept changes
            done = true;
            std::cout << std::endl;

            // Save the current line before returning
            if (!buffer.empty()) {
                m_program.setLine(lineNumber, buffer);
            }
        } else if (ch == '\x1B') {  // ESC key
            // Check for arrow key sequences
            if (std::cin.peek() == '[') {
                std::cin.get(); // consume '['
                char seq2 = std::cin.get();
                switch (seq2) {
                    case 'A':  // Up arrow - move to previous line if it exists
                        {
                            // First, save current line content
                            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                            std::cout << std::endl;

                            if (!buffer.empty()) {
                                m_program.setLine(lineNumber, buffer);
                            }

                            // Find previous line
                            int prevLine = m_program.getPreviousLineNumber(lineNumber);
                            if (prevLine > 0) {
                                // Previous line exists, edit it
                                std::string prevContent = m_program.getLine(prevLine);

                                // Recursively edit the previous line
                                editLineInteractive(prevLine, prevContent);
                                return "\x1B";  // Return ESC to signal already handled
                            }
                            // If no previous line, just continue editing current line
                            tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case 'B':  // Down arrow - move to next line or create it
                        {
                            // First, save current line content
                            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                            std::cout << std::endl;

                            if (!buffer.empty()) {
                                m_program.setLine(lineNumber, buffer);
                            }

                            // Find or create next line
                            int nextLine = m_program.getNextLineNumber(lineNumber);
                            if (nextLine == -1) {
                                // No next line exists, create one
                                nextLine = findNextAvailableLineNumber(lineNumber);
                            }

                            // Get content of next line (empty if newly created)
                            std::string nextContent = m_program.getLine(nextLine);

                            // Recursively edit the next line
                            editLineInteractive(nextLine, nextContent);
                            return "\x1B";  // Return ESC to signal already handled
                        }
                        break;
                    case 'C':  // Right arrow
                        if (cursorPos < buffer.length()) {
                            cursorPos++;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case 'D':  // Left arrow
                        if (cursorPos > 0) {
                            cursorPos--;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case '1':  // Ctrl+Arrow sequences start with 1;5
                        if (std::cin.peek() == ';') {
                            std::cin.get();  // consume ';'
                            if (std::cin.peek() == '5') {
                                std::cin.get();  // consume '5'
                                char direction = std::cin.get();
                                switch (direction) {
                                    case 'C':  // Ctrl+Right - move word forward
                                        cursorPos = findNextWord(buffer, cursorPos);
                                        redrawLine(lineNumber, buffer, cursorPos);
                                        break;
                                    case 'D':  // Ctrl+Left - move word backward
                                        cursorPos = findPrevWord(buffer, cursorPos);
                                        redrawLine(lineNumber, buffer, cursorPos);
                                        break;
                                }
                            }
                        } else if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            cursorPos = 0;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case 'H':  // Home key
                        cursorPos = 0;
                        redrawLine(lineNumber, buffer, cursorPos);
                        break;
                    case 'F':  // End key
                        cursorPos = buffer.length();
                        redrawLine(lineNumber, buffer, cursorPos);
                        break;

                    case '4':  // End key (alternative sequence)
                        if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            cursorPos = buffer.length();
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case '7':  // Home key (alternative sequence)
                        if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            cursorPos = 0;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case '3':  // Delete key (may have ~ after)
                        if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            if (cursorPos < buffer.length()) {
                                buffer.erase(cursorPos, 1);
                                redrawLine(lineNumber, buffer, cursorPos);
                            }
                        }
                        break;

                }
            } else {
                // Single ESC - cancel edit
                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                return "\x1B";  // Return ESC to indicate cancel
            }
        } else if (ch == '\x7F' || ch == '\b') {  // Backspace
            if (cursorPos > 0) {
                buffer.erase(cursorPos - 1, 1);
                cursorPos--;
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x03') {  // Ctrl+C
            // Cancel edit
            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
            return "\x1B";  // Return ESC to indicate cancel
        } else if (ch == '\x01') {  // Ctrl+A - move to beginning of line
            cursorPos = 0;
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch == '\x05') {  // Ctrl+E - move to end of line
            cursorPos = buffer.length();
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch == '\x0B') {  // Ctrl+K - delete from cursor to end
            if (cursorPos < buffer.length()) {
                clearToEndOfLine(buffer, cursorPos);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x15') {  // Ctrl+U - delete from beginning to cursor
            if (cursorPos > 0) {
                clearToStartOfLine(buffer, cursorPos);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x17') {  // Ctrl+W - delete word backward
            if (cursorPos > 0) {
                deleteWordBackward(buffer, cursorPos);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x04') {  // Ctrl+D - delete character at cursor (like Del key)
            if (cursorPos < buffer.length()) {
                buffer.erase(cursorPos, 1);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x0C') {  // Ctrl+L - refresh display
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch == '\x09') {  // Tab - insert spaces or complete keyword
            // Insert 4 spaces for tab (BASIC convention)
            for (int i = 0; i < 4; i++) {
                buffer.insert(cursorPos, 1, ' ');
                cursorPos++;
            }
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            buffer.insert(cursorPos, 1, ch);
            cursorPos++;
            redrawLine(lineNumber, buffer, cursorPos);
        }
        // Ignore other control characters
    }

    // Restore normal terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    return buffer;
}

size_t ShellCore::findWordStart(const std::string& buffer, size_t pos) {
    if (pos > buffer.length()) pos = buffer.length();
    while (pos > 0 && (buffer[pos-1] == ' ' || buffer[pos-1] == '\t')) {
        pos--;
    }
    while (pos > 0 && buffer[pos-1] != ' ' && buffer[pos-1] != '\t') {
        pos--;
    }
    return pos;
}

size_t ShellCore::findWordEnd(const std::string& buffer, size_t pos) {
    while (pos < buffer.length() && buffer[pos] != ' ' && buffer[pos] != '\t') {
        pos++;
    }
    return pos;
}

size_t ShellCore::findNextWord(const std::string& buffer, size_t pos) {
    // Skip current word
    while (pos < buffer.length() && buffer[pos] != ' ' && buffer[pos] != '\t') {
        pos++;
    }
    // Skip whitespace
    while (pos < buffer.length() && (buffer[pos] == ' ' || buffer[pos] == '\t')) {
        pos++;
    }
    return pos;
}

size_t ShellCore::findPrevWord(const std::string& buffer, size_t pos) {
    // Skip whitespace backward
    while (pos > 0 && (buffer[pos-1] == ' ' || buffer[pos-1] == '\t')) {
        pos--;
    }
    // Skip word backward
    while (pos > 0 && buffer[pos-1] != ' ' && buffer[pos-1] != '\t') {
        pos--;
    }
    return pos;
}

void ShellCore::clearToEndOfLine(std::string& buffer, size_t pos) {
    buffer.erase(pos);
}

void ShellCore::clearToStartOfLine(std::string& buffer, size_t& pos) {
    buffer.erase(0, pos);
    pos = 0;
}

void ShellCore::deleteWordBackward(std::string& buffer, size_t& pos) {
    size_t wordStart = findPrevWord(buffer, pos);
    buffer.erase(wordStart, pos - wordStart);
    pos = wordStart;
}

void ShellCore::redrawLine(int lineNumber, const std::string& buffer, size_t cursorPos) {
    // Clear current line and redraw
    std::cout << "\r\x1B[K";  // Move to start and clear line
    std::cout << lineNumber << " " << buffer;

    // Position cursor correctly
    if (cursorPos < buffer.length()) {
        size_t moveBack = buffer.length() - cursorPos;
        std::cout << "\x1B[" << moveBack << "D";  // Move cursor left
    }

    std::cout.flush();
}

void ShellCore::showSearchResult(int foundLine, const std::string& foundContent, int contextLines) {
    std::vector<int> lineNumbers = m_program.getLineNumbers();

    // Find the position of foundLine in the sorted list
    auto it = std::find(lineNumbers.begin(), lineNumbers.end(), foundLine);
    if (it == lineNumbers.end()) {
        return;  // Line not found (shouldn't happen)
    }

    int foundIndex = std::distance(lineNumbers.begin(), it);

    // Calculate range to display
    int startIndex = std::max(0, foundIndex - contextLines);
    int endIndex = std::min(static_cast<int>(lineNumbers.size()) - 1, foundIndex + contextLines);

    std::cout << "\nFound \"" << m_lastSearchText << "\" at line " << foundLine << ":\n\n";

    // Display context lines
    for (int i = startIndex; i <= endIndex; i++) {
        int lineNum = lineNumbers[i];
        std::string content = m_program.getLine(lineNum);

        if (lineNum == foundLine) {
            // Highlight the found line
            std::cout << ">>> " << lineNum << " " << content << "\n";
        } else {
            std::cout << "    " << lineNum << " " << content << "\n";
        }
    }

    std::cout << "\n";
}

int ShellCore::findNextAvailableLineNumber(int currentLine) {
    // Find the next line number that would make sense for continuation
    // Look for a gap of at least 10 or suggest currentLine + 10
    int suggested = currentLine + 10;

    // Check if there's a line close to our suggestion
    for (int step = 10; step <= 100; step += 10) {
        int candidate = currentLine + step;
        if (!m_program.hasLine(candidate)) {
            return candidate;
        }
    }

    // If all nearby multiples of 10 are taken, just suggest +10 anyway
    // The user can always override
    return suggested;
}

int ShellCore::findPreviousLineNumber(int currentLine) {
    // Find the previous line number that exists in the program
    std::vector<int> lineNumbers = m_program.getLineNumbers();

    // Find the largest line number that's less than currentLine
    int previous = -1;
    for (int lineNum : lineNumbers) {
        if (lineNum < currentLine && lineNum > previous) {
            previous = lineNum;
        }
    }

    return (previous > 0) ? previous : -1;
}

std::string ShellCore::readInputWithInlineEditing() {
    std::string buffer = "";
    size_t cursorPos = 0;
    bool done = false;

    // Enable raw mode for character input
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // Show initial prompt
    std::cout << m_suggestedNextLine << " ";
    std::cout.flush();

    while (!done) {
        char ch = std::cin.get();

        if (ch == '\n' || ch == '\r') {
            // Enter - finish input
            done = true;
            std::cout << std::endl;
        } else if (ch == '\x1B') {  // ESC or arrow keys
            if (std::cin.peek() == '[') {
                std::cin.get(); // consume '['
                char seq = std::cin.get();
                switch (seq) {
                    case 'A':  // Up arrow - edit previous line
                        {
                            // Save current line if not empty
                            if (!buffer.empty()) {
                                std::string formattedBuffer = m_parser.formatBasicKeywords(buffer);
                                m_program.setLine(m_suggestedNextLine, formattedBuffer);
                            }

                            // Find previous line relative to the last entered line
                            int prevLine = findPreviousLineNumber(m_lastLineNumber);
                            if (prevLine > 0) {
                                // Get previous line content
                                std::string prevContent = m_program.getLine(prevLine);

                                // Switch to editing the previous line
                                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                                std::cout << "\r\x1B[K"; // Clear current line
                                std::cout << "Editing line " << prevLine << ":\n";

                                // Edit the previous line and save result
                                std::string editedContent = editLineInteractive(prevLine, prevContent);
                                if (editedContent != "\x1B") { // Not cancelled
                                    m_program.setLine(prevLine, editedContent);
                                }

                                // Return to continuation mode with next line after the edited one
                                m_suggestedNextLine = findNextAvailableLineNumber(prevLine);
                                m_autoContinueMode = true;
                                return "";
                            }
                        }
                        break;
                    case 'C':  // Right arrow
                        if (cursorPos < buffer.length()) {
                            cursorPos++;
                            std::cout << "\x1B[C";
                            std::cout.flush();
                        }
                        break;
                    case 'D':  // Left arrow
                        if (cursorPos > 0) {
                            cursorPos--;
                            std::cout << "\x1B[D";
                            std::cout.flush();
                        }
                        break;
                    default:
                        // Ignore other arrow keys
                        break;
                }
            } else {
                // Single ESC - cancel
                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                std::cout << "\r\x1B[K"; // Clear line
                return "";
            }
        } else if (ch == '\x7F' || ch == '\b') {  // Backspace
            if (cursorPos > 0) {
                buffer.erase(cursorPos - 1, 1);
                cursorPos--;
                // Redraw line
                std::cout << "\r" << m_suggestedNextLine << " " << buffer;
                // Position cursor
                for (size_t i = 0; i < buffer.length() - cursorPos; i++) {
                    std::cout << "\x1B[D";
                }
                std::cout << " \x1B[D"; // Clear extra character
                std::cout.flush();
            }
        } else if (ch == '\x03') {  // Ctrl+C
            // Cancel
            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
            std::cout << "\r\x1B[K"; // Clear line
            return "";
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            buffer.insert(cursorPos, 1, ch);
            cursorPos++;
            // Redraw line
            std::cout << "\r" << m_suggestedNextLine << " " << buffer;
            // Position cursor
            for (size_t i = 0; i < buffer.length() - cursorPos; i++) {
                std::cout << "\x1B[D";
            }
            std::cout.flush();
        }
    }

    // Restore terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    return buffer;
}




// Configuration

void ShellCore::setVerbose(bool verbose) {
    m_verbose = verbose;
}

void ShellCore::setDebug(bool debug) {
    m_debug = debug;
}

bool ShellCore::isVerbose() const {
    return m_verbose;
}

bool ShellCore::isDebug() const {
    return m_debug;
}

// Utility functions

std::string ShellCore::generateTempFilename() {
    static int counter = 0;
    return TEMP_FILE_PREFIX + std::to_string(counter++) + ".bas";
}

bool ShellCore::fileExists(const std::string& filename) const {
    std::ifstream file(filename);
    return file.good();
}

std::string ShellCore::readFileContent(const std::string& filename) const {
    std::ifstream file(filename);
    if (!file) return "";

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

bool ShellCore::writeFileContent(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file) return false;

    file << content;
    return file.good();
}

void ShellCore::showError(const std::string& error) {
    std::cout << "Error: " << error << std::endl;
}

void ShellCore::showMessage(const std::string& message) {
    std::cout << message << std::endl;
}

void ShellCore::showSuccess(const std::string& message) {
    std::cout << message << std::endl;
}

std::string ShellCore::getDefaultExtension(const std::string& filename) const {
    return ".bas";
}

std::string ShellCore::addExtensionIfNeeded(const std::string& filename) const {
    if (filename.find('.') == std::string::npos) {
        return filename + getDefaultExtension(filename);
    }
    return filename;
}

void ShellCore::printProgramLine(int lineNumber, const std::string& code) {
    std::cout << lineNumber << " " << code << std::endl;
}

void ShellCore::printHeader(const std::string& title) {
    std::cout << "\n" << title << "\n";
    std::cout << std::string(title.length(), '=') << "\n";
}

void ShellCore::printSeparator() {
    std::cout << std::string(40, '-') << "\n";
}

// BASIC directory helpers
std::string ShellCore::getBasicScriptsDir() const {
    const char* home = getenv("HOME");
    if (!home) {
        return "./";
    }
    return std::string(home) + "/SuperTerminal/BASIC/";
}

std::string ShellCore::getBasicLibDir() const {
    return getBasicScriptsDir() + "lib/";
}

void ShellCore::ensureBasicDirectories() const {
    std::string scriptsDir = getBasicScriptsDir();
    std::string libDir = getBasicLibDir();

    // Create directories if they don't exist
    mkdir(scriptsDir.c_str(), 0755);
    mkdir(libDir.c_str(), 0755);
}

std::string ShellCore::resolveFilePath(const std::string& filename) const {
    // If it's an absolute path, use as-is
    if (!filename.empty() && filename[0] == '/') {
        return filename;
    }

    // If it's a relative path with directory components, check if it exists as-is first
    if (filename.find('/') != std::string::npos) {
        if (fileExists(filename)) {
            return filename;
        }
        // If not found, continue to check other locations
    }

    // Check current directory first (for files without path separators)
    if (fileExists(filename)) {
        return filename;
    }

    // Check in BASIC scripts directory
    std::string scriptsPath = getBasicScriptsDir() + filename;
    if (fileExists(scriptsPath)) {
        return scriptsPath;
    }

    // Then check in lib directory
    std::string libPath = getBasicLibDir() + filename;
    if (fileExists(libPath)) {
        return libPath;
    }

    // Default to current directory for new files (if no path separator)
    // or the original path (if it has path separators)
    if (filename.find('/') != std::string::npos) {
        return filename;
    }
    return scriptsPath;
}

// =============================================================================
// Help Database Handlers
// =============================================================================

void ShellCore::handleHelpRebuild() {
    using namespace FasterBASIC::HelpSystem;
    
    std::cout << "\nRebuilding help database...\n";
    std::cout << "===========================\n\n";
    
    auto& helpDB = getGlobalHelpDatabase();
    
    // Set up progress callback
    helpDB.setRebuildProgressCallback([](const std::string& message, int current, int total) {
        if (total > 0) {
            std::cout << "  [" << current << "/" << total << "] " << message << "\n";
        } else {
            std::cout << "  " << message << "\n";
        }
    });
    
    // Perform rebuild
    bool success = helpDB.rebuild();
    
    if (success) {
        auto stats = helpDB.getStatistics();
        std::cout << "\nHelp database rebuilt successfully!\n";
        std::cout << "-----------------------------------\n";
        std::cout << "  Commands:  " << stats.commandCount << "\n";
        std::cout << "  Functions: " << stats.functionCount << "\n";
        std::cout << "  Articles:  " << stats.articleCount << "\n";
        std::cout << "  Tags:      " << stats.tagCount << "\n";
        std::cout << "  Version:   " << stats.schemaVersion << "\n";
        std::cout << "\n";
    } else {
        showError("Failed to rebuild help database: " + helpDB.getLastError());
    }
}

void ShellCore::handleHelpSearch(const std::string& query) {
    using namespace FasterBASIC::HelpSystem;
    
    std::cout << "\nSearching for: " << query << "\n";
    std::cout << std::string(15 + query.length(), '=') << "\n\n";
    
    auto& helpDB = getGlobalHelpDatabase();
    
    if (!helpDB.isOpen()) {
        showError("Help database is not available");
        std::cout << "Run HELP REBUILD to initialize the help database.\n\n";
        return;
    }
    
    auto results = helpDB.search(query, 20);
    
    if (results.empty()) {
        std::cout << "No results found for \"" << query << "\"\n\n";
        
        // Try to find similar commands
        auto similar = helpDB.findSimilarCommands(query, 5);
        if (!similar.empty()) {
            std::cout << "Did you mean one of these?\n";
            for (const auto& cmd : similar) {
                std::cout << "  " << cmd << "\n";
            }
            std::cout << "\n";
        }
        return;
    }
    
    std::cout << "Found " << results.size() << " result(s):\n\n";
    
    for (const auto& result : results) {
        std::string typeLabel = result.type;
        std::transform(typeLabel.begin(), typeLabel.end(), typeLabel.begin(), ::toupper);
        
        std::cout << "  [" << typeLabel << "] " << result.name << "\n";
        std::cout << "    " << result.description << "\n";
        
        if (!result.category.empty()) {
            std::cout << "    Category: " << result.category << "\n";
        }
        
        if (!result.excerpt.empty()) {
            std::cout << "    " << result.excerpt << "\n";
        }
        
        std::cout << "\n";
    }
    
    std::cout << "Use HELP <command> to see detailed help for a specific command.\n";
    std::cout << "Use HELP ARTICLE <name> to read a full article.\n\n";
}

void ShellCore::handleHelpTag(const std::string& tag) {
    using namespace FasterBASIC::HelpSystem;
    
    auto& helpDB = getGlobalHelpDatabase();
    
    if (!helpDB.isOpen()) {
        showError("Help database is not available");
        std::cout << "Run HELP REBUILD to initialize the help database.\n\n";
        return;
    }
    
    // If no tag specified, list all available tags
    if (tag.empty()) {
        auto tags = helpDB.getAllTags();
        
        if (tags.empty()) {
            std::cout << "\nNo tags available.\n\n";
            return;
        }
        
        std::cout << "\nAvailable Tags:\n";
        std::cout << "===============\n\n";
        
        // Sort tags alphabetically
        std::sort(tags.begin(), tags.end());
        
        // Display in columns
        int col = 0;
        for (const auto& t : tags) {
            std::cout << std::setw(20) << std::left << t;
            col++;
            if (col >= 3) {
                std::cout << "\n";
                col = 0;
            }
        }
        if (col > 0) std::cout << "\n";
        
        std::cout << "\nUse HELP TAG <tagname> to see items with that tag.\n\n";
        return;
    }
    
    // Search by tag
    std::cout << "\nItems tagged with: " << tag << "\n";
    std::cout << std::string(19 + tag.length(), '=') << "\n\n";
    
    auto results = helpDB.searchByTag(tag);
    
    if (results.empty()) {
        std::cout << "No items found with tag \"" << tag << "\"\n\n";
        return;
    }
    
    std::cout << "Found " << results.size() << " item(s):\n\n";
    
    // Group by type
    std::vector<SearchResult> commands, functions, articles;
    for (const auto& result : results) {
        if (result.type == "command") {
            commands.push_back(result);
        } else if (result.type == "function") {
            functions.push_back(result);
        } else if (result.type == "article") {
            articles.push_back(result);
        }
    }
    
    if (!commands.empty()) {
        std::cout << "Commands:\n";
        for (const auto& cmd : commands) {
            std::cout << "  " << std::setw(25) << std::left << cmd.name 
                     << cmd.description << "\n";
        }
        std::cout << "\n";
    }
    
    if (!functions.empty()) {
        std::cout << "Functions:\n";
        for (const auto& fn : functions) {
            std::cout << "  " << std::setw(25) << std::left << fn.name 
                     << fn.description << "\n";
        }
        std::cout << "\n";
    }
    
    if (!articles.empty()) {
        std::cout << "Articles:\n";
        for (const auto& art : articles) {
            std::cout << "  " << std::setw(25) << std::left << art.name 
                     << art.description << "\n";
        }
        std::cout << "\n";
    }
}

void ShellCore::handleHelpArticles() {
    using namespace FasterBASIC::HelpSystem;
    
    auto& helpDB = getGlobalHelpDatabase();
    
    if (!helpDB.isOpen()) {
        showError("Help database is not available");
        std::cout << "Run HELP REBUILD to initialize the help database.\n\n";
        return;
    }
    
    auto articles = helpDB.getAllArticleNames();
    
    if (articles.empty()) {
        std::cout << "\nNo articles available.\n";
        std::cout << "Articles provide tutorials and in-depth explanations of concepts.\n\n";
        return;
    }
    
    std::cout << "\nAvailable Articles:\n";
    std::cout << "===================\n\n";
    
    // Get full article info for each
    for (const auto& name : articles) {
        auto article = helpDB.getArticleHelp(name);
        if (article) {
            std::cout << "  " << std::setw(30) << std::left << article->title;
            
            if (!article->difficulty.empty()) {
                std::cout << " [" << article->difficulty << "]";
            }
            
            if (article->estimatedTime > 0) {
                std::cout << " (" << article->estimatedTime << " min)";
            }
            
            std::cout << "\n";
            
            if (!article->category.empty()) {
                std::cout << "    Category: " << article->category << "\n";
            }
        }
    }
    
    std::cout << "\nUse HELP ARTICLE <name> to read a full article.\n\n";
}

void ShellCore::handleHelpArticle(const std::string& articleName) {
    using namespace FasterBASIC::HelpSystem;
    
    auto& helpDB = getGlobalHelpDatabase();
    
    if (!helpDB.isOpen()) {
        showError("Help database is not available");
        std::cout << "Run HELP REBUILD to initialize the help database.\n\n";
        return;
    }
    
    auto article = helpDB.getArticleHelp(articleName);
    
    if (!article) {
        std::cout << "\nArticle not found: " << articleName << "\n\n";
        
        // Try to find similar article names
        auto allArticles = helpDB.getAllArticleNames();
        std::vector<std::string> similar;
        
        std::string lowerQuery = articleName;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        
        for (const auto& name : allArticles) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find(lowerQuery) != std::string::npos) {
                similar.push_back(name);
            }
        }
        
        if (!similar.empty()) {
            std::cout << "Did you mean one of these?\n";
            for (const auto& name : similar) {
                std::cout << "  " << name << "\n";
            }
            std::cout << "\n";
        }
        
        return;
    }
    
    // Display article
    std::cout << "\n" << article->title << "\n";
    std::cout << std::string(article->title.length(), '=') << "\n\n";
    
    // Metadata
    if (!article->author.empty()) {
        std::cout << "Author: " << article->author << "\n";
    }
    
    if (!article->difficulty.empty()) {
        std::cout << "Difficulty: " << article->difficulty << "\n";
    }
    
    if (article->estimatedTime > 0) {
        std::cout << "Estimated Time: " << article->estimatedTime << " minutes\n";
    }
    
    if (!article->category.empty()) {
        std::cout << "Category: " << article->category << "\n";
    }
    
    if (!article->tags.empty()) {
        std::cout << "Tags: ";
        for (size_t i = 0; i < article->tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << article->tags[i];
        }
        std::cout << "\n";
    }
    
    std::cout << "\n";
    
    // Content
    std::cout << article->content << "\n\n";
    
    // Related commands
    if (!article->relatedCommands.empty()) {
        std::cout << "Related Commands:\n";
        std::cout << "-----------------\n";
        for (const auto& cmd : article->relatedCommands) {
            std::cout << "  " << cmd << "\n";
        }
        std::cout << "\n";
        std::cout << "Use HELP <command> for detailed command documentation.\n\n";
    }
}

} // namespace FasterBASIC
