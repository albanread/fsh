//
// unicode_runtime.cpp
// FasterBASIC - Unicode Runtime Library Implementation
//
// Provides UTF-8 to UTF-32 conversion and Unicode string operations
// for OPTION UNICODE mode.
//

#include "unicode_runtime.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

// =============================================================================
// UTF-8 Decoding/Encoding
// =============================================================================

// Get number of bytes in a UTF-8 sequence from the first byte
static inline int utf8_sequence_length(unsigned char first_byte) {
    if ((first_byte & 0x80) == 0x00) return 1;  // 0xxxxxxx
    if ((first_byte & 0xE0) == 0xC0) return 2;  // 110xxxxx
    if ((first_byte & 0xF0) == 0xE0) return 3;  // 1110xxxx
    if ((first_byte & 0xF8) == 0xF0) return 4;  // 11110xxx
    return 0; // Invalid
}

// Decode a UTF-8 sequence to a codepoint
static int32_t utf8_decode(const char* utf8, int* bytes_consumed) {
    const unsigned char* s = (const unsigned char*)utf8;
    int len = utf8_sequence_length(s[0]);
    
    if (len == 0) {
        *bytes_consumed = 1;
        return 0xFFFD; // Replacement character for invalid UTF-8
    }
    
    int32_t codepoint = 0;
    
    switch (len) {
        case 1:
            codepoint = s[0];
            break;
            
        case 2:
            if ((s[1] & 0xC0) != 0x80) {
                *bytes_consumed = 1;
                return 0xFFFD;
            }
            codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
            break;
            
        case 3:
            if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
                *bytes_consumed = 1;
                return 0xFFFD;
            }
            codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            break;
            
        case 4:
            if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) {
                *bytes_consumed = 1;
                return 0xFFFD;
            }
            codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | 
                       ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
            break;
    }
    
    *bytes_consumed = len;
    
    // Validate codepoint range
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        return 0xFFFD;
    }
    
    return codepoint;
}

// Encode a codepoint to UTF-8
static int utf8_encode(int32_t codepoint, char* out) {
    if (codepoint < 0 || codepoint > 0x10FFFF) {
        // Invalid codepoint - use replacement character
        out[0] = (char)0xEF;
        out[1] = (char)0xBF;
        out[2] = (char)0xBD;
        return 3;
    }
    
    if (codepoint < 0x80) {
        // 1-byte sequence (ASCII)
        out[0] = (char)codepoint;
        return 1;
    }
    else if (codepoint < 0x800) {
        // 2-byte sequence
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }
    else if (codepoint < 0x10000) {
        // 3-byte sequence
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            // Invalid surrogate range - use replacement character
            out[0] = (char)0xEF;
            out[1] = (char)0xBF;
            out[2] = (char)0xBD;
            return 3;
        }
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }
    else {
        // 4-byte sequence
        out[0] = (char)(0xF0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }
}

// =============================================================================
// Public API: UTF-8 / UTF-32 Conversion
// =============================================================================

int32_t* unicode_from_utf8(const char* utf8_str, int32_t* out_len) {
    if (!utf8_str || !out_len) {
        return nullptr;
    }
    
    // First pass: count codepoints
    int32_t count = 0;
    const char* p = utf8_str;
    while (*p) {
        int bytes_consumed = 0;
        utf8_decode(p, &bytes_consumed);
        p += bytes_consumed;
        count++;
    }
    
    // Allocate array
    int32_t* codepoints = (int32_t*)malloc(count * sizeof(int32_t));
    if (!codepoints) {
        return nullptr;
    }
    
    // Second pass: decode codepoints
    p = utf8_str;
    for (int32_t i = 0; i < count; i++) {
        int bytes_consumed = 0;
        codepoints[i] = utf8_decode(p, &bytes_consumed);
        p += bytes_consumed;
    }
    
    *out_len = count;
    return codepoints;
}

