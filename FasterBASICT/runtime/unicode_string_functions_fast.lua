--
-- unicode_string_functions_fast.lua
-- FasterBASIC Unicode String Functions Library (Zero-Copy Version)
--
-- Provides Unicode-aware string manipulation functions for OPTION UNICODE mode.
-- Uses FFI cdata arrays for zero-copy native performance.
--

local unicode_string_lib = {}

-- Load the zero-copy unicode implementation
local unicode_fast = require('unicode_string_fast')
local UString = unicode_fast.UString

-- Also load the FFI bindings for compatibility functions
local unicode = require('unicode_ffi_bindings')

if not unicode.available then
    error("Unicode runtime library not available - cannot use OPTION UNICODE")
end

-- =============================================================================
-- Helper Functions
-- =============================================================================

local function ensure_ustring(val)
    if type(val) == "table" and getmetatable(val) == UString then
        -- Already a UString
        return val
    elseif type(val) == "string" then
        -- Convert UTF-8 string to UString
        return UString.from_utf8(val)
    elseif type(val) == "number" then
        -- Single codepoint
        return UString.chr(val)
    elseif type(val) == "table" then
        -- Legacy codepoint array
        return UString.from_table(val)
    else
        -- Empty string
        return UString.from_utf8("")
    end
end

local function ensure_int(val)
    if type(val) == "number" then
        return math.floor(val)
    elseif type(val) == "string" then
        return math.floor(tonumber(val) or 0)
    else
        return 0
    end
end

-- Convert result to appropriate format
-- For now, keep returning UString objects (handles)
local function to_result(ustr)
    return ustr
end

-- =============================================================================
-- String Comparison Functions
-- =============================================================================

-- Compare two Unicode strings for equality (content comparison, not reference)
function unicode_string_equal(a, b)
    -- Handle nil cases
    if a == nil and b == nil then
        return true
    end
    if a == nil or b == nil then
        return false
    end

    -- Convert to UString
    local ustr_a = ensure_ustring(a)
    local ustr_b = ensure_ustring(b)

    -- Use zero-copy comparison
    return ustr_a:equals(ustr_b)
end

-- Compare two Unicode strings lexicographically
-- Returns: -1 if a < b, 0 if a == b, 1 if a > b
function unicode_string_compare(a, b)
    local ustr_a = ensure_ustring(a)
    local ustr_b = ensure_ustring(b)

    -- Fast equality check
    if ustr_a:equals(ustr_b) then
        return 0
    end

    -- Compare codepoint by codepoint
    local len = math.min(ustr_a.len, ustr_b.len)
    for i = 0, len - 1 do
        if ustr_a.data[i] < ustr_b.data[i] then
            return -1
        elseif ustr_a.data[i] > ustr_b.data[i] then
            return 1
        end
    end

    -- All compared codepoints are equal, check lengths
    if ustr_a.len < ustr_b.len then
        return -1
    elseif ustr_a.len > ustr_b.len then
        return 1
    else
        return 0
    end
end

-- =============================================================================
-- String Manipulation Functions
-- =============================================================================

-- LEN(string) - Get length of Unicode string
function unicode_len(s)
    if s == nil then
        return 0
    end
    local ustr = ensure_ustring(s)
    return ustr.len
end

-- LEFT$(string, n) - Get first n characters
function unicode_left(s, n)
    local ustr = ensure_ustring(s)
    n = ensure_int(n)
    return to_result(ustr:left(n))
end

-- RIGHT$(string, n) - Get last n characters
function unicode_right(s, n)
    local ustr = ensure_ustring(s)
    n = ensure_int(n)
    return to_result(ustr:right(n))
end

-- MID$(string, start, [length]) - Get substring
function unicode_mid(s, start, length)
    local ustr = ensure_ustring(s)
    start = ensure_int(start)
    length = length and ensure_int(length) or (ustr.len - start + 1)
    return to_result(ustr:mid(start, length))
end

-- String concatenation
function unicode_concat(a, b)
    local ustr_a = ensure_ustring(a)
    local ustr_b = ensure_ustring(b)
    return to_result(ustr_a:concat(ustr_b))
end

-- UPPER$(string) - Convert to uppercase
function unicode_upper(s)
    local ustr = ensure_ustring(s)
    return to_result(ustr:upper())
end

-- LOWER$(string) - Convert to lowercase
function unicode_lower(s)
    local ustr = ensure_ustring(s)
    return to_result(ustr:lower())
end

-- REVERSE$(string) - Reverse string
function unicode_reverse(s)
    local ustr = ensure_ustring(s)
    local len = ustr.len
    if len == 0 then
        return to_result(ustr)
    end

    -- Create reversed array
    local ffi = require('ffi')
    local result = ffi.cast("int32_t*", ffi.C.malloc(len * 4))
    if result == nil then
        error("Failed to allocate memory for reverse")
    end

    -- Reverse copy
    for i = 0, len - 1 do
        result[i] = ustr.data[len - 1 - i]
    end

    return to_result(UString.new(result, len, true))
