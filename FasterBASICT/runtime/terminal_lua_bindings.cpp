//
// terminal_lua_bindings.cpp
// FasterBASIC Runtime - Terminal I/O Lua Bindings
//
// Provides Lua bindings for terminal I/O functionality.
// These bindings are used by compiled BASIC code to perform terminal operations.
//

#include "terminal_lua_bindings.h"
#include "terminal_io.h"
#include <lua.hpp>
#include <iostream>
#include <string>
#include <cmath>
#include <chrono>

namespace FasterBASIC {

// =============================================================================
// Terminal Control Functions
// =============================================================================

void terminal_clear_screen() {
    g_terminal.clearScreen();
}

void terminal_set_cursor_position(int x, int y) {
    g_terminal.locate(x, y);
}

void terminal_get_cursor_position(int& x, int& y) {
    g_terminal.getCursorPosition(x, y);
}

void terminal_get_screen_size(int& width, int& height) {
    auto size = g_terminal.getScreenSize();
    width = size.first;
    height = size.second;
}

// =============================================================================
// Input Functions
// =============================================================================

std::string terminal_read_line(const std::string& prompt) {
    return g_terminal.input(prompt);
}

std::string terminal_read_key_nonblocking() {
    return g_terminal.inkey();
}

char terminal_read_key_blocking() {
    return g_terminal.waitForKey();
}

bool terminal_key_available() {
    return g_terminal.kbhit();
}

// =============================================================================
// Output Functions
// =============================================================================

void terminal_write_text(const std::string& text) {
    g_terminal.print(text, false);
}

void terminal_write_text_at(int x, int y, const std::string& text) {
    g_terminal.printAt(x, y, text);
}

void terminal_write_line(const std::string& text) {
    g_terminal.print(text, true);
}

// =============================================================================
// Color and Display Functions
// =============================================================================

void terminal_set_colors(int foreground, int background) {
    if (foreground >= 0 && foreground <= 15) {
        g_terminal.setForegroundColor(static_cast<TerminalColor>(foreground));
    }
    if (background >= 0 && background <= 15) {
        g_terminal.setBackgroundColor(static_cast<TerminalColor>(background));
    }
}

void terminal_reset_display() {
    g_terminal.resetColors();
}

void terminal_show_cursor(bool visible) {
    g_terminal.showCursor(visible);
}

void terminal_sound_bell() {
    g_terminal.beep();
}

// =============================================================================
// Lua Binding Functions
// =============================================================================

static int lua_terminal_cls(lua_State* L) {
    // For standalone mode, just print newlines
    std::cout << "\n\n\n\n\n\n\n\n\n\n" << std::flush;
    return 0;
}

static int lua_terminal_locate(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    terminal_set_cursor_position(x, y);
    return 0;
}

static int lua_terminal_pos(lua_State* L) {
    int x, y;
    terminal_get_cursor_position(x, y);
    lua_pushinteger(L, x);
    return 1;
}

static int lua_terminal_csrlin(lua_State* L) {
    int x, y;
    terminal_get_cursor_position(x, y);
    lua_pushinteger(L, y);
    return 1;
}

static int lua_terminal_input(lua_State* L) {
    std::string prompt = "";
    if (lua_gettop(L) > 0 && !lua_isnil(L, 1)) {
        prompt = luaL_checkstring(L, 1);
    }
    
    std::string result = terminal_read_line(prompt);
    lua_pushstring(L, result.c_str());
    return 1;
}

static int lua_terminal_inkey(lua_State* L) {
    std::string key = terminal_read_key_nonblocking();
    lua_pushstring(L, key.c_str());
    return 1;
}

static int lua_terminal_key(lua_State* L) {
    char key = terminal_read_key_blocking();
    std::string keyStr(1, key);
    lua_pushstring(L, keyStr.c_str());
    return 1;
}

static int lua_terminal_kbhit(lua_State* L) {
    bool available = terminal_key_available();
    lua_pushboolean(L, available);
    return 1;
}

static int lua_terminal_print(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    terminal_write_text(text);
    return 0;
}

static int lua_terminal_print_at(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    terminal_write_text_at(x, y, text);
    return 0;
}

static int lua_terminal_width(lua_State* L) {
    int width, height;
    terminal_get_screen_size(width, height);
    lua_pushinteger(L, width);
    return 1;
}

static int lua_terminal_height(lua_State* L) {
    int width, height;
    terminal_get_screen_size(width, height);
    lua_pushinteger(L, height);
    return 1;
}

static int lua_terminal_color(lua_State* L) {
    int fg = luaL_optinteger(L, 1, -1);
    int bg = luaL_optinteger(L, 2, -1);
    terminal_set_colors(fg, bg);
    return 0;
}

static int lua_terminal_reset(lua_State* L) {
    terminal_reset_display();
    return 0;
}

static int lua_terminal_cursor(lua_State* L) {
    int visible = lua_toboolean(L, 1);
    terminal_show_cursor(visible != 0);
    return 0;
}

static int lua_terminal_beep(lua_State* L) {
    terminal_sound_bell();
    return 0;
}

// Basic print function (what BASIC PRINT statements call)
static int lua_basic_console(lua_State* L) {
    int n = lua_gettop(L);
    std::string output;
    
    for (int i = 1; i <= n; i++) {
        if (i > 1) {
            output += " ";  // Space between arguments
        }
        
        if (lua_isstring(L, i)) {
            output += lua_tostring(L, i);
        } else if (lua_isnumber(L, i)) {
            double num = lua_tonumber(L, i);
            // Check if it's an integer
            if (num == floor(num)) {
                output += std::to_string((long long)num);
            } else {
                output += std::to_string(num);
            }
        } else if (lua_isboolean(L, i)) {
            output += lua_toboolean(L, i) ? "true" : "false";
        } else if (lua_isnil(L, i)) {
            // Skip nil values
        } else {
            output += "[object]";
        }
    }
    
    // Print to console (stdout)
    std::cout << output << std::flush;
    return 0;
}

static int lua_basic_print(lua_State* L) {
    // Use Lua's native io.write for print functionality
    // Unicode-aware: converts codepoint arrays to UTF-8 strings
    int n = lua_gettop(L);
    
    for (int i = 1; i <= n; i++) {
        // Get io.write function for each argument
        lua_getglobal(L, "io");
        lua_getfield(L, -1, "write");
        
        // Check if argument is a table (Unicode codepoint array)
        if (lua_istable(L, i)) {
            // Convert codepoint array to UTF-8 string
            // Get unicode.to_utf8 function
            lua_getglobal(L, "unicode");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "to_utf8");
                if (lua_isfunction(L, -1)) {
                    // Call unicode.to_utf8(codepoint_array)
                    lua_pushvalue(L, i);  // Push the codepoint array
                    lua_call(L, 1, 1);    // Call to_utf8, returns UTF-8 string
                    // Stack: io, io.write, unicode, utf8_string
                    // Move string before io.write for the call
                    lua_remove(L, -2);  // Remove unicode table
                    lua_remove(L, -3);  // Remove io table
                    // Stack: io.write, utf8_string
                    lua_call(L, 1, 0);  // Call io.write with string
                    continue;
                } else {
                    lua_pop(L, 2);  // Pop function and unicode table
                }
            } else {
                lua_pop(L, 1);  // Pop non-table unicode
            }
        }
        
