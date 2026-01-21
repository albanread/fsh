-- math_functions.lua
-- FasterBASICT Runtime Math Functions Library
-- Provides BCX-compatible advanced math functions

local math_lib = {}

-- Helper function to ensure numeric type
local function ensure_number(val)
    if type(val) == "number" then
        return val
    elseif type(val) == "string" then
        return tonumber(val) or 0
    else
        return 0
    end
end

-- =============================================================================
-- Power Function
-- =============================================================================

-- POW - Power function (x^y)
-- Lua 5.1/LuaJIT doesn't have math.pow, use ^ operator
function math_pow(x, y)
    x = ensure_number(x)
    y = ensure_number(y)
    return x ^ y
end

-- =============================================================================
-- Rounding and Truncation Functions
-- =============================================================================

-- ROUND - Round to n decimal places
function math_round(x, places)
    x = ensure_number(x)
    places = places and ensure_number(places) or 0

    if places == 0 then
        -- Round to nearest integer
        return math.floor(x + 0.5)
    else
        -- Round to n decimal places
        local mult = 10 ^ places
        return math.floor(x * mult + 0.5) / mult
    end
end

-- FRAC - Fractional part (x - floor(x))
function math_frac(x)
    x = ensure_number(x)
    return x - math.floor(x)
end

-- =============================================================================
-- Hyperbolic Trigonometric Functions
-- =============================================================================

-- SINH - Hyperbolic sine
function math_sinh(x)
    x = ensure_number(x)
    return (math.exp(x) - math.exp(-x)) / 2
end

-- COSH - Hyperbolic cosine
function math_cosh(x)
    x = ensure_number(x)
    return (math.exp(x) + math.exp(-x)) / 2
end

-- TANH - Hyperbolic tangent
function math_tanh(x)
    x = ensure_number(x)
    local exp_pos = math.exp(x)
    local exp_neg = math.exp(-x)
    return (exp_pos - exp_neg) / (exp_pos + exp_neg)
end

-- =============================================================================
-- Inverse Hyperbolic Functions
-- =============================================================================

-- ASINH - Inverse hyperbolic sine
function math_asinh(x)
    x = ensure_number(x)
    return math.log(x + math.sqrt(x * x + 1))
end

-- ACOSH - Inverse hyperbolic cosine
function math_acosh(x)
    x = ensure_number(x)
    if x < 1 then
        error("ACOSH: argument must be >= 1")
    end
    return math.log(x + math.sqrt(x * x - 1))
end

-- ATANH - Inverse hyperbolic tangent
function math_atanh(x)
    x = ensure_number(x)
    if x <= -1 or x >= 1 then
        error("ATANH: argument must be in range (-1, 1)")
    end
    return 0.5 * math.log((1 + x) / (1 - x))
end

-- =============================================================================
-- Logarithm Functions
-- =============================================================================

-- LOG10 - Base-10 logarithm
function math_log10(x)
    x = ensure_number(x)
    if x <= 0 then
        error("LOG10: argument must be > 0")
    end
    -- Lua 5.1 compatible: log10(x) = log(x) / log(10)
    return math.log(x) / math.log(10)
end

-- =============================================================================
-- Number Base Conversion Functions
-- =============================================================================

-- BIN2DEC - Convert binary string to decimal
function math_bin2dec(binStr)
    if type(binStr) ~= "string" then
        binStr = tostring(binStr)
    end

    -- Remove common prefixes
    binStr = binStr:gsub("^0b", ""):gsub("^0B", "")

    -- Remove whitespace
    binStr = binStr:gsub("%s+", "")

    -- Validate binary string
    if not binStr:match("^[01]+$") then
        error("BIN2DEC: invalid binary string")
    end

    -- Convert to decimal
    local result = 0
    local len = #binStr

    for i = 1, len do
        local bit = binStr:sub(i, i)
        if bit == "1" then
            result = result + 2 ^ (len - i)
        end
    end

    return result
end

-- HEX2DEC - Convert hexadecimal string to decimal
function math_hex2dec(hexStr)
    if type(hexStr) ~= "string" then
        hexStr = tostring(hexStr)
    end

    -- Remove common prefixes
    hexStr = hexStr:gsub("^0x", ""):gsub("^0X", "")
    hexStr = hexStr:gsub("^&h", ""):gsub("^&H", "")

    -- Remove whitespace
    hexStr = hexStr:gsub("%s+", "")

    -- Validate hex string
    if not hexStr:match("^[0-9a-fA-F]+$") then
        error("HEX2DEC: invalid hexadecimal string")
    end

    -- Convert to decimal using Lua's built-in
    return tonumber(hexStr, 16)
end

-- OCT2DEC - Convert octal string to decimal
function math_oct2dec(octStr)
    if type(octStr) ~= "string" then
        octStr = tostring(octStr)
    end

    -- Remove common prefixes
    octStr = octStr:gsub("^0o", ""):gsub("^0O", "")
    octStr = octStr:gsub("^&o", ""):gsub("^&O", "")

    -- Remove whitespace
    octStr = octStr:gsub("%s+", "")

    -- Validate octal string
    if not octStr:match("^[0-7]+$") then
        error("OCT2DEC: invalid octal string")
    end

    -- Convert to decimal using Lua's built-in
    return tonumber(octStr, 8)
