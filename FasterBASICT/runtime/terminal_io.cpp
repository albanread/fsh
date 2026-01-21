//
// terminal_io.cpp
// FasterBASIC Runtime - Terminal I/O Implementation
//
// Provides terminal I/O functionality for standalone BASIC programs.
// Supports standard UNIX terminal operations with ANSI escape sequences.
//

#include "terminal_io.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

namespace FasterBASIC {

// Global terminal instance
TerminalIO g_terminal;

TerminalIO::TerminalIO() 
    : m_rawModeEnabled(false)
    , m_cursorVisible(true) {
    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &m_originalTermios);
}

TerminalIO::~TerminalIO() {
    restoreTerminalSettings();
}

void TerminalIO::clearScreen() {
    sendEscapeSequence("\033[2J\033[H");
    flush();
}

void TerminalIO::locate(int x, int y) {
    // BASIC uses 1-based coordinates, ANSI uses 1-based too
    std::ostringstream oss;
    oss << "\033[" << y << ";" << x << "H";
    sendEscapeSequence(oss.str());
    flush();
}

void TerminalIO::getCursorPosition(int& x, int& y) {
    if (!readCursorPosition(x, y)) {
        x = 1;
        y = 1;
    }
}

std::pair<int, int> TerminalIO::getScreenSize() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return {w.ws_col, w.ws_row};
    }
    return {80, 25}; // Default fallback
}

std::string TerminalIO::input(const std::string& prompt) {
    if (!prompt.empty()) {
        std::cout << prompt;
        flush();
    }
    
    std::string line;
    std::getline(std::cin, line);
    return line;
}

std::string TerminalIO::inkey() {
    if (!kbhit()) {
        return "";
    }
    
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        return std::string(1, ch);
    }
    return "";
}

char TerminalIO::waitForKey() {
    bool wasRawMode = m_rawModeEnabled;
    if (!wasRawMode) {
        enableRawMode();
    }
    
    char ch;
    ssize_t result;
    while ((result = read(STDIN_FILENO, &ch, 1)) != 1) {
        if (result == -1 && errno == EINTR) {
            // Interrupted by signal (e.g., SIGWINCH) - return a special value
            // to allow the event loop to process the signal
            ch = 0;
            break;
        }
        // Keep trying for other errors
    }
    
    if (!wasRawMode) {
        disableRawMode();
    }
    
    return ch;
}

bool TerminalIO::kbhit() {
    if (!m_rawModeEnabled) {
        enableRawMode();
    }
    
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    return result > 0;
}

void TerminalIO::print(const std::string& text, bool newline) {
    std::cout << text;
    if (newline) {
        std::cout << std::endl;
    }
    flush();
}

void TerminalIO::printAt(int x, int y, const std::string& text) {
    locate(x, y);
    print(text, false);
}

void TerminalIO::setForegroundColor(TerminalColor color) {
    std::string code = getColorCode(color, false);
    sendEscapeSequence(code);
}

void TerminalIO::setBackgroundColor(TerminalColor color) {
    std::string code = getColorCode(color, true);
    sendEscapeSequence(code);
}

void TerminalIO::resetColors() {
    sendEscapeSequence("\033[0m");
}

void TerminalIO::showCursor(bool show) {
    if (show) {
        sendEscapeSequence("\033[?25h");
    } else {
        sendEscapeSequence("\033[?25l");
    }
    m_cursorVisible = show;
    flush();
}

void TerminalIO::hideCursor() {
    showCursor(false);
}

void TerminalIO::saveCursor() {
    sendEscapeSequence("\033[s");
}

void TerminalIO::restoreCursor() {
    sendEscapeSequence("\033[u");
}

void TerminalIO::enableRawMode() {
    if (m_rawModeEnabled) {
        return;
    }
    
    struct termios raw = m_originalTermios;
    
    // Disable canonical mode and echo
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    
    // Set minimum characters to read and timeout
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    m_rawModeEnabled = true;
}

void TerminalIO::disableRawMode() {
    if (!m_rawModeEnabled) {
        return;
    }
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_originalTermios);
    m_rawModeEnabled = false;
}

