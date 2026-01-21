//
// basic_bitwise.h
// FasterBASIC - Bitwise Operations Runtime
//
// Provides bitwise operations for BASIC logical operators when in BITWISE mode.
// These functions convert operands to 32-bit integers and perform bitwise operations.
//

#ifndef BASIC_BITWISE_H
#define BASIC_BITWISE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Bitwise AND
// Converts operands to 32-bit signed integers and performs bitwise AND
// Example: basic_band(5.7, 3.2) -> band(5, 3) -> 1
int32_t basic_band(double a, double b);

// Bitwise OR
// Converts operands to 32-bit signed integers and performs bitwise OR
// Example: basic_bor(5.7, 3.2) -> bor(5, 3) -> 7
int32_t basic_bor(double a, double b);

// Bitwise XOR (Exclusive OR)
// Converts operands to 32-bit signed integers and performs bitwise XOR
// Example: basic_bxor(5.7, 3.2) -> bxor(5, 3) -> 6
int32_t basic_bxor(double a, double b);

// Bitwise NOT
// Converts operand to 32-bit signed integer and performs bitwise NOT
// Example: basic_bnot(5.7) -> bnot(5) -> -6 (two's complement)
int32_t basic_bnot(double a);

// Bitwise EQV (Equivalence)
// Returns NOT(a XOR b) - true if both bits are same
// Example: basic_beqv(5, 3) -> ~(5 ^ 3) -> ~6 -> -7
int32_t basic_beqv(double a, double b);

// Bitwise IMP (Implication)
// Returns NOT(a) OR b - false only if a is true and b is false
// Example: basic_bimp(5, 0) -> ~5 | 0 -> -6
int32_t basic_bimp(double a, double b);

// Left shift
// Shifts a left by b bits (equivalent to a * 2^b for positive shifts)
// Example: basic_shl(5, 2) -> 20
int32_t basic_shl(double a, double b);

// Right shift (arithmetic)
// Shifts a right by b bits, preserving sign (equivalent to a / 2^b)
// Example: basic_shr(20, 2) -> 5, basic_shr(-20, 2) -> -5
int32_t basic_shr(double a, double b);

#ifdef __cplusplus
}
#endif

#endif // BASIC_BITWISE_H