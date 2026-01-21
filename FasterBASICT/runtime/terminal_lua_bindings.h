//
// terminal_lua_bindings.h
// FasterBASIC Runtime - Terminal I/O Lua Bindings
//
// Provides Lua bindings for terminal I/O functionality.
// These bindings are used by compiled BASIC code to perform terminal operations.
//

#ifndef TERMINAL_LUA_BINDINGS_H
#define TERMINAL_LUA_BINDINGS_H

#include <lua.hpp>
#include <string>

namespace FasterBASIC {

// =============================================================================
// Terminal Control Functions
// =============================================================================

// Clear screen and reset cursor position
void terminal_clear_screen();

// Set cursor position (1-based coordinates like BASIC)
void terminal_set_cursor_position(int x, int y);

// Get current cursor position
void terminal_get_cursor_position(int& x, int& y);

// Get terminal dimensions
void terminal_get_screen_size(int& width, int& height);

// =============================================================================
// Input Functions
// =============================================================================

// Read line input with optional prompt (INPUT statement)
std::string terminal_read_line(const std::string& prompt = "");

// Non-blocking single character input (INKEY$ function)
std::string terminal_read_key_nonblocking();

// Blocking single character input (KEY function)
char terminal_read_key_blocking();

// Check if key is available (for INKEY$ implementation)
bool terminal_key_available();

// =============================================================================
// Output Functions
// =============================================================================

// Print text at current cursor position
void terminal_write_text(const std::string& text);

// Print text at specific position
void terminal_write_text_at(int x, int y, const std::string& text);

// Print with newline
void terminal_write_line(const std::string& text);

// =============================================================================
// Color and Display Functions
// =============================================================================

// Set text colors (0-15 for standard ANSI colors)
void terminal_set_colors(int foreground, int background);

// Reset colors to default
void terminal_reset_display();

// Show/hide cursor
void terminal_show_cursor(bool visible);

// Sound bell/beep
void terminal_sound_bell();

// =============================================================================
// Lua Registration Function
// =============================================================================

// Register all terminal I/O Lua bindings
void registerTerminalBindings(lua_State* L);

} // namespace FasterBASIC

#endif // TERMINAL_LUA_BINDINGS_H