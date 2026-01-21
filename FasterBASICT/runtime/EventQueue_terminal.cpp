#include "EventQueue_terminal.h"

namespace FasterBASIC {
namespace Terminal {

EventQueue::EventQueue() {
    // Constructor - nothing special needed, members initialize themselves
}

EventQueue::~EventQueue() {
    // Destructor - notify any waiting threads before destruction
    clear();
    condVar_.notify_all();
}

void EventQueue::post(const QueuedEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(event);
    condVar_.notify_one();  // Wake up one waiting thread if any
}

bool EventQueue::tryDequeue(QueuedEvent& outEvent) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return false;
    }
    
    outEvent = queue_.front();
    queue_.pop();
    return true;
}

bool EventQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

size_t EventQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void EventQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear the queue by swapping with an empty queue
    std::queue<QueuedEvent> emptyQueue;
    std::swap(queue_, emptyQueue);
}

bool EventQueue::waitDequeue(QueuedEvent& outEvent, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait until either:
    // 1. The queue is not empty, or
    // 2. The timeout expires
    auto timeout = std::chrono::milliseconds(timeoutMs);
    bool result = condVar_.wait_for(lock, timeout, [this]() {
        return !queue_.empty();
    });
    
    if (result && !queue_.empty()) {
        outEvent = queue_.front();
        queue_.pop();
        return true;
    }
    
    return false;
}

} // namespace Terminal
} // namespace FasterBASIC