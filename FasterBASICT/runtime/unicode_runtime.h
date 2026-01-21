//
// unicode_runtime.h
// FasterBASIC - Unicode Runtime Library
//
// Provides UTF-8 to UTF-32 conversion and Unicode string operations
// for OPTION UNICODE mode. Strings are represented as arrays of 32-bit
// codepoints in Lua tables for proper Unicode character semantics.
//

#ifndef UNICODE_RUNTIME_H
#define UNICODE_RUNTIME_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// UTF-8 / UTF-32 Conversion
// =============================================================================

/**
 * Convert UTF-8 string to array of UTF-32 codepoints
 * 
 * @param utf8_str Input UTF-8 encoded string (null-terminated)
 * @param out_len Output parameter: number of codepoints in result
 * @return Pointer to malloc'd array of codepoints (caller must free)
 *         Returns NULL on invalid UTF-8 or allocation failure
 */
int32_t* unicode_from_utf8(const char* utf8_str, int32_t* out_len);

/**
 * Convert array of UTF-32 codepoints to UTF-8 string
 * 
 * @param codepoints Array of UTF-32 codepoints
 * @param len Number of codepoints in array
 * @param out_len Output parameter: length of UTF-8 string in bytes
 * @return Pointer to malloc'd UTF-8 string (caller must free)
 *         Returns NULL on invalid codepoint or allocation failure
 */
char* unicode_to_utf8(const int32_t* codepoints, int32_t len, int32_t* out_len);

/**
 * Free memory allocated by unicode functions
 * 
 * @param ptr Pointer to memory allocated by unicode_from_utf8 or unicode_to_utf8
 */
void unicode_free(void* ptr);

// =============================================================================
// Unicode Case Conversion
// =============================================================================

/**
 * Convert codepoints to uppercase (in-place)
 * Uses Unicode case mapping tables for correct international text handling
 * 
 * @param codepoints Array of codepoints to convert (modified in-place)
 * @param len Number of codepoints
 */
void unicode_upper(int32_t* codepoints, int32_t len);

/**
 * Convert codepoints to lowercase (in-place)
 * Uses Unicode case mapping tables for correct international text handling
 * 
 * @param codepoints Array of codepoints to convert (modified in-place)
 * @param len Number of codepoints
 */
void unicode_lower(int32_t* codepoints, int32_t len);

/**
 * Convert codepoints to uppercase (allocates new array)
 * 
 * @param codepoints Input array of codepoints
 * @param len Number of codepoints
 * @return Pointer to malloc'd array of uppercase codepoints (caller must free)
 */
int32_t* unicode_upper_new(const int32_t* codepoints, int32_t len);

/**
 * Convert codepoints to lowercase (allocates new array)
 * 
 * @param codepoints Input array of codepoints
 * @param len Number of codepoints
 * @return Pointer to malloc'd array of lowercase codepoints (caller must free)
 */
int32_t* unicode_lower_new(const int32_t* codepoints, int32_t len);

// =============================================================================
// Validation and Utilities
// =============================================================================

/**
 * Check if a codepoint is valid Unicode
 * 
 * @param codepoint Codepoint to validate
 * @return 1 if valid, 0 if invalid
 */
int unicode_is_valid_codepoint(int32_t codepoint);

/**
 * Get number of UTF-8 bytes needed for a codepoint
 * 
 * @param codepoint Unicode codepoint
 * @return Number of bytes (1-4) or 0 if invalid
 */
int unicode_codepoint_to_utf8_bytes(int32_t codepoint);

/**
 * Get Unicode character category
 * Returns simplified category for basic classification
 * 
 * @param codepoint Unicode codepoint
 * @return Category code: 
 *         0 = Other
 *         1 = Letter
 *         2 = Number
 *         3 = Space
 *         4 = Punctuation
 *         5 = Symbol
 */
int unicode_category(int32_t codepoint);

/**
 * Check if codepoint is whitespace
 * 
 * @param codepoint Unicode codepoint
 * @return 1 if whitespace, 0 otherwise
 */