end

-- =============================================================================
-- Type Conversion Functions (BCX compatibility)
-- =============================================================================

-- CINT - Convert to integer (rounded, not truncated)
function math_cint(x)
    x = ensure_number(x)
    -- Round to nearest integer (banker's rounding)
    if x >= 0 then
        return math.floor(x + 0.5)
    else
        return math.ceil(x - 0.5)
    end
end

-- CLNG - Convert to long integer (same as CINT in Lua)
function math_clng(x)
    return math_cint(x)
end

-- NTH - Check if counter is at every Nth occurrence
-- Returns -1 (BASIC true) if count MOD n = 0, otherwise 0 (BASIC false)
function math_nth(count, n)
    count = ensure_number(count)
    n = ensure_number(n)

    if n == 0 then
        error("NTH: interval cannot be zero")
    end

    -- Return BASIC boolean: -1 for true, 0 for false
    if count % n == 0 then
        return -1
    else
        return 0
    end
end

-- =============================================================================
-- Advanced Math Utility Functions
-- =============================================================================

-- GCD - Greatest Common Divisor
function math_gcd(a, b)
    a = math.abs(ensure_number(a))
    b = math.abs(ensure_number(b))

    -- Handle special cases
    if a == 0 then return b end
    if b == 0 then return a end

    -- Euclidean algorithm
    while b ~= 0 do
        local temp = b
        b = a % b
        a = temp
    end

    return a
end

-- LCM - Least Common Multiple
function math_lcm(a, b)
    a = math.abs(ensure_number(a))
    b = math.abs(ensure_number(b))

    -- Handle special cases
    if a == 0 or b == 0 then return 0 end

    -- LCM(a,b) = |a*b| / GCD(a,b)
    return (a * b) / math_gcd(a, b)
end

-- CLAMP - Clamp value between min and max
function math_clamp(x, min_val, max_val)
    x = ensure_number(x)
    min_val = ensure_number(min_val)
    max_val = ensure_number(max_val)

    if x < min_val then return min_val end
    if x > max_val then return max_val end
    return x
end

-- LERP - Linear interpolation
function math_lerp(a, b, t)
    a = ensure_number(a)
    b = ensure_number(b)
    t = ensure_number(t)

    return a + (b - a) * t
end

-- MAP - Map value from one range to another
function math_map(x, in_min, in_max, out_min, out_max)
    x = ensure_number(x)
    in_min = ensure_number(in_min)
    in_max = ensure_number(in_max)
    out_min = ensure_number(out_min)
    out_max = ensure_number(out_max)

    -- Prevent division by zero
    if in_max == in_min then
        return out_min
    end

    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
end

-- =============================================================================
-- Statistical Functions
-- =============================================================================

-- AVERAGE - Calculate average of numbers
function math_average(...)
    local args = { ... }
    if #args == 0 then return 0 end

    local sum = 0
    for i = 1, #args do
        sum = sum + ensure_number(args[i])
    end

    return sum / #args
end

-- MEDIAN - Calculate median of numbers
function math_median(...)
    local args = { ... }
    if #args == 0 then return 0 end

    -- Convert to numbers and sort
    local numbers = {}
    for i = 1, #args do
        numbers[i] = ensure_number(args[i])
    end
    table.sort(numbers)

    local len = #numbers
    if len % 2 == 0 then
        -- Even number of elements: average of middle two
        return (numbers[len / 2] + numbers[len / 2 + 1]) / 2
    else
        -- Odd number of elements: middle element
        return numbers[math.ceil(len / 2)]
    end
end

-- =============================================================================
-- Angle Conversion Helpers
-- =============================================================================

-- DEG2RAD - Convert degrees to radians (if not already in math lib)
function math_deg2rad(degrees)
    degrees = ensure_number(degrees)
    return degrees * math.pi / 180
end

-- RAD2DEG - Convert radians to degrees (if not already in math lib)
function math_rad2deg(radians)
    radians = ensure_number(radians)
    return radians * 180 / math.pi
end

-- =============================================================================
-- Export all functions to global scope (for BASIC runtime)
-- =============================================================================

-- Make all functions globally available for BASIC programs
_G.math_pow = math_pow
_G.math_round = math_round
_G.math_frac = math_frac
_G.math_sinh = math_sinh
_G.math_cosh = math_cosh
_G.math_tanh = math_tanh
_G.math_asinh = math_asinh
_G.math_acosh = math_acosh
_G.math_atanh = math_atanh
_G.math_log10 = math_log10
_G.math_bin2dec = math_bin2dec
_G.math_hex2dec = math_hex2dec
_G.math_oct2dec = math_oct2dec
_G.math_cint = math_cint
_G.math_clng = math_clng
_G.math_gcd = math_gcd
_G.math_lcm = math_lcm
_G.math_clamp = math_clamp
_G.math_lerp = math_lerp
_G.math_map = math_map
_G.math_average = math_average
_G.math_median = math_median
_G.math_deg2rad = math_deg2rad
_G.math_rad2deg = math_rad2deg

return math_lib
