-- math_plugin_runtime.lua
-- Lua runtime implementations for Math Extensions plugin
--
-- These functions are called by BASIC programs that use the Math Extensions plugin.
-- They must be loaded into the Lua runtime before executing compiled BASIC code.

-- =============================================================================
-- FACTORIAL(n) - Calculate factorial
-- =============================================================================
function math_factorial(n)
    if type(n) ~= "number" then
        error("FACTORIAL: argument must be a number")
    end

    n = math.floor(n)

    if n < 0 then
        error("FACTORIAL: negative numbers not supported")
    end

    if n > 20 then
        error("FACTORIAL: input too large (max 20 to avoid overflow)")
    end

    if n == 0 or n == 1 then
        return 1
    end

    local result = 1
    for i = 2, n do
        result = result * i
    end

    return result
end

-- =============================================================================
-- ISPRIME(n) - Check if number is prime
-- =============================================================================
function math_isprime(n)
    if type(n) ~= "number" then
        return false
    end

    n = math.floor(n)

    if n < 2 then
        return false
    end

    if n == 2 then
        return true
    end

    if n % 2 == 0 then
        return false
    end

    -- Check odd divisors up to sqrt(n)
    local limit = math.floor(math.sqrt(n))
    for i = 3, limit, 2 do
        if n % i == 0 then
            return false
        end
    end

    return true
end

-- =============================================================================
-- GCD(a, b) - Greatest common divisor (Euclidean algorithm)
-- =============================================================================
function math_gcd(a, b)
    if type(a) ~= "number" or type(b) ~= "number" then
        error("GCD: both arguments must be numbers")
    end

    a = math.floor(math.abs(a))
    b = math.floor(math.abs(b))

    -- Euclidean algorithm
    while b ~= 0 do
        local temp = b
        b = a % b
        a = temp
    end

    return a
end

-- =============================================================================
-- LCM(a, b) - Least common multiple
-- =============================================================================
function math_lcm(a, b)
    if type(a) ~= "number" or type(b) ~= "number" then
        error("LCM: both arguments must be numbers")
    end

    a = math.floor(math.abs(a))
    b = math.floor(math.abs(b))

    if a == 0 or b == 0 then
        return 0
    end

    -- LCM(a,b) = |a*b| / GCD(a,b)
    return (a * b) / math_gcd(a, b)
end

-- =============================================================================
-- CLAMP(value, min, max) - Constrain value to range
-- =============================================================================
function math_clamp(value, min_val, max_val)
    if type(value) ~= "number" or type(min_val) ~= "number" or type(max_val) ~= "number" then
        error("CLAMP: all arguments must be numbers")
    end

    if min_val > max_val then
        error("CLAMP: min must be less than or equal to max")
    end

    if value < min_val then
        return min_val
    elseif value > max_val then
        return max_val
    else
        return value
    end
end

-- =============================================================================
-- LERP(a, b, t) - Linear interpolation
-- =============================================================================
function math_lerp(a, b, t)
    if type(a) ~= "number" or type(b) ~= "number" or type(t) ~= "number" then
        error("LERP: all arguments must be numbers")
    end

    -- Standard lerp formula: a + (b - a) * t
    -- More precise than: a * (1 - t) + b * t
    return a + (b - a) * t
end

-- =============================================================================
-- FIB(n) - Fibonacci number (iterative approach for efficiency)
-- =============================================================================
function math_fib(n)
    if type(n) ~= "number" then
        error("FIB: argument must be a number")
    end

    n = math.floor(n)

    if n < 0 then
        error("FIB: negative indices not supported")
    end

    if n == 0 then
        return 0
    end

    if n == 1 then
        return 1
    end

    -- Iterative calculation (avoids stack overflow for large n)
    local a, b = 0, 1
    for i = 2, n do
        local temp = a + b
        a = b
        b = temp

        -- Check for overflow (Lua integers are 64-bit)
        if b < 0 then
            error("FIB: result too large (overflow at position " .. i .. ")")
        end
    end

    return b
end

-- =============================================================================
-- POW2(n) - Calculate 2^n (fast bit-shift when possible)
-- =============================================================================
function math_pow2(n)
    if type(n) ~= "number" then
        error("POW2: argument must be a number")
    end

    n = math.floor(n)

    if n < 0 then
        error("POW2: negative exponents not supported")
    end

    if n > 30 then
        error("POW2: exponent too large (max 30 to avoid overflow)")
    end

    -- Simple power calculation
    -- Could use bit.lshift(1, n) if LuaJIT bit operations available
    return 2 ^ n
end

-- =============================================================================
-- Plugin initialization (if needed)
-- =============================================================================

-- =============================================================================
-- BASIC-name aliases (FACTORIAL instead of math_factorial, etc.)
-- =============================================================================

FACTORIAL = math_factorial
ISPRIME = math_isprime
GCD = math_gcd
LCM = math_lcm
CLAMP = math_clamp
LERP = math_lerp
FIB = math_fib
POW2 = math_pow2

-- Register plugin info for debugging/introspection
if not _MATH_PLUGIN_LOADED then
    _MATH_PLUGIN_LOADED = true
    _MATH_PLUGIN_VERSION = "1.0.0"

    -- Optional: Print confirmation when loaded
    -- print("Math Extensions plugin runtime loaded")
end
