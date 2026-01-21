//
// console_stubs.cpp
// Console-mode stubs for functions that only exist in GUI SuperTerminal
//

#include <cstdint>

// Stub for st_get_frame_count - only needed in GUI mode
// Return 0 to indicate frame counter is not available
// TimerManager should handle this gracefully
extern "C" uint64_t st_get_frame_count(void) {
    return 0;
}

// Note: Frame-based timers (AFTER/EVERY FRAMES) will not work in console mode
// Time-based timers (AFTER/EVERY SECS/MS) will work normally