char* unicode_to_utf8(const int32_t* codepoints, int32_t len, int32_t* out_len) {
    if (!codepoints || len < 0 || !out_len) {
        return nullptr;
    }
    
    // Calculate required buffer size (worst case: 4 bytes per codepoint + null terminator)
    int32_t max_size = len * 4 + 1;
    char* buffer = (char*)malloc(max_size);
    if (!buffer) {
        return nullptr;
    }
    
    // Encode each codepoint
    int32_t pos = 0;
    for (int32_t i = 0; i < len; i++) {
        int bytes_written = utf8_encode(codepoints[i], buffer + pos);
        pos += bytes_written;
    }
    
    buffer[pos] = '\0';
    *out_len = pos;
    
    return buffer;
}

void unicode_free(void* ptr) {
    free(ptr);
}

// =============================================================================
// Unicode Case Conversion
// =============================================================================

// Simple case conversion for ASCII and Latin-1
// For full Unicode support, would need comprehensive case mapping tables
static int32_t simple_upper(int32_t cp) {
    // ASCII lowercase to uppercase
    if (cp >= 0x61 && cp <= 0x7A) {
        return cp - 32;
    }
    // Latin-1 supplement lowercase to uppercase
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) {
        return cp - 32;
    }
    // Greek lowercase to uppercase (basic range)
    if (cp >= 0x3B1 && cp <= 0x3C9) {
        return cp - 32;
    }
    // Cyrillic lowercase to uppercase
    if (cp >= 0x430 && cp <= 0x44F) {
        return cp - 32;
    }
    return cp;
}

static int32_t simple_lower(int32_t cp) {
    // ASCII uppercase to lowercase
    if (cp >= 0x41 && cp <= 0x5A) {
        return cp + 32;
    }
    // Latin-1 supplement uppercase to lowercase
    if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) {
        return cp + 32;
    }
    // Greek uppercase to lowercase (basic range)
    if (cp >= 0x391 && cp <= 0x3A9) {
        return cp + 32;
    }
    // Cyrillic uppercase to lowercase
    if (cp >= 0x410 && cp <= 0x42F) {
        return cp + 32;
    }
    return cp;
}

void unicode_upper(int32_t* codepoints, int32_t len) {
    if (!codepoints || len < 0) return;
    
    for (int32_t i = 0; i < len; i++) {
        codepoints[i] = simple_upper(codepoints[i]);
    }
}

void unicode_lower(int32_t* codepoints, int32_t len) {
    if (!codepoints || len < 0) return;
    
    for (int32_t i = 0; i < len; i++) {
        codepoints[i] = simple_lower(codepoints[i]);
    }
}

int32_t* unicode_upper_new(const int32_t* codepoints, int32_t len) {
    if (!codepoints || len < 0) return nullptr;
    
    int32_t* result = (int32_t*)malloc(len * sizeof(int32_t));
    if (!result) return nullptr;
    
    for (int32_t i = 0; i < len; i++) {
        result[i] = simple_upper(codepoints[i]);
    }
    
    return result;
}

int32_t* unicode_lower_new(const int32_t* codepoints, int32_t len) {
    if (!codepoints || len < 0) return nullptr;
    
    int32_t* result = (int32_t*)malloc(len * sizeof(int32_t));
    if (!result) return nullptr;
    
    for (int32_t i = 0; i < len; i++) {
        result[i] = simple_lower(codepoints[i]);
    }
    
    return result;
}

// =============================================================================
// Validation and Utilities
// =============================================================================

int unicode_is_valid_codepoint(int32_t codepoint) {
    if (codepoint < 0 || codepoint > 0x10FFFF) return 0;
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0; // Surrogate range
    return 1;
}

int unicode_codepoint_to_utf8_bytes(int32_t codepoint) {
    if (!unicode_is_valid_codepoint(codepoint)) return 0;
    
    if (codepoint < 0x80) return 1;
    if (codepoint < 0x800) return 2;
    if (codepoint < 0x10000) return 3;
    return 4;
}

