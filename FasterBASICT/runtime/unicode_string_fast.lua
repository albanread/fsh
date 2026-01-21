--
-- unicode_string_fast.lua
-- FasterBASIC - Zero-Copy Unicode String Implementation
--
-- Uses FFI cdata arrays for UTF-32 codepoints instead of Lua tables.
-- This allows C functions to operate directly on the data without copying.
--
-- String representation: ffi cdata int32_t[] array
-- This can be accessed efficiently from both Lua and C code with ZERO copies!
--

local ffi = require('ffi')

-- Load unicode FFI bindings
local unicode = require('unicode_ffi_bindings')

-- =============================================================================
-- FFI Declarations for Direct Memory Access
-- =============================================================================

ffi.cdef [[
    void* memcpy(void* dest, const void* src, size_t n);
    void* memmove(void* dest, const void* src, size_t n);
    int memcmp(const void* s1, const void* s2, size_t n);
    void* malloc(size_t size);
    void free(void* ptr);
]]

-- =============================================================================
-- Unicode String Object (wraps FFI cdata)
-- =============================================================================

local UString = {}
UString.__index = UString

-- Create a new Unicode string from cdata array
-- @param cdata: FFI int32_t[] array
-- @param len: Length of array
-- @param owned: If true, this object owns the memory and will free it
function UString.new(cdata, len, owned)
    local obj = {
        data = cdata,          -- FFI cdata int32_t[]
        len = len or 0,        -- Length in codepoints
        owned = owned or false -- Whether we own the memory
    }
    return setmetatable(obj, UString)
end

-- Create from UTF-8 string (via C conversion)
function UString.from_utf8(utf8_str)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    local len_ptr = ffi.new("int32_t[1]")
    local cdata = unicode.lib.unicode_from_utf8(utf8_str, len_ptr)

    if cdata == nil then
        error("Failed to convert UTF-8 string")
    end

    local len = len_ptr[0]
    return UString.new(cdata, len, true) -- We own this memory
end

-- Convert to UTF-8 string
function UString:to_utf8()
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if self.len == 0 then
        return ""
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local utf8_ptr = unicode.lib.unicode_to_utf8(self.data, self.len, out_len_ptr)

    if utf8_ptr == nil then
        error("Failed to convert to UTF-8")
    end

    local result = ffi.string(utf8_ptr, out_len_ptr[0])
    unicode.lib.unicode_free(utf8_ptr)

    return result
end

-- Create from Lua table of codepoints
function UString.from_table(tbl)
    local len = #tbl
    if len == 0 then
        return UString.new(nil, 0, false)
    end

    -- Allocate FFI array
    local cdata = ffi.new("int32_t[?]", len)
    for i = 1, len do
        cdata[i - 1] = tbl[i]
    end

    return UString.new(cdata, len, false) -- FFI manages this memory
end

-- Convert to Lua table
function UString:to_table()
    local result = {}
    for i = 0, self.len - 1 do
        result[i + 1] = self.data[i]
    end
    return result
end

-- Garbage collection: free owned memory
function UString:__gc()
    if self.owned and self.data ~= nil then
        unicode.lib.unicode_free(self.data)
        self.data = nil
    end
end

-- =============================================================================
-- Zero-Copy String Operations
-- =============================================================================

-- Length (O(1))
function UString:length()
    return self.len
end

function UString:__len()
    return self.len
end

-- Concatenation (ZERO-COPY via C memcpy)
function UString:concat(other)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    local len1 = self.len
    local len2 = other.len

    if len1 == 0 and len2 == 0 then
        return UString.new(nil, 0, false)
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local result_ptr = unicode.lib.unicode_concat(
        self.data, len1,
        other.data, len2,
        out_len_ptr
    )

    if result_ptr == nil then
        return UString.new(nil, 0, false)
    end

    return UString.new(result_ptr, out_len_ptr[0], true)
end

function UString:__concat(other)
    if type(self) == "string" then
        self = UString.from_utf8(self)
    end
    if type(other) == "string" then
        other = UString.from_utf8(other)
    end
    return self:concat(other)
end

-- Substring (ZERO-COPY via C memcpy)
function UString:substring(start, length)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if self.len == 0 or length <= 0 or start < 1 or start > self.len then
        return UString.new(nil, 0, false)
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local result_ptr = unicode.lib.unicode_substring(
        self.data, start, length, self.len, out_len_ptr
    )

    if result_ptr == nil then
        return UString.new(nil, 0, false)
    end

    return UString.new(result_ptr, out_len_ptr[0], true)
end

-- MID$ alias
function UString:mid(start, length)
    return self:substring(start, length)
end

-- LEFT$ (ZERO-COPY via C memcpy)
function UString:left(n)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if self.len == 0 or n <= 0 then
        return UString.new(nil, 0, false)
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local result_ptr = unicode.lib.unicode_left(self.data, self.len, n, out_len_ptr)

    if result_ptr == nil then
        return UString.new(nil, 0, false)
    end

    return UString.new(result_ptr, out_len_ptr[0], true)
