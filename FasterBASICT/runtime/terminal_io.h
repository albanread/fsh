//
// terminal_io.h
// FasterBASIC Runtime - Terminal I/O Functions
//
// Provides terminal I/O functionality for standalone BASIC programs.
// Supports standard UNIX terminal operations with ANSI escape sequences.
//

#ifndef TERMINAL_IO_H
#define TERMINAL_IO_H

#include <string>
#include <utility>
#include <termios.h>

namespace FasterBASIC {

// Terminal colors (ANSI color codes)
enum class TerminalColor {
    BLACK = 0,
    RED = 1,
    GREEN = 2,
    YELLOW = 3,
    BLUE = 4,
    MAGENTA = 5,
    CYAN = 6,
    WHITE = 7,
    BRIGHT_BLACK = 8,
    BRIGHT_RED = 9,
    BRIGHT_GREEN = 10,
    BRIGHT_YELLOW = 11,
    BRIGHT_BLUE = 12,
    BRIGHT_MAGENTA = 13,
    BRIGHT_CYAN = 14,
    BRIGHT_WHITE = 15
};

// Terminal I/O Manager
class TerminalIO {
public:
    TerminalIO();
    ~TerminalIO();

    // Screen control
    void clearScreen();                              // CLS
    void locate(int x, int y);                       // LOCATE x, y (1-based coordinates)
    void getCursorPosition(int& x, int& y);          // Get current cursor position
    std::pair<int, int> getScreenSize();             // Get terminal width/height

    // Input functions
    std::string input(const std::string& prompt = ""); // INPUT with optional prompt
    std::string inkey();                             // INKEY$ - non-blocking single char
    char waitForKey();                               // Wait for single key press
    bool kbhit();                                    // Check if key is available

    // Output functions
    void print(const std::string& text, bool newline = false);
    void printAt(int x, int y, const std::string& text);
    
    // Color functions
    void setForegroundColor(TerminalColor color);
    void setBackgroundColor(TerminalColor color);
    void resetColors();
    
    // Cursor control
    void showCursor(bool show = true);
    void hideCursor();
    void saveCursor();
    void restoreCursor();
    
    // Terminal state
    void enableRawMode();
    void disableRawMode();
    bool isRawModeEnabled() const { return m_rawModeEnabled; }
    
    // Utility
    void flush();
    void beep();

private:
    bool m_rawModeEnabled;
    bool m_cursorVisible;
    struct termios m_originalTermios;  // Original terminal settings
    
    // Helper functions
    void sendEscapeSequence(const std::string& sequence);
    std::string getColorCode(TerminalColor color, bool background = false);
    void restoreTerminalSettings();
    bool readCursorPosition(int& x, int& y);
};

// Global terminal instance
extern TerminalIO g_terminal;

// Convenience functions (for C-style access)
extern "C" {
    void terminal_cls();
    void terminal_locate(int x, int y);
    void terminal_print(const char* text);
    void terminal_print_at(int x, int y, const char* text);
    char* terminal_input(const char* prompt);
    char* terminal_inkey();
    char terminal_key();
    int terminal_kbhit();
    void terminal_beep();
    void terminal_set_color(int fg, int bg);
    void terminal_reset_colors();
    int terminal_width();
    int terminal_height();
}

} // namespace FasterBASIC

#endif // TERMINAL_IO_H