//
// screen_editor.cpp
// FasterBASIC Shell - Screen Editor Implementation
//
// Full-screen editor for BASIC programs
//

#include "screen_editor.h"
#include "program_manager_v2.h"
#include "../src/basic_formatter_lib.h"
#include "../src/fasterbasic_lexer.h"
#include "../src/fasterbasic_parser.h"
#include "../src/fasterbasic_semantic.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// External flag for terminal resize detection (set by signal handler)
extern volatile bool g_terminalResized;

namespace FasterBASIC {

ScreenEditor::ScreenEditor(ProgramManagerV2& program, const FasterBASIC::ModularCommands::CommandRegistry* registry)
    : m_program(program)
    , m_terminal(g_terminal)
    , m_registry(registry)
    , m_cursorLine(0)
    , m_cursorCol(0)
    , m_topLine(0)
    , m_screenWidth(80)
    , m_screenHeight(24)
    , m_visibleLines(23)
    , m_modified(false)
    , m_quit(false)
    , m_statusIsError(false)
    , m_syntaxHighlighter()
    , m_syntaxHighlightingEnabled(true)
    , m_formattingEnabled(true)
    , m_shouldRun(false)
    , m_showErrors(true)
    , m_totalErrors(0)
    , m_totalWarnings(0)
{
    // Get actual terminal size
    auto size = m_terminal.getScreenSize();
    m_screenWidth = size.first;
    m_screenHeight = size.second;
    m_visibleLines = m_screenHeight - STATUS_LINE_HEIGHT;
    
    rebuildLineCache();
}

ScreenEditor::~ScreenEditor() {
    m_terminal.disableRawMode();
    m_terminal.showCursor();
    m_terminal.resetColors();
}

bool ScreenEditor::run() {
    // Setup terminal
    m_terminal.clearScreen();
    m_terminal.enableRawMode();
    m_terminal.showCursor();
    
    m_quit = false;
    m_modified = false;
    m_shouldRun = false;
    m_statusMessage = "Screen Editor - Ctrl+Q:Quit Ctrl+S:Save Ctrl+R:Run Ctrl+K:Kill Ctrl+D:Dup Ctrl+F:Format";
    
    // Initial draw
    rebuildLineCache();
    
    // Ensure we have at least one line and cursor is valid
    if (m_lineNumbers.empty()) {
        showMessage("Error: Could not initialize editor", true);
        m_quit = true;
        return false;
    }
    
    // Ensure cursor position is valid
    m_cursorLine = 0;
    m_cursorCol = 0;
    
    redraw();
    
    // Main editor loop
    while (!m_quit) {
        // Check for terminal resize
        if (g_terminalResized) {
            g_terminalResized = false;
            
            // Update terminal size
            auto size = m_terminal.getScreenSize();
            m_screenWidth = size.first;
            m_screenHeight = size.second;
            m_visibleLines = m_screenHeight - STATUS_LINE_HEIGHT;
            
            // DEBUG: Show resize detected
            std::cerr << "\n\rDEBUG: Resize detected! New size: " << m_screenWidth << "x" << m_screenHeight << "\n\r";
            std::cerr.flush();
            
            // Adjust scroll position if needed
            rebuildLineCache();
            int totalLines = m_lineNumbers.size();
            if (m_topLine + m_visibleLines > totalLines) {
                m_topLine = (totalLines > m_visibleLines) 
                    ? totalLines - m_visibleLines 
                    : 0;
            }
            
            // Full redraw
            m_terminal.clearScreen();
            redraw();
        }
        
        char ch = m_terminal.waitForKey();
        
        // If waitForKey was interrupted by a signal (returns 0), 
        // loop back to check for resize
        if (ch == 0) {
            std::cerr << "\n\rDEBUG: waitForKey returned 0 (signal)\n\r";
            std::cerr.flush();
            continue;
        }
        
        // Handle control characters
        if (ch == 17) { // Ctrl+Q
            if (m_modified) {
                showMessage("Program modified. Ctrl+S to save, Ctrl+Q again to quit without saving", true);
                char confirm = m_terminal.waitForKey();
                if (confirm == 17) { // Ctrl+Q again
                    m_quit = true;
                }
            } else {
                m_quit = true;
            }
            continue;
        } else if (ch == 19) { // Ctrl+S
            if (saveProgram()) {
                showMessage("Program saved successfully");
                m_modified = false;
            } else {
                showMessage("Failed to save program", true);
            }
            redraw();
            continue;
        } else if (ch == 11) { // Ctrl+K - Kill line
            killLine();
            m_modified = true;
            redraw();
            continue;
        } else if (ch == 4) { // Ctrl+D - Duplicate line
            duplicateLine();
            m_modified = true;
            redraw();
            continue;
        } else if (ch == 12) { // Ctrl+L - List
            listLines();
            redraw();
            continue;
        } else if (ch == 6) { // Ctrl+F - Format
            formatProgram();
            redraw();
            continue;
        } else if (ch == 18) { // Ctrl+R - Run
            runProgram();
            continue;

        } else if (ch == 27) { // ESC - might be escape sequence
            if (handleEscapeSequence()) {
                redraw();
            }
            continue;
        }
        
        // Handle regular key press
        if (handleKeyPress(ch)) {
            redraw();
        }
    }
    
    // Cleanup
    m_terminal.disableRawMode();
    m_terminal.clearScreen();
    m_terminal.locate(1, 1);
    
    return m_modified;
}

void ScreenEditor::redraw() {
    m_terminal.clearScreen();
    
    // Draw all visible lines
    int lineCount = getLineCount();
    for (int i = 0; i < m_visibleLines && (m_topLine + i) < lineCount; i++) {
        int editorLine = m_topLine + i;
        int lineNum = getActualLineNumber(editorLine);
        bool isCurrent = (editorLine == m_cursorLine);
        
        // Position cursor at start of this screen line
        m_terminal.locate(1, i + 1);
        
        // Draw line number
        drawLineNumber(lineNum, isCurrent);
        
        // Get line text
        std::string lineText = "";
        if (editorLine < lineCount) {
            lineText = m_program.getLine(lineNum);
        }
        
        // Draw line text with syntax highlighting
        int maxTextWidth = m_screenWidth - LINE_NUM_WIDTH;
        renderLineWithSyntaxHighlighting(lineText, maxTextWidth);
    }
    
    // Draw status line
    drawStatusLine();
    
    // Position cursor
    updateCursorPosition();
    m_terminal.flush();
}

void ScreenEditor::drawStatusLine() {
    m_terminal.locate(1, m_screenHeight);
    
    // Draw status line with inverted colors
    m_terminal.setForegroundColor(TerminalColor::BLACK);
    m_terminal.setBackgroundColor(TerminalColor::WHITE);
    
    // Build status text
    std::ostringstream status;
    status << " Line:" << (m_cursorLine + 1) << "/" << getLineCount() 
           << " Col:" << (m_cursorCol + 1);
    
    if (m_modified) {
        status << " [Modified]";
    }
    
    if (!m_program.getFilename().empty()) {
        status << " " << m_program.getFilename();
    }
    
    // Pad to screen width
    std::string statusText = status.str();
    if (statusText.length() < m_screenWidth) {
        statusText.append(m_screenWidth - statusText.length(), ' ');
    } else if (statusText.length() > m_screenWidth) {
        statusText = statusText.substr(0, m_screenWidth);
    }
    
    m_terminal.print(statusText, false);
    
    // Reset colors
    m_terminal.resetColors();
    
    // Show message if any
    if (!m_statusMessage.empty()) {
        m_terminal.locate(1, m_screenHeight - 1);
        if (m_statusIsError) {
            m_terminal.setForegroundColor(TerminalColor::BRIGHT_RED);
        } else {
            m_terminal.setForegroundColor(TerminalColor::BRIGHT_GREEN);
        }
        
        std::string msg = " " + m_statusMessage;
        if (msg.length() > m_screenWidth) {
            msg = msg.substr(0, m_screenWidth);
        }
        m_terminal.print(msg, false);
        m_terminal.resetColors();
    }
}

void ScreenEditor::drawLineNumber(int lineNum, bool current) {
    if (current) {
        m_terminal.setForegroundColor(TerminalColor::BRIGHT_YELLOW);
    } else {
        m_terminal.setForegroundColor(TerminalColor::CYAN);
    }
    
    std::ostringstream oss;
    oss << lineNum << ": ";
    std::string numStr = oss.str();
    
    // Pad to LINE_NUM_WIDTH
    if (numStr.length() < LINE_NUM_WIDTH) {
        numStr.insert(0, LINE_NUM_WIDTH - numStr.length(), ' ');
    }
    
    m_terminal.print(numStr, false);
    m_terminal.resetColors();
}

void ScreenEditor::updateCursorPosition() {
    // Calculate screen position
    int screenLine = m_cursorLine - m_topLine + 1;
    int screenCol = LINE_NUM_WIDTH + m_cursorCol + 1;
    
    // Clamp to screen bounds
    if (screenLine < 1) screenLine = 1;
    if (screenLine > m_visibleLines) screenLine = m_visibleLines;
    if (screenCol < LINE_NUM_WIDTH + 1) screenCol = LINE_NUM_WIDTH + 1;
    if (screenCol > m_screenWidth) screenCol = m_screenWidth;
    
    m_terminal.locate(screenCol, screenLine);
}

void ScreenEditor::showMessage(const std::string& msg, bool isError) {
    m_statusMessage = msg;
    m_statusIsError = isError;
}

// Navigation functions
void ScreenEditor::moveCursorUp() {
    if (m_cursorLine > 0) {
        m_cursorLine--;
        ensureCursorVisible();
        
        // Clamp column to line length
        std::string lineText = getCurrentLineText();
        if (m_cursorCol > static_cast<int>(lineText.length())) {
            m_cursorCol = lineText.length();
        }
    }
}

void ScreenEditor::moveCursorDown() {
    if (m_cursorLine < getLineCount() - 1) {
        m_cursorLine++;
        ensureCursorVisible();
        
        // Clamp column to line length
        std::string lineText = getCurrentLineText();
        if (m_cursorCol > static_cast<int>(lineText.length())) {
            m_cursorCol = lineText.length();
        }
    }
}

void ScreenEditor::moveCursorLeft() {
    if (m_cursorCol > 0) {
        m_cursorCol--;
    } else if (m_cursorLine > 0) {
        // Move to end of previous line
        m_cursorLine--;
        std::string lineText = getCurrentLineText();
        m_cursorCol = lineText.length();
        ensureCursorVisible();
    }
}

void ScreenEditor::moveCursorRight() {
    std::string lineText = getCurrentLineText();
    if (m_cursorCol < static_cast<int>(lineText.length())) {
        m_cursorCol++;
    } else if (m_cursorLine < getLineCount() - 1) {
        // Move to start of next line
        m_cursorLine++;
        m_cursorCol = 0;
        ensureCursorVisible();
    }
}

void ScreenEditor::moveCursorToLineStart() {
    m_cursorCol = 0;
}

void ScreenEditor::moveCursorToLineEnd() {
    std::string lineText = getCurrentLineText();
    m_cursorCol = lineText.length();
}

void ScreenEditor::pageUp() {
    m_cursorLine -= m_visibleLines;
    if (m_cursorLine < 0) {
        m_cursorLine = 0;
    }
    m_topLine = m_cursorLine;
    
    std::string lineText = getCurrentLineText();
    if (m_cursorCol > static_cast<int>(lineText.length())) {
        m_cursorCol = lineText.length();
    }
}

void ScreenEditor::pageDown() {
    m_cursorLine += m_visibleLines;
    int maxLine = getLineCount() - 1;
    if (m_cursorLine > maxLine) {
        m_cursorLine = maxLine;
    }
    if (m_cursorLine < 0) {
        m_cursorLine = 0;
    }
    ensureCursorVisible();
    
    std::string lineText = getCurrentLineText();
    if (m_cursorCol > static_cast<int>(lineText.length())) {
        m_cursorCol = lineText.length();
    }
}

void ScreenEditor::gotoLine(int lineNum) {
    int index = getEditorLineIndex(lineNum);
    if (index >= 0 && index < getLineCount()) {
        m_cursorLine = index;
        m_cursorCol = 0;
        ensureCursorVisible();
    }
}

// Editing functions
void ScreenEditor::insertChar(char ch) {
    std::string lineText = getCurrentLineText();
    
    // Ensure cursor is within bounds
    if (m_cursorCol > static_cast<int>(lineText.length())) {
        m_cursorCol = lineText.length();
    }
    
    // Safely insert character
    if (m_cursorCol >= 0 && m_cursorCol <= static_cast<int>(lineText.length())) {
        lineText.insert(m_cursorCol, 1, ch);
        setCurrentLineText(lineText);
        m_cursorCol++;
        m_modified = true;
    }
}

void ScreenEditor::deleteChar() {
    std::string lineText = getCurrentLineText();
    if (m_cursorCol >= 0 && m_cursorCol < static_cast<int>(lineText.length())) {
        lineText.erase(m_cursorCol, 1);
        setCurrentLineText(lineText);
        m_modified = true;
    } else if (m_cursorLine < getLineCount() - 1) {
        // Join with next line
        joinWithNextLine();
    }
}

void ScreenEditor::backspace() {
    if (m_cursorCol > 0) {
        m_cursorCol--;
        deleteChar();
    } else if (m_cursorLine > 0) {
        // Move to end of previous line and join
        int prevLine = m_cursorLine - 1;
        std::string prevText = "";
        int prevLineNum = getActualLineNumber(prevLine);
        prevText = m_program.getLine(prevLineNum);
        
        m_cursorCol = prevText.length();
        m_cursorLine = prevLine;
        joinWithNextLine();
    }
}

void ScreenEditor::insertNewLine() {
    int currentLineNum = getActualLineNumber(m_cursorLine);
    
    // Create new blank line below current line with auto-numbering
    int newLineNum = currentLineNum + 10;
    
    // Make sure we don't collide with next line
    if (m_cursorLine < getLineCount() - 1) {
        int nextLineNum = getActualLineNumber(m_cursorLine + 1);
        if (newLineNum >= nextLineNum) {
            // Find a gap or renumber
            newLineNum = (currentLineNum + nextLineNum) / 2;
            if (newLineNum == currentLineNum) {
                // Need to renumber - for now just use currentLineNum + 1
                newLineNum = currentLineNum + 1;
            }
        }
    }
    
    // Insert blank line
    m_program.setLineRaw(newLineNum, "");
    
    // Move cursor to new line at start
    m_cursorLine++;
    m_cursorCol = 0;
    
    rebuildLineCache();
    ensureCursorVisible();
    m_modified = true;
}

void ScreenEditor::killLine() {
    if (getLineCount() == 0) return;
    
    int lineNum = getActualLineNumber(m_cursorLine);
    m_program.deleteLine(lineNum);
    
    rebuildLineCache();
    
    // Adjust cursor position
    if (m_cursorLine >= getLineCount() && m_cursorLine > 0) {
        m_cursorLine--;
    }
    m_cursorCol = 0;
    
    showMessage("Line deleted");
}

void ScreenEditor::duplicateLine() {
    if (getLineCount() == 0) return;
    
    int currentLineNum = getActualLineNumber(m_cursorLine);
    std::string lineText = getCurrentLineText();
    
    // Find line number for duplicate
    int newLineNum = currentLineNum + 10;
    if (m_cursorLine < getLineCount() - 1) {
        int nextLineNum = getActualLineNumber(m_cursorLine + 1);
        if (newLineNum >= nextLineNum) {
            newLineNum = (currentLineNum + nextLineNum) / 2;
            if (newLineNum == currentLineNum) {
                newLineNum = currentLineNum + 1;
            }
        }
    }
    
    m_program.setLineRaw(newLineNum, lineText);
    
    rebuildLineCache();
    m_cursorLine++;
    ensureCursorVisible();
    
    showMessage("Line duplicated");
}

void ScreenEditor::joinWithNextLine() {
    if (m_cursorLine >= getLineCount() - 1) return;
    
    int currentLineNum = getActualLineNumber(m_cursorLine);
    int nextLineNum = getActualLineNumber(m_cursorLine + 1);
    
    std::string currentText = getCurrentLineText();
    std::string nextText = m_program.getLine(nextLineNum);
    
    // Join texts
    std::string joined = currentText + nextText;
    setCurrentLineText(joined);
    
    // Delete next line
    m_program.deleteLine(nextLineNum);
    
    rebuildLineCache();
    m_modified = true;
}

// File operations
// Format entire program
void ScreenEditor::formatProgram() {
    if (m_program.isEmpty()) {
        showMessage("No program to format", true);
        return;
    }
    
    showMessage("Formatting program...", false);
    m_terminal.flush();
    
    // Get current program
    std::string programText = m_program.generateProgram();
    
    // Format with indentation
    FormatterOptions options;
    options.start_line = -1;        // Keep existing line numbers
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;  // Don't renumber references
    options.add_indentation = true;     // Add indentation
    
    FormatterResult result = formatBasicCode(programText, options, m_registry, getGlobalPredefinedConstants());
    
    if (!result.success || result.formatted_code.empty()) {
        showMessage("Format failed: " + result.error_message, true);
        return;
    }
    
    // Save filename before clearing
    std::string savedFilename = m_program.getFilename();
    
    // Parse the formatted program back into the program manager
    m_program.clear();
    std::istringstream iss(result.formatted_code);
    std::string line;
    int lineCount = 0;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        // Strip any ANSI escape codes that might have leaked in
        std::string cleanLine;
        bool inEscape = false;
        for (char ch : line) {
            if (ch == '\033') {  // ESC character
                inEscape = true;
            } else if (inEscape && ch == 'm') {
                inEscape = false;
            } else if (!inEscape) {
                cleanLine += ch;
            }
        }
        
        // Extract line number and code
        std::istringstream lineStream(cleanLine);
        int lineNum;
        if (lineStream >> lineNum) {
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            if (!code.empty() || cleanLine.find_first_not_of(" \t") != std::string::npos) {
                m_program.setLineRaw(lineNum, code);
                lineCount++;
            }
        }
    }
    
