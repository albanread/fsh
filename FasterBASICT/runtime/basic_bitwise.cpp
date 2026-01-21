//
// basic_bitwise.cpp
// FasterBASIC - Bitwise Operations Runtime Implementation
//
// Implements bitwise operations for BASIC logical operators.
// All operations convert operands to 32-bit signed integers first.
//

#include "basic_bitwise.h"
#include <cmath>

// Helper function to convert double to 32-bit signed integer
// Uses truncation (towards zero) like BASIC INT() function
static inline int32_t to_int32(double value) {
    // Handle special cases
    if (std::isnan(value)) return 0;
    if (std::isinf(value)) return value > 0 ? INT32_MAX : INT32_MIN;
    
    // Truncate towards zero (BASIC semantics)
    return static_cast<int32_t>(std::trunc(value));
}

// Bitwise AND
int32_t basic_band(double a, double b) {
    return to_int32(a) & to_int32(b);
}

// Bitwise OR
int32_t basic_bor(double a, double b) {
    return to_int32(a) | to_int32(b);
}

// Bitwise XOR
int32_t basic_bxor(double a, double b) {
    return to_int32(a) ^ to_int32(b);
}

// Bitwise NOT
int32_t basic_bnot(double a) {
    return ~to_int32(a);
}

// Bitwise EQV (Equivalence)
// Returns NOT(a XOR b) - bits are same
int32_t basic_beqv(double a, double b) {
    return ~(to_int32(a) ^ to_int32(b));
}

// Bitwise IMP (Implication)
// Returns NOT(a) OR b - logical implication
int32_t basic_bimp(double a, double b) {
    return ~to_int32(a) | to_int32(b);
}

// Left shift
int32_t basic_shl(double a, double b) {
    int32_t ia = to_int32(a);
    int32_t shift = to_int32(b);
    
    // Clamp shift amount to valid range [0, 31]
    if (shift < 0) return ia;  // No shift for negative
    if (shift >= 32) return 0; // Shifted all bits out
    
    return ia << shift;
}

// Right shift (arithmetic - preserves sign)
int32_t basic_shr(double a, double b) {
    int32_t ia = to_int32(a);
    int32_t shift = to_int32(b);
    
    // Clamp shift amount to valid range [0, 31]
    if (shift < 0) return ia;  // No shift for negative
    if (shift >= 32) {
        // Arithmetic shift: all bits become sign bit
        return ia < 0 ? -1 : 0;
    }
    
    return ia >> shift;
}