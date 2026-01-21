#ifndef FASTERBASIC_TIMER_LUA_BINDINGS_TERMINAL_H
#define FASTERBASIC_TIMER_LUA_BINDINGS_TERMINAL_H

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

namespace FasterBASIC {
namespace Terminal {

// Register all timer-related Lua bindings for terminal mode
// This makes the timer functions available to Lua code
// Frame-based functions are NOT included in terminal mode
void registerTimerBindings(lua_State* L);

// Lua binding: basic_timer_init()
// Initializes the timer system
// Must be called before using timers
// Returns: true on success, false on failure
int lua_basic_timer_init(lua_State* L);

// Lua binding: basic_timer_after(duration_ms, handler_name)
// Registers a one-shot timer that fires after duration_ms milliseconds
// Args:
//   duration_ms: number - milliseconds to wait before firing
//   handler_name: string - name of the BASIC subroutine to call
// Returns: timer_id (number) - unique ID for this timer
int lua_basic_timer_after(lua_State* L);

// Lua binding: basic_timer_every(duration_ms, handler_name)
// Registers a repeating timer that fires every duration_ms milliseconds
// Args:
//   duration_ms: number - milliseconds between each firing
//   handler_name: string - name of the BASIC subroutine to call
// Returns: timer_id (number) - unique ID for this timer
int lua_basic_timer_every(lua_State* L);

// Lua binding: basic_timer_stop(timer_id_or_handler)
// Stops a timer by ID or handler name, or stops all timers
// Args:
//   timer_id_or_handler: number|string|"ALL" - timer to stop
//     - number: stops timer with that ID
//     - string: stops all timers with that handler name
//     - "ALL": stops all timers
// Returns: none
int lua_basic_timer_stop(lua_State* L);

// Lua binding: basic_timer_try_dequeue()
// Non-blocking check for one pending timer event
// Returns event table {type="after"|"every", handler="name", timer_id=N} or nil
// This is used by the event-checker coroutine to poll for events
// Returns: table or nil
int lua_basic_timer_try_dequeue(lua_State* L);

// Lua binding: basic_timer_check()
// Non-blocking check for pending timer events
// Processes all pending events in the queue
// This should be called from safe points (loops, input, etc.)
// Returns: number - count of events processed
int lua_basic_timer_check(lua_State* L);

// Lua binding: basic_timer_is_active(timer_id)
// Checks if a timer is still active
// Args:
//   timer_id: number - timer ID to check
// Returns: boolean - true if active, false otherwise
int lua_basic_timer_is_active(lua_State* L);

// Lua binding: basic_timer_get_active_count()
// Gets the number of currently active timers
// Returns: number - count of active timers
int lua_basic_timer_get_active_count(lua_State* L);

// Lua binding: basic_timer_shutdown()
// Shuts down the timer system
// Stops all timers and the processor thread
// Returns: none
int lua_basic_timer_shutdown(lua_State* L);

// C++ utility function: stopAllTimersFromShell()
// Stops all active timers without shutting down the timer system
// This is called when Control+C is pressed to clear pending timers
// Safe to call even if timer system is not initialized
void stopAllTimersFromShell();

} // namespace Terminal
} // namespace FasterBASIC

#endif // FASTERBASIC_TIMER_LUA_BINDINGS_TERMINAL_H