end

-- RIGHT$ (ZERO-COPY via C memcpy)
function UString:right(n)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if self.len == 0 or n <= 0 then
        return UString.new(nil, 0, false)
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local result_ptr = unicode.lib.unicode_right(self.data, self.len, n, out_len_ptr)

    if result_ptr == nil then
        return UString.new(nil, 0, false)
    end

    return UString.new(result_ptr, out_len_ptr[0], true)
end

-- Comparison (ZERO-COPY via C memcmp)
function UString:equals(other)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    -- Quick length check
    if self.len ~= other.len then
        return false
    end

    if self.len == 0 then
        return true
    end

    return unicode.lib.unicode_compare(self.data, self.len, other.data, other.len) == 1
end

function UString:__eq(other)
    if type(other) == "string" then
        other = UString.from_utf8(other)
    end
    return self:equals(other)
end

-- Search/INSTR (ZERO-COPY via C memcmp)
function UString:instr(needle, start_pos)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    start_pos = start_pos or 1

    if self.len == 0 or needle.len == 0 then
        return 0
    end

    return unicode.lib.unicode_instr(
        self.data, self.len,
        needle.data, needle.len,
        start_pos
    )
end

-- String repeat
function UString.repeat_char(codepoint, count)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if count <= 0 then
        return UString.new(nil, 0, false)
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local result_ptr = unicode.lib.unicode_repeat(codepoint, count, out_len_ptr)

    if result_ptr == nil then
        return UString.new(nil, 0, false)
    end

    return UString.new(result_ptr, out_len_ptr[0], true)
end

-- SPACE$
function UString.space(count)
    return UString.repeat_char(32, count)
end

-- Uppercase (uses existing C function)
function UString:upper()
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if self.len == 0 then
        return UString.new(nil, 0, false)
    end

    local result_ptr = unicode.lib.unicode_upper_new(self.data, self.len)

    if result_ptr == nil then
        error("Failed to convert to uppercase")
    end

    return UString.new(result_ptr, self.len, true)
end

-- Lowercase (uses existing C function)
function UString:lower()
    if not unicode.available then
        error("Unicode runtime not available")
    end

    if self.len == 0 then
        return UString.new(nil, 0, false)
    end

    local result_ptr = unicode.lib.unicode_lower_new(self.data, self.len)

    if result_ptr == nil then
        error("Failed to convert to lowercase")
    end

    return UString.new(result_ptr, self.len, true)
end

-- Get codepoint at position (1-based)
function UString:at(pos)
    if pos < 1 or pos > self.len then
        return nil
    end
    return self.data[pos - 1]
end

-- ASC (get first codepoint)
function UString:asc()
    if self.len == 0 then
        return 0
    end
    return self.data[0]
end

-- CHR$ (create single-character string)
function UString.chr(codepoint)
    return UString.repeat_char(codepoint, 1)
end

-- String representation for printing
function UString:__tostring()
    return self:to_utf8()
end

-- =============================================================================
-- Batch Operations (for multiple concatenations)
-- =============================================================================

-- Efficient concatenation of multiple strings
-- Avoids intermediate allocations
function UString.concat_many(strings)
    if not unicode.available then
        error("Unicode runtime not available")
    end

    -- Calculate total length
    local total_len = 0
    for _, str in ipairs(strings) do
        total_len = total_len + str.len
    end

    if total_len == 0 then
        return UString.new(nil, 0, false)
    end

    -- Allocate result once
    local result = ffi.cast("int32_t*", ffi.C.malloc(total_len * 4))
    if result == nil then
        error("Failed to allocate memory")
    end

    -- Copy all strings
    local offset = 0
    for _, str in ipairs(strings) do
        if str.len > 0 then
            ffi.C.memcpy(result + offset, str.data, str.len * 4)
            offset = offset + str.len
        end
    end

    return UString.new(result, total_len, true)
end

-- String builder for repeated concatenations
local StringBuilder = {}
StringBuilder.__index = StringBuilder

function StringBuilder.new()
    return setmetatable({
        parts = {},
        total_len = 0
    }, StringBuilder)
end

function StringBuilder:append(str)
    if str.len > 0 then
        table.insert(self.parts, str)
        self.total_len = self.total_len + str.len
    end
    return self
end

function StringBuilder:build()
    return UString.concat_many(self.parts)
end

-- =============================================================================
-- Module Exports
-- =============================================================================

return {
    UString = UString,
    StringBuilder = StringBuilder,

    -- Convenience constructors
    from_utf8 = UString.from_utf8,
    from_table = UString.from_table,
    chr = UString.chr,
    space = UString.space,
    repeat_char = UString.repeat_char,
    concat_many = UString.concat_many,
}
