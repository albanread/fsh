#include "timer_lua_bindings.h"
#include "TimerManager.h"
#include "EventQueue.h"
#include <memory>
#include <string>
#include <iostream>
#include <cstring>

namespace FasterBASIC {

// Global timer system instance
static std::shared_ptr<EventQueue> g_eventQueue;
static std::shared_ptr<TimerManager> g_timerManager;
static bool g_timerSystemInitialized = false;

// Map to store handler coroutines
// Key: handler name, Value: lua_State* (coroutine)
static std::map<std::string, lua_State*> g_handlerCoroutines;

void registerTimerBindings(lua_State* L) {
    // Register all timer functions in the global namespace
    lua_register(L, "basic_timer_init", lua_basic_timer_init);
    lua_register(L, "basic_timer_after", lua_basic_timer_after);
    lua_register(L, "basic_timer_every", lua_basic_timer_every);
    lua_register(L, "basic_timer_after_frames", lua_basic_timer_after_frames);
    lua_register(L, "basic_timer_every_frame", lua_basic_timer_every_frame);
    lua_register(L, "basic_timer_stop", lua_basic_timer_stop);
    lua_register(L, "basic_timer_try_dequeue", lua_basic_timer_try_dequeue);
    lua_register(L, "basic_timer_check", lua_basic_timer_check);
    lua_register(L, "basic_timer_is_active", lua_basic_timer_is_active);
    lua_register(L, "basic_timer_get_active_count", lua_basic_timer_get_active_count);
    lua_register(L, "basic_timer_shutdown", lua_basic_timer_shutdown);
}

int lua_basic_timer_init(lua_State* L) {
    if (g_timerSystemInitialized) {
        lua_pushboolean(L, 1);  // Already initialized
        return 1;
    }
    
    try {
        // Create event queue and timer manager
        g_eventQueue = std::make_shared<EventQueue>();
        g_timerManager = std::make_shared<TimerManager>();
        
        // Initialize timer manager with event queue
        g_timerManager->initialize(g_eventQueue);
        
        // Start the processor thread
        g_timerManager->start();
        
        g_timerSystemInitialized = true;
        lua_pushboolean(L, 1);  // Success
        
    } catch (const std::exception& e) {
        std::cerr << "Timer init error: " << e.what() << std::endl;
        lua_pushboolean(L, 0);  // Failure
    }
    
    return 1;
}

int lua_basic_timer_try_dequeue(lua_State* L) {
    if (!g_timerSystemInitialized) {
        lua_pushnil(L);
        return 1;
    }
    
    try {
        QueuedEvent event(EventType::TIMER_AFTER, "", 0);
        
        // Try to dequeue one event (non-blocking)
        if (g_eventQueue->tryDequeue(event)) {
            // Create and return event table
            lua_createtable(L, 0, 3);  // table with 3 fields
            
            // Set type field
            lua_pushstring(L, event.type == EventType::TIMER_AFTER ? "after" : "every");
            lua_setfield(L, -2, "type");
            
            // Set handler field
            lua_pushstring(L, event.handlerName.c_str());
            lua_setfield(L, -2, "handler");
            
            // Set timer_id field
            lua_pushinteger(L, event.timerId);
            lua_setfield(L, -2, "timer_id");
            
            return 1;  // Return the event table
        } else {
            // No events pending
            lua_pushnil(L);
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error dequeuing timer event: " << e.what() << std::endl;
        lua_pushnil(L);
        return 1;
    }
}

int lua_basic_timer_after(lua_State* L) {
    if (!g_timerSystemInitialized) {
        return luaL_error(L, "Timer system not initialized. Call basic_timer_init() first.");
    }
    
    // Get arguments
    if (lua_gettop(L) < 2) {
        return luaL_error(L, "basic_timer_after requires 2 arguments: duration_ms, handler_name");
    }
    
    int durationMs = luaL_checkinteger(L, 1);
    const char* handlerName = luaL_checkstring(L, 2);
    
    if (durationMs < 0) {
        return luaL_error(L, "Timer duration must be non-negative");
    }
    
    try {
        int timerId = g_timerManager->registerAfter(durationMs, handlerName);
        lua_pushinteger(L, timerId);
        return 1;
        
    } catch (const std::exception& e) {
        return luaL_error(L, "Error registering AFTER timer: %s", e.what());
    }
}

int lua_basic_timer_every(lua_State* L) {
    if (!g_timerSystemInitialized) {
        return luaL_error(L, "Timer system not initialized. Call basic_timer_init() first.");
    }
    
    // Get arguments
    if (lua_gettop(L) < 2) {
        return luaL_error(L, "basic_timer_every requires 2 arguments: duration_ms, handler_name");
    }
    
    int durationMs = luaL_checkinteger(L, 1);
    const char* handlerName = luaL_checkstring(L, 2);
    
    if (durationMs <= 0) {
        return luaL_error(L, "Repeating timer duration must be positive");
    }
    
    try {
        int timerId = g_timerManager->registerEvery(durationMs, handlerName);
        lua_pushinteger(L, timerId);
        return 1;
        
    } catch (const std::exception& e) {
        return luaL_error(L, "Error registering EVERY timer: %s", e.what());
    }
}

int lua_basic_timer_after_frames(lua_State* L) {
    if (!g_timerSystemInitialized) {
        return luaL_error(L, "Timer system not initialized. Call basic_timer_init() first.");
    }
    
    // Get arguments
    if (lua_gettop(L) < 2) {
        return luaL_error(L, "basic_timer_after_frames requires 2 arguments: frame_count, handler_name");
    }
    
    int frameCount = luaL_checkinteger(L, 1);
    const char* handlerName = luaL_checkstring(L, 2);
    
    if (frameCount < 0) {
        return luaL_error(L, "Frame count must be non-negative");
    }
    
    try {
        int timerId = g_timerManager->registerAfterFrames(frameCount, handlerName);
        lua_pushinteger(L, timerId);
        return 1;
        
    } catch (const std::exception& e) {
        return luaL_error(L, "Error registering AFTERFRAMES timer: %s", e.what());
    }
}

int lua_basic_timer_every_frame(lua_State* L) {
    if (!g_timerSystemInitialized) {
        return luaL_error(L, "Timer system not initialized. Call basic_timer_init() first.");
    }
    
    // Get arguments
    if (lua_gettop(L) < 2) {
        return luaL_error(L, "basic_timer_every_frame requires 2 arguments: frame_count, handler_name");
    }
    
    int frameCount = luaL_checkinteger(L, 1);
    const char* handlerName = luaL_checkstring(L, 2);
    
    if (frameCount <= 0) {
        return luaL_error(L, "Repeating frame timer interval must be positive");
    }
    
    try {
        int timerId = g_timerManager->registerEveryFrame(frameCount, handlerName);
        lua_pushinteger(L, timerId);
        return 1;
        
    } catch (const std::exception& e) {
        return luaL_error(L, "Error registering EVERYFRAME timer: %s", e.what());
    }
}

int lua_basic_timer_stop(lua_State* L) {
    if (!g_timerSystemInitialized) {
        return luaL_error(L, "Timer system not initialized");
    }
    
    if (lua_gettop(L) < 1) {
        return luaL_error(L, "basic_timer_stop requires 1 argument");
    }
    
    try {
        if (lua_isnumber(L, 1)) {
            // Stop by timer ID
            int timerId = lua_tointeger(L, 1);
            g_timerManager->stopTimer(timerId);
            
        } else if (lua_isstring(L, 1)) {
            const char* arg = lua_tostring(L, 1);
            
            if (strcmp(arg, "ALL") == 0) {
                // Stop all timers
                g_timerManager->stopAllTimers();
            } else {
                // Stop by handler name
                g_timerManager->stopTimerByHandler(arg);
            }
        } else {
            return luaL_error(L, "basic_timer_stop argument must be number, string, or 'ALL'");
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        return luaL_error(L, "Error stopping timer: %s", e.what());
    }
}

int lua_basic_timer_check(lua_State* L) {
    if (!g_timerSystemInitialized) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    int eventsProcessed = 0;
    
    try {
        QueuedEvent event(EventType::TIMER_AFTER, "", 0);
        
        // Process all pending events in the queue
        while (g_eventQueue->tryDequeue(event)) {
            eventsProcessed++;
            
            // Get the handler function from the _handler_functions table
            lua_getglobal(L, "_handler_functions");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, event.handlerName.c_str());
                lua_remove(L, -2);  // Remove the _handler_functions table
                
                if (lua_isfunction(L, -1)) {
                    // Call the handler directly
                    int result = lua_pcall(L, 0, 0, 0);
                    
                    if (result != LUA_OK) {
                        // Error occurred
                        const char* error = lua_tostring(L, -1);
                        std::cerr << "Timer handler error (" << event.handlerName 
                                  << "): " << (error ? error : "unknown error") << std::endl;
                        lua_pop(L, 1);  // Pop error
                    }
                } else {
                    // Handler not found or not a function
                    std::cerr << "Timer handler not found or not a function: " 
                              << event.handlerName << std::endl;
                    lua_pop(L, 1);  // Pop the non-function value
                }
            } else {
                // _handler_functions table not found
                std::cerr << "_handler_functions table not found in Lua state" << std::endl;
                lua_pop(L, 1);  // Pop whatever was returned
            }
        }
        
        lua_pushinteger(L, eventsProcessed);
        return 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Error checking timer events: " << e.what() << std::endl;
        lua_pushinteger(L, eventsProcessed);
        return 1;
    }
}

int lua_basic_timer_is_active(lua_State* L) {
    if (!g_timerSystemInitialized) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    if (lua_gettop(L) < 1) {
        return luaL_error(L, "basic_timer_is_active requires 1 argument: timer_id");
    }
    
    int timerId = luaL_checkinteger(L, 1);
    bool isActive = g_timerManager->isTimerActive(timerId);
    
    lua_pushboolean(L, isActive ? 1 : 0);
    return 1;
}

int lua_basic_timer_get_active_count(lua_State* L) {
    if (!g_timerSystemInitialized) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    int count = g_timerManager->getActiveTimerCount();
    lua_pushinteger(L, count);
    return 1;
}

int lua_basic_timer_shutdown(lua_State* /* L */) {
    if (!g_timerSystemInitialized) {
        return 0;
    }
    
    try {
        // Stop the timer manager
        g_timerManager->stop();
        
        // Clear the event queue
        g_eventQueue->clear();
        
        // Clean up handler coroutines
        g_handlerCoroutines.clear();
        
        // Reset global pointers
        g_timerManager.reset();
        g_eventQueue.reset();
        
        g_timerSystemInitialized = false;
        
    } catch (const std::exception& e) {
        std::cerr << "Error shutting down timer system: " << e.what() << std::endl;
    }
    
    return 0;
}

} // namespace FasterBASIC

// C API for Framework to notify timer system of frame completions
extern "C" {
    void basic_timer_on_frame_completed(uint64_t frameNumber) {
        if (FasterBASIC::g_timerSystemInitialized && FasterBASIC::g_timerManager) {
            FasterBASIC::g_timerManager->onFrameCompleted(frameNumber);
        }
    }
}