int unicode_is_space(int32_t codepoint);

/**
 * Check if codepoint is a letter
 * 
 * @param codepoint Unicode codepoint
 * @return 1 if letter, 0 otherwise
 */
int unicode_is_letter(int32_t codepoint);

/**
 * Check if codepoint is a digit
 * 
 * @param codepoint Unicode codepoint
 * @return 1 if digit, 0 otherwise
 */
int unicode_is_digit(int32_t codepoint);

// =============================================================================
// Accelerated String Operations
// =============================================================================

/**
 * Concatenate two codepoint arrays (optimized with memcpy)
 * 
 * @param cp1 First codepoint array
 * @param len1 Length of first array
 * @param cp2 Second codepoint array
 * @param len2 Length of second array
 * @param out_len Output parameter: length of result array
 * @return Pointer to malloc'd concatenated array (caller must free)
 */
int32_t* unicode_concat(const int32_t* cp1, int32_t len1, 
                        const int32_t* cp2, int32_t len2, 
                        int32_t* out_len);

/**
 * Extract substring from codepoint array (optimized with memcpy)
 * 
 * @param codepoints Source codepoint array
 * @param start Starting position (1-based, BASIC convention)
 * @param length Number of codepoints to extract
 * @param src_len Length of source array
 * @param out_len Output parameter: length of result array
 * @return Pointer to malloc'd substring array (caller must free)
 */
int32_t* unicode_substring(const int32_t* codepoints, int32_t start, 
                           int32_t length, int32_t src_len, 
                           int32_t* out_len);

/**
 * Compare two codepoint arrays for equality (optimized with memcmp)
 * 
 * @param cp1 First codepoint array
 * @param len1 Length of first array
 * @param cp2 Second codepoint array
 * @param len2 Length of second array
 * @return 1 if equal, 0 if not equal
 */
int unicode_compare(const int32_t* cp1, int32_t len1, 
                    const int32_t* cp2, int32_t len2);

/**
 * Find needle in haystack (optimized search)
 * 
 * @param haystack Array to search in
 * @param haystack_len Length of haystack
 * @param needle Array to search for
 * @param needle_len Length of needle
 * @param start_pos Starting position (1-based, BASIC convention)
 * @return Position where needle was found (1-based), or 0 if not found
 */
int32_t unicode_instr(const int32_t* haystack, int32_t haystack_len,
                      const int32_t* needle, int32_t needle_len,
                      int32_t start_pos);

/**
 * Copy a portion of codepoint array (left substring, optimized)
 * 
 * @param codepoints Source codepoint array
 * @param src_len Length of source array
 * @param n Number of codepoints to copy from left
 * @param out_len Output parameter: length of result array
 * @return Pointer to malloc'd array (caller must free)
 */
int32_t* unicode_left(const int32_t* codepoints, int32_t src_len, 
                      int32_t n, int32_t* out_len);

/**
 * Copy a portion of codepoint array (right substring, optimized)
 * 
 * @param codepoints Source codepoint array
 * @param src_len Length of source array
 * @param n Number of codepoints to copy from right
 * @param out_len Output parameter: length of result array
 * @return Pointer to malloc'd array (caller must free)
 */
int32_t* unicode_right(const int32_t* codepoints, int32_t src_len, 
                       int32_t n, int32_t* out_len);

/**
 * Repeat a codepoint n times (optimized)
 * 
 * @param codepoint Codepoint to repeat
 * @param count Number of times to repeat
 * @param out_len Output parameter: length of result array
 * @return Pointer to malloc'd array (caller must free)
 */
int32_t* unicode_repeat(int32_t codepoint, int32_t count, int32_t* out_len);

// =============================================================================
// Version Information
// =============================================================================

/**
 * Get Unicode runtime version string
 * 
 * @return Version string (e.g., "1.0.0")
 */
const char* unicode_version();

/**
 * Get supported Unicode version
 * 
 * @return Unicode version string (e.g., "15.0")
 */
const char* unicode_standard_version();

#ifdef __cplusplus
}
#endif

#endif // UNICODE_RUNTIME_H