    // Restore filename after formatting
    if (!savedFilename.empty()) {
        m_program.setFilename(savedFilename);
    }
    
    // Rebuild cache and reposition cursor
    rebuildLineCache();
    if (m_cursorLine >= getLineCount()) {
        m_cursorLine = getLineCount() - 1;
    }
    if (m_cursorLine < 0) {
        m_cursorLine = 0;
    }
    m_cursorCol = 0;
    ensureCursorVisible();
    
    m_modified = true;
    showMessage("Program formatted (" + std::to_string(lineCount) + " lines)");
}

void ScreenEditor::runProgram() {
    // Set flag to indicate program should be run
    m_shouldRun = true;
    
    // Save if modified
    if (m_modified) {
        showMessage("Saving before run...", false);
        m_terminal.flush();
        if (!saveProgram()) {
            showMessage("Save failed - run cancelled", true);
            m_shouldRun = false;
            return;
        }
    }
    
    // Exit editor to run
    m_quit = true;
}

bool ScreenEditor::saveProgram() {
    std::string filename = m_program.getFilename();
    
    // Prompt for filename if not set
    if (filename.empty()) {
        showMessage("Enter filename to save: ", false);
        m_terminal.locate(1, m_screenHeight - 1);
        m_terminal.setForegroundColor(TerminalColor::BRIGHT_GREEN);
        m_terminal.print(" Enter filename to save: ", false);
        m_terminal.resetColors();
        m_terminal.flush();
        
        // Read filename from user
        m_terminal.disableRawMode();
        std::string input;
        std::getline(std::cin, input);
        m_terminal.enableRawMode();
        
        // Trim whitespace
        size_t start = input.find_first_not_of(" \t\r\n");
        size_t end = input.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            filename = input.substr(start, end - start + 1);
        } else {
            filename = input;
        }
        
        // Check if user cancelled
        if (filename.empty()) {
            showMessage("Save cancelled", true);
            return false;
        }
        
        // Add .bas extension if not present
        if (filename.find('.') == std::string::npos) {
            filename += ".bas";
        }
    }
    
    // Save using REPLView
    REPLView& replView = m_program.getREPLView();
    if (replView.save(filename)) {
        m_program.setFilename(filename);
        m_program.setModified(false);
        showMessage("Saved to " + filename);
        return true;
    } else {
        showMessage("Failed to save " + filename, true);
        return false;
    }
}

