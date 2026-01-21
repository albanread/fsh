#ifndef FASTERBASIC_TIMER_MANAGER_TERMINAL_H
#define FASTERBASIC_TIMER_MANAGER_TERMINAL_H

#include "EventQueue_terminal.h"
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>

namespace FasterBASIC {
namespace Terminal {

// Represents a timer entry (terminal version - no frame-based timers)
struct TimerEntry {
    int timerId;
    EventType type;              // TIMER_AFTER or TIMER_EVERY
    std::string handlerName;     // BASIC subroutine to call
    int durationMs;              // Timer duration in milliseconds
    int elapsedMs;               // Elapsed time since last fire
    bool active;                 // Whether timer is active
    
    TimerEntry(int id, EventType t, const std::string& handler, int duration)
        : timerId(id), type(t), handlerName(handler), 
          durationMs(duration), elapsedMs(0), active(true) {}
};

// Thread-safe timer manager for terminal environments
// Runs a background processor thread that updates timers and posts events
// Frame-based timers are NOT supported in terminal mode
class TimerManager {
public:
    TimerManager();
    ~TimerManager();
    
    // Initialize with an event queue
    // Must be called before starting the processor
    void initialize(std::shared_ptr<EventQueue> queue);
    
    // Register a one-shot timer (AFTER)
    // Returns the timer ID
    int registerAfter(int durationMs, const std::string& handlerName);
    
    // Register a repeating timer (EVERY)
    // Returns the timer ID
    int registerEvery(int durationMs, const std::string& handlerName);
    
    // Stop a specific timer by ID
    void stopTimer(int timerId);
    
    // Stop a specific timer by handler name
    void stopTimerByHandler(const std::string& handlerName);
    
    // Stop all timers
    void stopAllTimers();
    
    // Check if a timer is active
    bool isTimerActive(int timerId) const;
    
    // Get the number of active timers
    int getActiveTimerCount() const;
    
    // Start the processor thread
    // This begins monitoring and firing timers
    void start();
    
    // Stop the processor thread
    // This gracefully shuts down the background thread
    void stop();
    
    // Check if processor is running
    bool isRunning() const;
    
    // Set the update interval (default 1ms)
    // This controls how often the processor checks timers
    void setUpdateIntervalMs(int intervalMs);

private:
    // The processor thread main loop
    void processorLoop();
    
    // Update all timers by deltaMs and fire any that are ready
    void updateTimers(int deltaMs);
    
    // Fire a timer (post event to queue)
    void fireTimer(const TimerEntry& timer);
    
    // Remove inactive timers (cleanup)
    void cleanupInactiveTimers();
    
    // Calculate optimal sleep time until next timer fires (in milliseconds)
    int calculateNextWakeTime();
    
    std::shared_ptr<EventQueue> eventQueue_;
    std::map<int, TimerEntry> timers_;
    mutable std::mutex mutex_;
    
    std::thread processorThread_;
    std::atomic<bool> running_;
    std::atomic<bool> shouldStop_;
    std::atomic<int> updateIntervalMs_;
    
    int nextTimerId_;
    
    // Disable copy and assignment
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
};

} // namespace Terminal
} // namespace FasterBASIC

#endif // FASTERBASIC_TIMER_MANAGER_TERMINAL_H