        // Normal case: push the argument to io.write
        lua_pushvalue(L, i);
        // Stack: io, io.write, argument
        lua_remove(L, -3);  // Remove io table
        // Stack: io.write, argument
        lua_call(L, 1, 0);  // Call io.write with 1 argument
    }
    
    return 0;
}

static int lua_basic_print_newline(lua_State* L) {
    // Use Lua's native print for newline
    lua_getglobal(L, "print");
    lua_pushstring(L, "");
    lua_call(L, 1, 0);
    return 0;
}

// Enhanced CLS that also resets colors and shows cursor
static int lua_basic_cls(lua_State* L) {
    terminal_clear_screen();
    terminal_reset_display();
    terminal_show_cursor(true);
    return 0;
}

// Enhanced INPUT that handles prompts and multiple variables
static int lua_basic_input(lua_State* L) {
    std::string prompt = "";
    if (lua_gettop(L) > 0 && !lua_isnil(L, 1)) {
        prompt = luaL_checkstring(L, 1);
        if (!prompt.empty() && prompt.back() != ' ' && prompt.back() != '?') {
            prompt += "? ";
        }
    }
    
    std::string result = terminal_read_line(prompt);
    lua_pushstring(L, result.c_str());
    return 1;
}

// LOCATE function (same as lua_terminal_locate but follows BASIC conventions)
static int lua_basic_locate(lua_State* L) {
    int row = luaL_checkinteger(L, 1);
    int col = luaL_optinteger(L, 2, 1);
    terminal_set_cursor_position(col, row);  // BASIC LOCATE is row,col but our locate is x,y
    return 0;
}

// POS function - get current column
static int lua_basic_pos(lua_State* L) {
    int x, y;
    terminal_get_cursor_position(x, y);
    lua_pushinteger(L, x);
    return 1;
}

// CSRLIN function - get current row
static int lua_basic_csrlin(lua_State* L) {
    int x, y;
    terminal_get_cursor_position(x, y);
    lua_pushinteger(L, y);
    return 1;
}

// =============================================================================
// Registration Function
// =============================================================================