end

-- INSTR([start,] haystack, needle) - Find substring
function unicode_instr(...)
    local args = { ... }
    local start, haystack, needle

    if #args == 2 then
        -- INSTR(haystack, needle)
        start = 1
        haystack = args[1]
        needle = args[2]
    elseif #args == 3 then
        -- INSTR(start, haystack, needle)
        start = ensure_int(args[1])
        haystack = args[2]
        needle = args[3]
    else
        error("INSTR requires 2 or 3 arguments")
    end

    local ustr_haystack = ensure_ustring(haystack)
    local ustr_needle = ensure_ustring(needle)

    return ustr_haystack:instr(ustr_needle, start)
end

-- CHR$(codepoint) - Create single-character string
function unicode_chr(codepoint)
    codepoint = ensure_int(codepoint)
    return to_result(UString.chr(codepoint))
end

-- ASC(string) - Get first codepoint
function unicode_asc(s)
    local ustr = ensure_ustring(s)
    return ustr:asc()
end

-- SPACE$(n) - Create string of n spaces
function unicode_space(n)
    n = ensure_int(n)
    return to_result(UString.space(n))
end

-- STRING$(n, codepoint) - Repeat character n times
function unicode_string(n, cp)
    n = ensure_int(n)

    if type(cp) == "string" or (type(cp) == "table" and getmetatable(cp) == UString) then
        -- STRING$(n, string) - repeat first character
        local ustr = ensure_ustring(cp)
        if ustr.len == 0 then
            return to_result(UString.from_utf8(""))
        end
        cp = ustr.data[0]
    else
        cp = ensure_int(cp)
    end

    return to_result(UString.repeat_char(cp, n))
end

-- TRIM$(string) - Remove leading and trailing whitespace
function unicode_trim(s)
    local ustr = ensure_ustring(s)

    if ustr.len == 0 then
        return to_result(ustr)
    end

    -- Find first non-space
    local start_idx = 0
    while start_idx < ustr.len and unicode.is_space(ustr.data[start_idx]) do
        start_idx = start_idx + 1
    end

    if start_idx >= ustr.len then
        -- All spaces
        return to_result(UString.from_utf8(""))
    end

    -- Find last non-space
    local end_idx = ustr.len - 1
    while end_idx >= 0 and unicode.is_space(ustr.data[end_idx]) do
        end_idx = end_idx - 1
    end

    -- Extract trimmed substring (1-based for substring function)
    local len = end_idx - start_idx + 1
    return to_result(ustr:substring(start_idx + 1, len))
end

-- LTRIM$(string) - Remove leading whitespace
function unicode_ltrim(s)
    local ustr = ensure_ustring(s)

    if ustr.len == 0 then
        return to_result(ustr)
    end

    -- Find first non-space
    local start_idx = 0
    while start_idx < ustr.len and unicode.is_space(ustr.data[start_idx]) do
        start_idx = start_idx + 1
    end

    if start_idx >= ustr.len then
        return to_result(UString.from_utf8(""))
    end

    local len = ustr.len - start_idx
    return to_result(ustr:substring(start_idx + 1, len))
end

-- RTRIM$(string) - Remove trailing whitespace
function unicode_rtrim(s)
    local ustr = ensure_ustring(s)

    if ustr.len == 0 then
        return to_result(ustr)
    end

    -- Find last non-space
    local end_idx = ustr.len - 1
    while end_idx >= 0 and unicode.is_space(ustr.data[end_idx]) do
        end_idx = end_idx - 1
    end

    if end_idx < 0 then
        return to_result(UString.from_utf8(""))
    end

    local len = end_idx + 1
    return to_result(ustr:substring(1, len))
end

-- =============================================================================
-- Conversion Functions
-- =============================================================================

