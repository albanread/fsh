#include "TimerManager_terminal.h"
#include <algorithm>
#include <stdexcept>

namespace FasterBASIC {
namespace Terminal {

TimerManager::TimerManager()
    : running_(false), shouldStop_(false), updateIntervalMs_(1), nextTimerId_(1) {
}

TimerManager::~TimerManager() {
    stop();
}

void TimerManager::initialize(std::shared_ptr<EventQueue> queue) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventQueue_ = queue;
}

int TimerManager::registerAfter(int durationMs, const std::string& handlerName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int timerId = nextTimerId_++;
    timers_.emplace(timerId, TimerEntry(timerId, EventType::TIMER_AFTER, handlerName, durationMs));
    
    return timerId;
}

int TimerManager::registerEvery(int durationMs, const std::string& handlerName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int timerId = nextTimerId_++;
    timers_.emplace(timerId, TimerEntry(timerId, EventType::TIMER_EVERY, handlerName, durationMs));
    
    return timerId;
}

void TimerManager::stopTimer(int timerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = timers_.find(timerId);
    if (it != timers_.end()) {
        it->second.active = false;
    }
}

void TimerManager::stopTimerByHandler(const std::string& handlerName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : timers_) {
        if (pair.second.handlerName == handlerName) {
            pair.second.active = false;
        }
    }
}

void TimerManager::stopAllTimers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : timers_) {
        pair.second.active = false;
    }
}

bool TimerManager::isTimerActive(int timerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = timers_.find(timerId);
    if (it != timers_.end()) {
        return it->second.active;
    }
    return false;
}

int TimerManager::getActiveTimerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int count = 0;
    for (const auto& pair : timers_) {
        if (pair.second.active) {
            count++;
        }
    }
    return count;
}

void TimerManager::start() {
    if (running_) {
        return;  // Already running
    }
    
    if (!eventQueue_) {
        throw std::runtime_error("TimerManager::start() - EventQueue not initialized");
    }
    
    shouldStop_ = false;
    running_ = true;
    processorThread_ = std::thread(&TimerManager::processorLoop, this);
}

void TimerManager::stop() {
    if (!running_) {
        return;  // Not running
    }
    
    shouldStop_ = true;
    
    if (processorThread_.joinable()) {
        processorThread_.join();
    }
    
    running_ = false;
}

bool TimerManager::isRunning() const {
    return running_;
}

void TimerManager::setUpdateIntervalMs(int intervalMs) {
    updateIntervalMs_ = intervalMs;
}

void TimerManager::processorLoop() {
    const int MIN_SLEEP_MS = 10;  // Minimum sleep time: 10ms
    const int MAX_SLEEP_MS = 100; // Maximum sleep time: 100ms
    
    auto lastUpdate = std::chrono::steady_clock::now();
    auto lastCleanup = lastUpdate;
    
    while (!shouldStop_) {
        // Calculate delta time
        auto now = std::chrono::steady_clock::now();
        auto deltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastUpdate
        ).count();
        lastUpdate = now;
        
        // Update timers if enough time has passed
        if (deltaMs > 0) {
            updateTimers(static_cast<int>(deltaMs));
        }
        
        // Periodic cleanup of inactive timers (every 5 seconds)
        auto timeSinceCleanup = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastCleanup
        ).count();
        if (timeSinceCleanup >= 5000) {
            cleanupInactiveTimers();
            lastCleanup = now;
        }
        
        // Calculate optimal sleep time based on next timer
        int sleepMs = calculateNextWakeTime();
        
        // Clamp to min/max range
        if (sleepMs < MIN_SLEEP_MS) {
            sleepMs = MIN_SLEEP_MS;
        } else if (sleepMs > MAX_SLEEP_MS) {
            sleepMs = MAX_SLEEP_MS;
        }
        
        // Sleep until next check
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

int TimerManager::calculateNextWakeTime() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int minWakeTime = 100; // Default to 100ms if no timers
    
    for (const auto& pair : timers_) {
        const TimerEntry& timer = pair.second;
        
        if (!timer.active) {
            continue;
        }
        
        // Calculate milliseconds until fire
        int remaining = timer.durationMs - timer.elapsedMs;
        if (remaining < 0) {
            minWakeTime = 0; // Ready to fire now
            break;
        }
        
        // Track minimum wake time
        if (remaining < minWakeTime) {
            minWakeTime = remaining;
        }
    }
    
    return minWakeTime;
}

void TimerManager::updateTimers(int deltaMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : timers_) {
        TimerEntry& timer = pair.second;
        
        if (!timer.active) {
            continue;
        }
        
        // Update elapsed time
        timer.elapsedMs += deltaMs;
        
        if (timer.elapsedMs >= timer.durationMs) {
            // Fire the timer
            fireTimer(timer);
            
            // Handle timer based on type
            if (timer.type == EventType::TIMER_AFTER) {
                // One-shot timer - deactivate after firing
                timer.active = false;
            } else if (timer.type == EventType::TIMER_EVERY) {
                // Repeating timer - reset elapsed time
                // Preserve any overflow to maintain accuracy
                timer.elapsedMs = timer.elapsedMs % timer.durationMs;
            }
        }
    }
}

void TimerManager::fireTimer(const TimerEntry& timer) {
    if (eventQueue_) {
        QueuedEvent event(timer.type, timer.handlerName, timer.timerId);
        eventQueue_->post(event);
    }
}

void TimerManager::cleanupInactiveTimers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove all inactive timers
    auto it = timers_.begin();
    while (it != timers_.end()) {
        if (!it->second.active) {
            it = timers_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Terminal
} // namespace FasterBASIC