int unicode_category(int32_t codepoint) {
    // Simplified Unicode categories
    // 0 = Other, 1 = Letter, 2 = Number, 3 = Space, 4 = Punctuation, 5 = Symbol
    
    // Space
    if (codepoint == 0x20 || codepoint == 0x09 || codepoint == 0x0A || 
        codepoint == 0x0D || codepoint == 0x0B || codepoint == 0x0C ||
        (codepoint >= 0x2000 && codepoint <= 0x200B)) {
        return 3;
    }
    
    // ASCII digits
    if (codepoint >= 0x30 && codepoint <= 0x39) return 2;
    
    // ASCII letters
    if ((codepoint >= 0x41 && codepoint <= 0x5A) || 
        (codepoint >= 0x61 && codepoint <= 0x7A)) {
        return 1;
    }
    
    // Latin-1 letters
    if (codepoint >= 0xC0 && codepoint <= 0xFF && 
        codepoint != 0xD7 && codepoint != 0xF7) {
        return 1;
    }
    
    // Greek, Cyrillic, etc. (simplified)
    if ((codepoint >= 0x370 && codepoint <= 0x3FF) ||  // Greek
        (codepoint >= 0x400 && codepoint <= 0x4FF) ||  // Cyrillic
        (codepoint >= 0x4E00 && codepoint <= 0x9FFF)) { // CJK Unified Ideographs
        return 1;
    }
    
    // ASCII punctuation
    if ((codepoint >= 0x21 && codepoint <= 0x2F) ||
        (codepoint >= 0x3A && codepoint <= 0x40) ||
        (codepoint >= 0x5B && codepoint <= 0x60) ||
        (codepoint >= 0x7B && codepoint <= 0x7E)) {
        return 4;
    }
    
    // Symbols
    if ((codepoint >= 0x2000 && codepoint <= 0x206F) ||  // General Punctuation
        (codepoint >= 0x2190 && codepoint <= 0x21FF) ||  // Arrows
        (codepoint >= 0x2200 && codepoint <= 0x22FF)) {  // Mathematical Operators
        return 5;
    }
    
    return 0; // Other
}

int unicode_is_space(int32_t codepoint) {
    return unicode_category(codepoint) == 3;
}

int unicode_is_letter(int32_t codepoint) {
    return unicode_category(codepoint) == 1;
}

int unicode_is_digit(int32_t codepoint) {
    return unicode_category(codepoint) == 2;
}

// =============================================================================
// Version Information
// =============================================================================

const char* unicode_version() {
    return "1.0.0";
}

const char* unicode_standard_version() {
    return "15.0 (simplified)";
}

// =============================================================================
// Accelerated String Operations
// =============================================================================

int32_t* unicode_concat(const int32_t* cp1, int32_t len1, 
                        const int32_t* cp2, int32_t len2, 
                        int32_t* out_len) {
    if (!cp1 || !cp2 || len1 < 0 || len2 < 0 || !out_len) {
        return nullptr;
    }
    
    int32_t total_len = len1 + len2;
    *out_len = total_len;
    
    if (total_len == 0) {
        return nullptr;
    }
    
    // Allocate result array
    int32_t* result = (int32_t*)malloc(total_len * sizeof(int32_t));
    if (!result) {
        return nullptr;
    }
    
    // Use memcpy for fast block copy
    if (len1 > 0) {
        memcpy(result, cp1, len1 * sizeof(int32_t));
    }
    if (len2 > 0) {
        memcpy(result + len1, cp2, len2 * sizeof(int32_t));
    }
    
    return result;
}

int32_t* unicode_substring(const int32_t* codepoints, int32_t start, 
                           int32_t length, int32_t src_len, 
                           int32_t* out_len) {
    if (!codepoints || !out_len || src_len < 0) {
        return nullptr;
    }
    
    // Convert from 1-based BASIC indexing to 0-based C indexing
    int32_t start_idx = start - 1;
    
    // Bounds checking
    if (start_idx < 0 || start_idx >= src_len || length <= 0) {
        *out_len = 0;
        return nullptr;
    }
    
    // Calculate actual length to copy
    int32_t actual_len = length;
    if (start_idx + actual_len > src_len) {
        actual_len = src_len - start_idx;
    }
    
    *out_len = actual_len;
    
    // Allocate result array
    int32_t* result = (int32_t*)malloc(actual_len * sizeof(int32_t));
    if (!result) {
        return nullptr;
    }
    
    // Use memcpy for fast block copy
    memcpy(result, codepoints + start_idx, actual_len * sizeof(int32_t));
    
    return result;
}

