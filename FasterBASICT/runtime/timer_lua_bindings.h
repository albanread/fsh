#ifndef FASTERBASIC_TIMER_LUA_BINDINGS_H
#define FASTERBASIC_TIMER_LUA_BINDINGS_H

#include <cstdint>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

namespace FasterBASIC {

// Register all timer-related Lua bindings
// This makes the timer functions available to Lua code
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

// Lua binding: basic_timer_after_frames(frame_count, handler_name)
// Registers a one-shot frame-based timer that fires after frame_count frames
// Args:
//   frame_count: number - frames to wait before firing
//   handler_name: string - name of the BASIC subroutine to call
// Returns: timer_id (number) - unique ID for this timer
int lua_basic_timer_after_frames(lua_State* L);

// Lua binding: basic_timer_every_frame(frame_count, handler_name)
// Registers a repeating frame-based timer that fires every frame_count frames
// Args:
//   frame_count: number - frames between each firing
//   handler_name: string - name of the BASIC subroutine to call
// Returns: timer_id (number) - unique ID for this timer
int lua_basic_timer_every_frame(lua_State* L);

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

} // namespace FasterBASIC

// C API for Framework to notify timer system of frame completions
// This should be called by the render thread after each frame is rendered
extern "C" {
    // Notify the timer system that a frame has completed
    // frameNumber: the current frame number from st_get_frame_count()
    void basic_timer_on_frame_completed(uint64_t frameNumber);
}

#endif // FASTERBASIC_TIMER_LUA_BINDINGS_H