void registerTerminalBindings(lua_State* L) {
    // Core terminal functions (for internal use)
    lua_register(L, "terminal_cls", lua_terminal_cls);
    lua_register(L, "terminal_locate", lua_terminal_locate);
    lua_register(L, "terminal_pos", lua_terminal_pos);
    lua_register(L, "terminal_csrlin", lua_terminal_csrlin);
    lua_register(L, "terminal_input", lua_terminal_input);
    lua_register(L, "terminal_inkey", lua_terminal_inkey);
    lua_register(L, "terminal_key", lua_terminal_key);
    lua_register(L, "terminal_kbhit", lua_terminal_kbhit);
    lua_register(L, "terminal_print", lua_terminal_print);
    lua_register(L, "terminal_print_at", lua_terminal_print_at);
    lua_register(L, "terminal_width", lua_terminal_width);
    lua_register(L, "terminal_height", lua_terminal_height);
    lua_register(L, "terminal_color", lua_terminal_color);
    lua_register(L, "terminal_reset", lua_terminal_reset);
    lua_register(L, "terminal_cursor", lua_terminal_cursor);
    lua_register(L, "terminal_beep", lua_terminal_beep);
    
    // BASIC-style functions (what BASIC programs actually call)
    lua_register(L, "basic_cls", lua_basic_cls);
    lua_register(L, "basic_locate", lua_basic_locate);
    lua_register(L, "basic_pos", lua_basic_pos);
    lua_register(L, "basic_csrlin", lua_basic_csrlin);
    lua_register(L, "basic_inkey", lua_terminal_inkey);
    lua_register(L, "basic_beep", lua_terminal_beep);
    lua_register(L, "basic_color", lua_terminal_color);
    lua_register(L, "basic_console", lua_basic_console);
    lua_register(L, "basic_print", lua_basic_print);
    lua_register(L, "basic_print_newline", lua_basic_print_newline);
    
    // INPUT function for numeric variables
    lua_pushcfunction(L, [](lua_State* L) -> int {
        std::string result = terminal_read_line("");
        // Try to convert to number, return 0 if not a valid number
        double num = 0.0;
        try {
            num = std::stod(result);
        } catch (...) {
            num = 0.0;
        }
        lua_pushnumber(L, num);
        return 1;
    });
    lua_setglobal(L, "basic_input");
    
    // INPUT function for string variables
    lua_pushcfunction(L, [](lua_State* L) -> int {
        std::string result = terminal_read_line("");
        lua_pushstring(L, result.c_str());
        return 1;
    });
    lua_setglobal(L, "basic_input_string");
    
    // INPUT_AT function - input at specific coordinates
    // INPUT_AT function for positioned input
    // Only register if not already defined (don't override FBRunner3 version)
    lua_getglobal(L, "basic_input_at");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // Remove nil
        // FasterBASICT registering basic_input_at (FBRunner3 version not found)
        lua_pushcfunction(L, [](lua_State* L) -> int {

            int x = luaL_checkinteger(L, 1);
            int y = luaL_checkinteger(L, 2);
            std::string prompt = "";
            if (lua_gettop(L) > 2 && !lua_isnil(L, 3)) {
                prompt = luaL_checkstring(L, 3);
            }
            

            
            // Position cursor at specified coordinates
            terminal_locate(x, y);
            
            // Print prompt if provided
            if (!prompt.empty()) {
                terminal_print(prompt.c_str());
            }
            
            // Read input
            std::string result = terminal_read_line("");

            lua_pushstring(L, result.c_str());
            return 1;
        });
        lua_setglobal(L, "basic_input_at");
    } else {
        // FasterBASICT found existing basic_input_at, not overriding
        lua_pop(L, 1); // Remove existing function
    }
    
    // Aliases for compatibility with generated Lua code
    lua_register(L, "text_clear", lua_basic_cls);
    
    // Uppercase aliases for BASIC commands (what the code generator produces)
    lua_register(L, "CLS", lua_basic_cls);
    lua_register(L, "LOCATE", lua_basic_locate);
    lua_register(L, "AT", lua_basic_locate);  // AT is an alias for LOCATE
    lua_register(L, "BEEP", lua_terminal_beep);
    lua_register(L, "COLOR", lua_terminal_color);
    lua_register(L, "CONSOLE", lua_basic_console);
    
    // Sound functions - create proper stub for synth_frequency
    lua_pushcfunction(L, [](lua_State* L) -> int {
        // synth_frequency(frequency, duration) - just beep for terminal mode
        terminal_sound_bell();
        return 0;
    });
    lua_setglobal(L, "synth_frequency");
    
    // Graphics stubs (no-op for terminal mode)
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "chunky_clear");
    
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "gfx_point");
    
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "gfx_line");
    
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "gfx_rect");
    
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "gfx_rect_outline");
    
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "gfx_circle");
    
    lua_pushcfunction(L, [](lua_State* L) -> int { return 0; });
    lua_setglobal(L, "gfx_circle_outline");
    
    // Screen dimension functions
    lua_register(L, "screen_width", lua_terminal_width);
    lua_register(L, "screen_height", lua_terminal_height);
    
    // GETTICKS function - returns milliseconds since program start
    // We store the start time as an upvalue in the closure
    auto start_time = std::chrono::high_resolution_clock::now();
    auto start_time_ptr = new std::chrono::high_resolution_clock::time_point(start_time);
    
    lua_pushlightuserdata(L, start_time_ptr);
    lua_pushcclosure(L, [](lua_State* L) -> int {
        auto* start_ptr = (std::chrono::high_resolution_clock::time_point*)lua_touserdata(L, lua_upvalueindex(1));
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - *start_ptr).count();
        lua_pushnumber(L, static_cast<double>(elapsed));
        return 1;
    }, 1);
    lua_setglobal(L, "system_getticks");
}

} // namespace FasterBASIC