-- BIN$(number, [width]) - Convert to binary string
function unicode_bin(num, width)
    num = ensure_int(num)
    width = width and ensure_int(width) or nil

    if num < 0 then
        num = num + 0x100000000 -- Convert to unsigned 32-bit
    end

    local binary = ""
    if num == 0 then
        binary = "0"
    else
        while num > 0 do
            binary = (num % 2) .. binary
            num = math.floor(num / 2)
        end
    end

    if width then
        binary = string.rep("0", math.max(0, width - #binary)) .. binary
    end

    return to_result(UString.from_utf8(binary))
end

-- HEX$(number, [width]) - Convert to hexadecimal string
function unicode_hex(num, width)
    num = ensure_int(num)
    width = width and ensure_int(width) or nil

    if num < 0 then
        num = num + 0x100000000 -- Convert to unsigned 32-bit
    end

    local hex = string.format("%X", num)

    if width then
        hex = string.rep("0", math.max(0, width - #hex)) .. hex
    end

    return to_result(UString.from_utf8(hex))
end

-- STR$(number) - Convert number to string
function unicode_str(num)
    local str = tostring(num)
    -- Add leading space for positive numbers (BASIC convention)
    if type(num) == "number" and num >= 0 then
        str = " " .. str
    end
    return to_result(UString.from_utf8(str))
end

-- VAL(string) - Convert string to number
function unicode_val(s)
    local ustr = ensure_ustring(s)
    if ustr.len == 0 then
        return 0
    end

    -- Convert to UTF-8 for parsing
    local utf8_str = ustr:to_utf8()
    return tonumber(utf8_str) or 0
end

-- =============================================================================
-- Utility Functions
-- =============================================================================

-- Convert UString to UTF-8 for output
function unicode_to_utf8(s)
    if s == nil then
        return ""
    end
    local ustr = ensure_ustring(s)
    return ustr:to_utf8()
end

-- Convert UTF-8 to UString
function unicode_from_utf8(utf8_str)
    if type(utf8_str) ~= "string" then
        utf8_str = tostring(utf8_str or "")
    end
    return to_result(UString.from_utf8(utf8_str))
end

-- Convert legacy codepoint table to UString
function unicode_from_table(tbl)
    if type(tbl) ~= "table" then
        return to_result(UString.from_utf8(""))
    end
    return to_result(UString.from_table(tbl))
end

-- Convert UString to legacy codepoint table (for compatibility)
function unicode_to_table(s)
    local ustr = ensure_ustring(s)
    return ustr:to_table()
end

-- Check if value is a UString handle
function unicode_is_ustring(val)
    return type(val) == "table" and getmetatable(val) == UString
end

-- =============================================================================
-- Batch Operations (Performance Optimization)
-- =============================================================================

-- Concatenate multiple strings efficiently
function unicode_concat_many(...)
    local args = { ... }
    local ustrings = {}

    for i, arg in ipairs(args) do
        ustrings[i] = ensure_ustring(arg)
    end

    return to_result(unicode_fast.concat_many(ustrings))
end

-- Create string builder for repeated concatenations
function unicode_string_builder()
    return unicode_fast.StringBuilder.new()
end

-- =============================================================================
-- Export Functions
-- =============================================================================

-- Make functions globally available (for BASIC runtime)
_G.unicode_string_equal = unicode_string_equal
_G.unicode_string_compare = unicode_string_compare
_G.unicode_len = unicode_len
_G.unicode_left = unicode_left
_G.unicode_right = unicode_right
_G.unicode_mid = unicode_mid
_G.unicode_concat = unicode_concat
_G.unicode_upper = unicode_upper
_G.unicode_lower = unicode_lower
_G.unicode_reverse = unicode_reverse
_G.unicode_instr = unicode_instr
_G.unicode_chr = unicode_chr
_G.unicode_asc = unicode_asc
_G.unicode_space = unicode_space
_G.unicode_string = unicode_string
_G.unicode_trim = unicode_trim
_G.unicode_ltrim = unicode_ltrim
_G.unicode_rtrim = unicode_rtrim
_G.unicode_bin = unicode_bin
_G.unicode_hex = unicode_hex
_G.unicode_str = unicode_str
_G.unicode_val = unicode_val
_G.unicode_to_utf8 = unicode_to_utf8
_G.unicode_from_utf8 = unicode_from_utf8
_G.unicode_from_table = unicode_from_table
_G.unicode_to_table = unicode_to_table
_G.unicode_is_ustring = unicode_is_ustring
_G.unicode_concat_many = unicode_concat_many
_G.unicode_string_builder = unicode_string_builder

-- Also export as module
unicode_string_lib.equal = unicode_string_equal
unicode_string_lib.compare = unicode_string_compare
unicode_string_lib.len = unicode_len
unicode_string_lib.left = unicode_left
unicode_string_lib.right = unicode_right
unicode_string_lib.mid = unicode_mid
unicode_string_lib.concat = unicode_concat
unicode_string_lib.upper = unicode_upper
unicode_string_lib.lower = unicode_lower
unicode_string_lib.reverse = unicode_reverse
unicode_string_lib.instr = unicode_instr
unicode_string_lib.chr = unicode_chr
unicode_string_lib.asc = unicode_asc
unicode_string_lib.space = unicode_space
unicode_string_lib.string = unicode_string
unicode_string_lib.trim = unicode_trim
unicode_string_lib.ltrim = unicode_ltrim
unicode_string_lib.rtrim = unicode_rtrim
unicode_string_lib.bin = unicode_bin
unicode_string_lib.hex = unicode_hex
unicode_string_lib.str = unicode_str
unicode_string_lib.val = unicode_val
unicode_string_lib.to_utf8 = unicode_to_utf8
unicode_string_lib.from_utf8 = unicode_from_utf8
unicode_string_lib.from_table = unicode_from_table
unicode_string_lib.to_table = unicode_to_table
unicode_string_lib.is_ustring = unicode_is_ustring
unicode_string_lib.concat_many = unicode_concat_many
unicode_string_lib.string_builder = unicode_string_builder

-- Export UString class for advanced usage
unicode_string_lib.UString = UString

return unicode_string_lib
