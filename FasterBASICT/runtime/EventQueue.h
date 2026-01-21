#ifndef FASTERBASIC_EVENT_QUEUE_H
#define FASTERBASIC_EVENT_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <chrono>
#include <memory>

namespace FasterBASIC {

// Event types supported by the event system
enum class EventType {
    TIMER_AFTER,   // One-shot timer event
    TIMER_EVERY    // Repeating timer event
};

// Represents a queued event ready for dispatch
struct QueuedEvent {
    EventType type;
    std::string handlerName;  // Name of the BASIC subroutine to call
    int timerId;              // Unique timer ID for management
    
    QueuedEvent(EventType t, const std::string& handler, int id)
        : type(t), handlerName(handler), timerId(id) {}
};

// Thread-safe event queue for timer events
// The event processor thread posts events here, and the Lua main thread consumes them
class EventQueue {
public:
    EventQueue();
    ~EventQueue();
    
    // Post an event to the queue (called by event processor thread)
    // Thread-safe: can be called from any thread
    void post(const QueuedEvent& event);
    
    // Try to dequeue an event (non-blocking)
    // Returns true if an event was available, false otherwise
    // Thread-safe: typically called from Lua main thread
    bool tryDequeue(QueuedEvent& outEvent);
    
    // Check if the queue is empty
    // Thread-safe
    bool isEmpty() const;
    
    // Get the current queue size
    // Thread-safe
    size_t size() const;
    
    // Clear all events from the queue
    // Thread-safe
    void clear();
    
    // Wait for an event with timeout (blocking)
    // Returns true if an event was dequeued, false if timeout expired
    // timeout is in milliseconds
    // Thread-safe
    bool waitDequeue(QueuedEvent& outEvent, int timeoutMs);

private:
    std::queue<QueuedEvent> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
    
    // Disable copy and assignment
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
};

} // namespace FasterBASIC

#endif // FASTERBASIC_EVENT_QUEUE_H