void ScreenEditor::listLines() {
    // Simple implementation - just show a message
    // In a full implementation, this could show a dialog to jump to a line range
    std::ostringstream oss;
    oss << "Program has " << getLineCount() << " lines";
    if (getLineCount() > 0) {
        oss << " (line " << getActualLineNumber(0) << " to " 
            << getActualLineNumber(getLineCount() - 1) << ")";
    }
    showMessage(oss.str());
}

// Input handling
bool ScreenEditor::handleKeyPress(char ch) {
    // Clear status message on regular keypress
    m_statusMessage.clear();
    
    if (ch == 127 || ch == 8) { // Backspace
        backspace();
        return true;
    } else if (ch == 13 || ch == 10) { // Enter
        std::cerr << "DEBUG: Enter pressed, calling insertNewLine()" << std::endl;
        std::cerr << "DEBUG: Current line: " << m_cursorLine << ", lineCount: " << getLineCount() << std::endl;
        insertNewLine();
        std::cerr << "DEBUG: After insertNewLine, line: " << m_cursorLine << ", lineCount: " << getLineCount() << std::endl;
        return true;
    } else if (ch >= 32 && ch < 127) { // Printable characters
        insertChar(ch);
        return true;
    }
    
    return false;
}

bool ScreenEditor::handleEscapeSequence() {
    // Check if there's more input (escape sequence)
    if (!m_terminal.kbhit()) {
        return false;
    }
    
    char ch = m_terminal.waitForKey();
    if (ch == '[') {
        // ANSI escape sequence
        if (!m_terminal.kbhit()) {
            return false;
        }
        
        ch = m_terminal.waitForKey();
        switch (ch) {
            case 'A': // Up arrow
                moveCursorUp();
                return true;
            case 'B': // Down arrow
                moveCursorDown();
                return true;
            case 'C': // Right arrow
                moveCursorRight();
                return true;
            case 'D': // Left arrow
                moveCursorLeft();
                return true;
            case 'H': // Home
                moveCursorToLineStart();
                return true;
            case 'F': // End
                moveCursorToLineEnd();
                return true;
            case '5': // Page Up
                if (m_terminal.kbhit() && m_terminal.waitForKey() == '~') {
                    pageUp();
                    return true;
                }
                break;
            case '6': // Page Down
                if (m_terminal.kbhit() && m_terminal.waitForKey() == '~') {
                    pageDown();
                    return true;
                }
                break;
            case '3': // Delete
                if (m_terminal.kbhit() && m_terminal.waitForKey() == '~') {
                    deleteChar();
                    return true;
                }
                break;
        }
    }
    
    return false;
}

