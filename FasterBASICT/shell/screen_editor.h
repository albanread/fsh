//
// screen_editor.h
// FasterBASIC Shell - Screen Editor
//
// Full-screen editor for BASIC programs using terminal control
//

#ifndef SCREEN_EDITOR_H
#define SCREEN_EDITOR_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "../runtime/terminal_io.h"
#include "basic_syntax_highlighter.h"
#include "../src/basic_formatter_lib.h"
#include "../src/fasterbasic_semantic.h"

namespace FasterBASIC {

// Forward declaration
class ProgramManagerV2;

namespace ModularCommands {
    class CommandRegistry;
}

class ScreenEditor {
public:
    explicit ScreenEditor(ProgramManagerV2& program, const FasterBASIC::ModularCommands::CommandRegistry* registry = nullptr);
    ~ScreenEditor();

    // Main editor loop - returns true if program was modified
    bool run();
    
    // Check if user requested to run program (Ctrl+R)
    bool shouldRunProgram() const { return m_shouldRun; }

private:
    // Display functions
    void redraw();
    void drawStatusLine();
    void drawLineNumber(int lineNum, bool current);
    void updateCursorPosition();
    void showMessage(const std::string& msg, bool isError = false);

    // Navigation
    void moveCursorUp();
    void moveCursorDown();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorToLineStart();
    void moveCursorToLineEnd();
    void pageUp();
    void pageDown();
    void gotoLine(int lineNum);

    // Editing
    void insertChar(char ch);
    void deleteChar();           // Delete char at cursor (Delete key)
    void backspace();            // Delete char before cursor
    void insertNewLine();
    void killLine();             // Ctrl+K - delete current line
    void duplicateLine();        // Ctrl+D - duplicate current line
    void joinWithNextLine();

    // File operations
    bool saveProgram();
    void formatProgram();        // Ctrl+F - format entire program
    void runProgram();           // Ctrl+R - run program and return to editor
    void listLines();            // Ctrl+L - show line range dialog

    // Input handling
    bool handleKeyPress(char ch);
    bool handleEscapeSequence();

    // Utility
    void ensureCursorVisible();
    int getActualLineNumber(int editorLine) const;
    int getEditorLineIndex(int lineNumber) const;
    std::string getCurrentLineText() const;
    void setCurrentLineText(const std::string& text);
    int getLineCount() const;
    
    // Rendering with syntax highlighting
    void renderLineWithSyntaxHighlighting(const std::string& lineText, int maxTextWidth);
    
    // Format line text for display
    std::string formatLineForDisplay(const std::string& lineText);

    // Error tracking
    void parseAndCheckErrors();
    void clearErrors();
    void displayErrorsForCurrentLine();
    bool hasErrorOnLine(int lineNumber) const;
    bool hasWarningOnLine(int lineNumber) const;
    int getErrorCount() const;
    int getWarningCount() const;

    // Member variables
    ProgramManagerV2& m_program;
    TerminalIO& m_terminal;
    const FasterBASIC::ModularCommands::CommandRegistry* m_registry;
    
    // Editor state
    int m_cursorLine;           // Current line in editor (0-based)
    int m_cursorCol;            // Current column (0-based)
    int m_topLine;              // Top visible line (for scrolling)
    int m_screenWidth;
    int m_screenHeight;
    int m_visibleLines;         // Height minus status line
    bool m_modified;
    bool m_quit;
    std::string m_statusMessage;
    bool m_statusIsError;
    
    // Syntax highlighting
    BasicSyntaxHighlighter m_syntaxHighlighter;
    bool m_syntaxHighlightingEnabled;
    
    // Formatting
    bool m_formattingEnabled;
    
    // Run flag
    bool m_shouldRun;

    // Line cache (for performance)
    std::vector<int> m_lineNumbers;
    void rebuildLineCache();

    // Error tracking
    struct LineError {
        int lineNumber;           // BASIC line number (10, 20, etc.)
        std::string message;      // Error message
        bool isWarning;           // true = warning, false = error
        SemanticErrorType type;   // Error type for categorization
    };
    
    std::map<int, std::vector<LineError>> m_lineErrors;  // Line number -> errors
    bool m_showErrors;                                    // Toggle error display
    int m_totalErrors;                                    // Count of errors
    int m_totalWarnings;                                  // Count of warnings

    // Constants
    static const int STATUS_LINE_HEIGHT = 1;
    static const int LINE_NUM_WIDTH = 6;  // "1000: " format
};

} // namespace FasterBASIC

#endif // SCREEN_EDITOR_H