void TerminalIO::flush() {
    std::cout.flush();
}

void TerminalIO::beep() {
    std::cout << '\a';
    flush();
}

// Private helper functions

void TerminalIO::sendEscapeSequence(const std::string& sequence) {
    std::cout << sequence;
}

std::string TerminalIO::getColorCode(TerminalColor color, bool background) {
    int baseCode = background ? 40 : 30;
    int colorCode = static_cast<int>(color);
    
    std::ostringstream oss;
    
    if (colorCode >= 8) {
        // Bright colors (use 90-97 for foreground, 100-107 for background)
        oss << "\033[" << (baseCode + 60 + (colorCode - 8)) << "m";
    } else {
        // Normal colors (use 30-37 for foreground, 40-47 for background)
        oss << "\033[" << (baseCode + colorCode) << "m";
    }
    
    return oss.str();
}

void TerminalIO::restoreTerminalSettings() {
    if (m_rawModeEnabled) {
        disableRawMode();
    }
    
    // Restore cursor visibility
    if (!m_cursorVisible) {
        showCursor(true);
    }
    
    // Reset colors
    resetColors();
}

bool TerminalIO::readCursorPosition(int& x, int& y) {
    // Send cursor position request
    sendEscapeSequence("\033[6n");
    flush();
    
    // Read response: ESC[row;colR
    char response[32];
    int i = 0;
    
    // Enable raw mode temporarily to read the response
    bool wasRawMode = m_rawModeEnabled;
    if (!wasRawMode) {
        enableRawMode();
    }
    
    // Read the escape sequence
    while (i < 31) {
        if (read(STDIN_FILENO, &response[i], 1) != 1) {
            break;
        }
        if (response[i] == 'R') {
            response[i + 1] = '\0';
            break;
        }
        i++;
    }
    
    if (!wasRawMode) {
        disableRawMode();
    }
    
    // Parse the response
    if (i >= 6 && response[0] == '\033' && response[1] == '[') {
        if (sscanf(&response[2], "%d;%d", &y, &x) == 2) {
            return true;
        }
    }
    
    return false;
}

// C-style convenience functions
extern "C" {
    void terminal_cls() {
        g_terminal.clearScreen();
    }
    
    void terminal_locate(int x, int y) {
        g_terminal.locate(x, y);
    }
    
    void terminal_print(const char* text) {
        if (text) {
            g_terminal.print(text, false);
        }
    }
    
    void terminal_print_at(int x, int y, const char* text) {
        if (text) {
            g_terminal.printAt(x, y, text);
        }
    }
    
    char* terminal_input(const char* prompt) {
        std::string promptStr = prompt ? prompt : "";
        std::string result = g_terminal.input(promptStr);
        
        // Allocate and return C string (caller must free)
        char* cstr = static_cast<char*>(malloc(result.length() + 1));
        if (cstr) {
            strcpy(cstr, result.c_str());
        }
        return cstr;
    }
    
    char* terminal_inkey() {
        std::string result = g_terminal.inkey();
        
        // Allocate and return C string (caller must free)
        char* cstr = static_cast<char*>(malloc(result.length() + 1));
        if (cstr) {
            strcpy(cstr, result.c_str());
        }
        return cstr;
    }
    
    char terminal_key() {
        return g_terminal.waitForKey();
    }
    
    int terminal_kbhit() {
        return g_terminal.kbhit() ? 1 : 0;
    }
    
    void terminal_beep() {
        g_terminal.beep();
    }
    
    void terminal_set_color(int fg, int bg) {
        if (fg >= 0 && fg <= 15) {
            g_terminal.setForegroundColor(static_cast<TerminalColor>(fg));
        }
        if (bg >= 0 && bg <= 15) {
            g_terminal.setBackgroundColor(static_cast<TerminalColor>(bg));
        }
    }
    
    void terminal_reset_colors() {
        g_terminal.resetColors();
    }
    
    int terminal_width() {
        auto size = g_terminal.getScreenSize();
        return size.first;
    }
    
    int terminal_height() {
        auto size = g_terminal.getScreenSize();
        return size.second;
    }
}

} // namespace FasterBASIC