// Utility functions
void ScreenEditor::ensureCursorVisible() {
    // Scroll up if needed
    if (m_cursorLine < m_topLine) {
        m_topLine = m_cursorLine;
    }
    
    // Scroll down if needed
    if (m_cursorLine >= m_topLine + m_visibleLines) {
        m_topLine = m_cursorLine - m_visibleLines + 1;
    }
}

int ScreenEditor::getActualLineNumber(int editorLine) const {
    if (editorLine >= 0 && editorLine < static_cast<int>(m_lineNumbers.size())) {
        return m_lineNumbers[editorLine];
    }
    // Return a safe default
    if (m_lineNumbers.empty()) {
        return 1000;
    }
    return m_lineNumbers.back() + 10;
}

int ScreenEditor::getEditorLineIndex(int lineNumber) const {
    for (size_t i = 0; i < m_lineNumbers.size(); i++) {
        if (m_lineNumbers[i] == lineNumber) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string ScreenEditor::getCurrentLineText() const {
    // Bounds check
    if (m_cursorLine < 0 || m_cursorLine >= getLineCount()) {
        return "";
    }
    
    int lineNum = getActualLineNumber(m_cursorLine);
    return m_program.getLine(lineNum);
}

void ScreenEditor::setCurrentLineText(const std::string& text) {
    // Bounds check
    if (m_cursorLine < 0 || m_cursorLine >= getLineCount()) {
        return;
    }
    
    int lineNum = getActualLineNumber(m_cursorLine);
    m_program.setLineRaw(lineNum, text);
}

int ScreenEditor::getLineCount() const {
    return static_cast<int>(m_lineNumbers.size());
}

void ScreenEditor::rebuildLineCache() {
    m_lineNumbers.clear();
    
    // Get all line numbers from program manager
    auto lines = m_program.getAllLines();
    for (const auto& pair : lines) {
        m_lineNumbers.push_back(pair.first);
    }
    
    // Sort line numbers
    std::sort(m_lineNumbers.begin(), m_lineNumbers.end());
    
    // If program is empty, add a starter line
    if (m_lineNumbers.empty()) {
        m_program.setLineRaw(1000, "");
        m_lineNumbers.push_back(1000);
        // Ensure program recognizes the change
        m_program.setModified(true);
    }
    
    // Clamp cursor to valid range
    if (m_cursorLine >= static_cast<int>(m_lineNumbers.size())) {
        m_cursorLine = m_lineNumbers.empty() ? 0 : static_cast<int>(m_lineNumbers.size()) - 1;
    }
    if (m_cursorLine < 0) {
        m_cursorLine = 0;
    }
}

void ScreenEditor::renderLineWithSyntaxHighlighting(const std::string& lineText, int maxTextWidth) {
    if (!m_syntaxHighlightingEnabled || lineText.empty()) {
        // No syntax highlighting - just print normally
        std::string text = lineText;
        if (text.length() > maxTextWidth) {
            text = text.substr(0, maxTextWidth - 3) + "...";
        }
        m_terminal.print(text, false);
        return;
    }
    
    // Truncate if needed
    std::string text = lineText;
    bool truncated = false;
    if (text.length() > maxTextWidth) {
        text = text.substr(0, maxTextWidth - 3);
        truncated = true;
    }
    
    // Get syntax highlighting colors
    std::vector<TerminalColor> colors = m_syntaxHighlighter.highlightLine(text);
    
    // Render character by character with colors
    TerminalColor lastColor = TerminalColor::WHITE;
    m_terminal.setForegroundColor(lastColor);
    
    for (size_t i = 0; i < text.length(); i++) {
        TerminalColor currentColor = (i < colors.size()) ? colors[i] : TerminalColor::WHITE;
        
        // Change color if needed
        if (currentColor != lastColor) {
            m_terminal.setForegroundColor(currentColor);
            lastColor = currentColor;
        }
        
        // Print character
        m_terminal.print(std::string(1, text[i]), false);
    }
    
    // Add truncation indicator if needed
    if (truncated) {
        m_terminal.setForegroundColor(TerminalColor::BRIGHT_BLACK);
        m_terminal.print("...", false);
    }
    
    // Reset color
    m_terminal.resetColors();
}

// =============================================================================
// Error Tracking Implementation
// =============================================================================

void ScreenEditor::parseAndCheckErrors() {
    // Clear previous errors
    clearErrors();
    
    // Get program source
    std::string programText = m_program.generateProgram();
    
    if (programText.empty()) {
        return; // No program to check
    }
    
    try {
        // Tokenize
        FasterBASIC::Lexer lexer;
        // Lexer errors are typically tokenization errors - we skip them for now
        lexer.tokenize(programText);
        
        // Parse into AST
        const auto& tokens = lexer.getTokens();
        FasterBASIC::Parser parser;
        auto ast = parser.parse(tokens);
        
        // Get line number mapping to convert source lines to BASIC line numbers
        const auto& lineMapping = parser.getLineNumberMapping();
        
        if (parser.hasErrors()) {
            // Handle parser errors
            for (const auto& error : parser.getErrors()) {
                // Convert source line number to BASIC line number
                const int* basicLineNum = lineMapping.getBasicLineNumber(error.location.line);
                if (basicLineNum) {
                    LineError err;
                    err.lineNumber = *basicLineNum;
                    err.message = error.what();
                    err.isWarning = false;
                    err.type = SemanticErrorType::TYPE_ERROR;
                    m_lineErrors[err.lineNumber].push_back(err);
                    m_totalErrors++;
                }
            }
        }
        
        // Semantic analysis
        FasterBASIC::SemanticAnalyzer analyzer;
        FasterBASIC::CompilerOptions options = parser.getOptions();
        analyzer.analyze(*ast, options);
        
        // Collect semantic errors
        for (const auto& error : analyzer.getErrors()) {
            // Convert source line number to BASIC line number
            const int* basicLineNum = lineMapping.getBasicLineNumber(error.location.line);
            if (basicLineNum) {
                LineError err;
                err.lineNumber = *basicLineNum;
                err.message = error.message;
                err.isWarning = false;
                err.type = error.type;
                m_lineErrors[err.lineNumber].push_back(err);
                m_totalErrors++;
            }
        }
        
        // Collect warnings
        for (const auto& warning : analyzer.getWarnings()) {
            // Convert source line number to BASIC line number
            const int* basicLineNum = lineMapping.getBasicLineNumber(warning.location.line);
            if (basicLineNum) {
                LineError err;
                err.lineNumber = *basicLineNum;
                err.message = warning.message;
                err.isWarning = true;
                err.type = SemanticErrorType::TYPE_ERROR; // Not used for warnings
                m_lineErrors[err.lineNumber].push_back(err);
                m_totalWarnings++;
            }
        }
        
    } catch (const std::exception& e) {
        // Parsing failed completely - show generic error
        showMessage(std::string("Parse error: ") + e.what(), true);
    } catch (...) {
        // Unknown error during parsing
        showMessage("Unknown error during error checking", true);
    }
}

void ScreenEditor::clearErrors() {
    m_lineErrors.clear();
    m_totalErrors = 0;
    m_totalWarnings = 0;
}

void ScreenEditor::displayErrorsForCurrentLine() {
    if (!m_showErrors) {
        return;
    }
    
    // Get the current line number
    if (m_cursorLine < 0 || m_cursorLine >= static_cast<int>(m_lineNumbers.size())) {
        return;
    }
    
    int lineNumber = m_lineNumbers[m_cursorLine];
    
    auto it = m_lineErrors.find(lineNumber);
    if (it == m_lineErrors.end() || it->second.empty()) {
        // No errors on this line - show normal status if no other message
        if (m_statusMessage.find("Ctrl+") != std::string::npos) {
            // Only update if showing default status
            if (m_totalErrors > 0 || m_totalWarnings > 0) {
                std::ostringstream oss;
                oss << "Ctrl+Q:Quit Ctrl+S:Save Ctrl+R:Run Ctrl+E:Check";
                if (m_totalErrors > 0) {
                    oss << " [" << m_totalErrors << " error";
                    if (m_totalErrors != 1) oss << "s";
                    oss << "]";
                }
                if (m_totalWarnings > 0) {
                    oss << " [" << m_totalWarnings << " warning";
                    if (m_totalWarnings != 1) oss << "s";
                    oss << "]";
                }
                m_statusMessage = oss.str();
                m_statusIsError = false;
                drawStatusLine();
            }
        }
        return;
    }
    
    const auto& errors = it->second;
    
    // Show first error/warning in status line
    const auto& firstError = errors[0];
    std::ostringstream oss;
    
    if (firstError.isWarning) {
        oss << "WARNING: ";
    } else {
        oss << "ERROR: ";
    }
    
    oss << firstError.message;
    
    if (errors.size() > 1) {
        oss << " (+" << (errors.size() - 1) << " more)";
    }
    
    showMessage(oss.str(), !firstError.isWarning);
}

bool ScreenEditor::hasErrorOnLine(int lineNumber) const {
    auto it = m_lineErrors.find(lineNumber);
    if (it == m_lineErrors.end()) {
        return false;
    }
    
    for (const auto& err : it->second) {
        if (!err.isWarning) {
            return true;
        }
    }
    return false;
}

bool ScreenEditor::hasWarningOnLine(int lineNumber) const {
    auto it = m_lineErrors.find(lineNumber);
    if (it == m_lineErrors.end()) {
        return false;
    }
    
    for (const auto& err : it->second) {
        if (err.isWarning) {
            return true;
        }
    }
    return false;
}

int ScreenEditor::getErrorCount() const {
    return m_totalErrors;
}

int ScreenEditor::getWarningCount() const {
    return m_totalWarnings;
}

} // namespace FasterBASIC