int unicode_compare(const int32_t* cp1, int32_t len1, 
                    const int32_t* cp2, int32_t len2) {
    if (!cp1 || !cp2) {
        return 0;
    }
    
    // Different lengths means not equal
    if (len1 != len2) {
        return 0;
    }
    
    // Empty arrays are equal
    if (len1 == 0) {
        return 1;
    }
    
    // Use memcmp for fast comparison
    return memcmp(cp1, cp2, len1 * sizeof(int32_t)) == 0 ? 1 : 0;
}

int32_t unicode_instr(const int32_t* haystack, int32_t haystack_len,
                      const int32_t* needle, int32_t needle_len,
                      int32_t start_pos) {
    if (!haystack || !needle || haystack_len <= 0 || needle_len <= 0) {
        return 0;
    }
    
    // Empty needle never found
    if (needle_len == 0) {
        return 0;
    }
    
    // Convert from 1-based to 0-based indexing
    int32_t start_idx = start_pos - 1;
    if (start_idx < 0) {
        start_idx = 0;
    }
    
    // Needle longer than remaining haystack
    if (start_idx + needle_len > haystack_len) {
        return 0;
    }
    
    // Search for needle in haystack
    // Optimization: use memcmp for comparing blocks instead of element-by-element
    for (int32_t i = start_idx; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len * sizeof(int32_t)) == 0) {
            // Found match, return 1-based position
            return i + 1;
        }
    }
    
    return 0;
}

int32_t* unicode_left(const int32_t* codepoints, int32_t src_len, 
                      int32_t n, int32_t* out_len) {
    if (!codepoints || !out_len || src_len < 0 || n < 0) {
        return nullptr;
    }
    
    // Calculate actual length to copy
    int32_t actual_len = n;
    if (actual_len > src_len) {
        actual_len = src_len;
    }
    
    *out_len = actual_len;
    
    if (actual_len == 0) {
        return nullptr;
    }
    
    // Allocate result array
    int32_t* result = (int32_t*)malloc(actual_len * sizeof(int32_t));
    if (!result) {
        return nullptr;
    }
    
    // Use memcpy for fast block copy
    memcpy(result, codepoints, actual_len * sizeof(int32_t));
    
    return result;
}

int32_t* unicode_right(const int32_t* codepoints, int32_t src_len, 
                       int32_t n, int32_t* out_len) {
    if (!codepoints || !out_len || src_len < 0 || n < 0) {
        return nullptr;
    }
    
    // Calculate actual length to copy
    int32_t actual_len = n;
    if (actual_len > src_len) {
        actual_len = src_len;
    }
    
    *out_len = actual_len;
    
    if (actual_len == 0) {
        return nullptr;
    }
    
    // Calculate starting position
    int32_t start_idx = src_len - actual_len;
    
    // Allocate result array
    int32_t* result = (int32_t*)malloc(actual_len * sizeof(int32_t));
    if (!result) {
        return nullptr;
    }
    
    // Use memcpy for fast block copy
    memcpy(result, codepoints + start_idx, actual_len * sizeof(int32_t));
    
    return result;
}

int32_t* unicode_repeat(int32_t codepoint, int32_t count, int32_t* out_len) {
    if (!out_len || count < 0) {
        return nullptr;
    }
    
    *out_len = count;
    
    if (count == 0) {
        return nullptr;
    }
    
    // Allocate result array
    int32_t* result = (int32_t*)malloc(count * sizeof(int32_t));
    if (!result) {
        return nullptr;
    }
    
    // Fill array with codepoint
    // Optimization: use loop unrolling for small counts, memcpy pattern for large
    if (count < 16) {
        // Simple loop for small counts
        for (int32_t i = 0; i < count; i++) {
            result[i] = codepoint;
        }
    } else {
        // For larger counts, fill first element then use doubling memcpy
        result[0] = codepoint;
        int32_t filled = 1;
        
        while (filled < count) {
            int32_t to_copy = filled;
            if (filled + to_copy > count) {
                to_copy = count - filled;
            }
            memcpy(result + filled, result, to_copy * sizeof(int32_t));
            filled += to_copy;
        }
    }
    
    return result;
}