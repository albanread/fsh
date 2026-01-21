--
-- unicode_pooled.lua
-- FasterBASIC - Lua wrapper for pooled C++ Unicode library
--
-- Provides automatic memory management and BASIC-compatible API
-- Uses reference counting and memory pooling for maximum performance
--

local ffi = require('ffi')

-- =============================================================================
-- FFI Declarations
-- =============================================================================

ffi.cdef [[
    // Opaque types
    typedef struct UnicodeString* UString;
    typedef struct UnicodeStringPool* UStringPool;

    // Pool management
    UStringPool ustring_pool_create(size_t initial_capacity);
    void ustring_pool_destroy(UStringPool pool);
    void ustring_pool_stats(UStringPool pool,
                           size_t* out_allocated,
                           size_t* out_pooled,
                           size_t* out_total_memory);

    // String creation
    UString ustring_from_utf8(const char* utf8_str, UStringPool pool);
    UString ustring_from_codepoints(const int32_t* codepoints, size_t length, UStringPool pool);
    UString ustring_empty(UStringPool pool);
    UString ustring_repeat(int32_t codepoint, size_t count, UStringPool pool);

    // Reference counting
    UString ustring_retain(UString str);
    void ustring_release(UString str);
    int32_t ustring_refcount(UString str);

    // Access
    size_t ustring_length(UString str);
    const int32_t* ustring_codepoints(UString str);
    char* ustring_to_utf8(UString str, size_t* out_length);
    int32_t ustring_at(UString str, size_t index);
    int ustring_is_empty(UString str);

    // Operations (return new strings with refcount=1)
    UString ustring_concat(UString str1, UString str2, UStringPool pool);
    UString ustring_substring(UString str, size_t start, size_t length, UStringPool pool);
    UString ustring_left(UString str, size_t count, UStringPool pool);
    UString ustring_right(UString str, size_t count, UStringPool pool);
    UString ustring_upper(UString str, UStringPool pool);
    UString ustring_lower(UString str, UStringPool pool);
    UString ustring_reverse(UString str, UStringPool pool);
    UString ustring_trim(UString str, UStringPool pool);

    // Comparison
    int ustring_equals(UString str1, UString str2);
    int ustring_compare(UString str1, UString str2);

    // Search
    size_t ustring_find(UString haystack, UString needle, size_t start_pos);

    // Batch operations
    UString ustring_concat_many(UString* strings, size_t count, UStringPool pool);

    // String builder
    typedef struct UnicodeStringBuilder* UStringBuilder;
    UStringBuilder ustring_builder_create(size_t initial_capacity, UStringPool pool);
    int ustring_builder_append(UStringBuilder builder, UString str);
    int ustring_builder_append_char(UStringBuilder builder, int32_t codepoint);
    UString ustring_builder_build(UStringBuilder builder);
    void ustring_builder_destroy(UStringBuilder builder);

    // Utility
    const char* ustring_version();
    void ustring_global_stats(size_t* out_total_strings, size_t* out_total_memory);

    // Memory functions
    void free(void* ptr);
]]

-- =============================================================================
-- Load Library
-- =============================================================================

local lib = nil
local available = false

local function try_load_library()
    local lib_names = {
        -- macOS
        'libunicode_string.dylib',
        './libunicode_string.dylib',
        'runtime/libunicode_string.dylib',
        '../runtime/libunicode_string.dylib',
        -- Linux
        'libunicode_string.so',
        './libunicode_string.so',
        'runtime/libunicode_string.so',
        -- Windows
        'unicode_string.dll',
        './unicode_string.dll',
        'runtime/unicode_string.dll',
    }

    for _, lib_name in ipairs(lib_names) do
        local ok, loaded = pcall(ffi.load, lib_name)
        if ok then
            return loaded
        end
    end

    return nil
end

lib = try_load_library()

if lib then
    available = true
else
    print("Warning: Pooled Unicode library not available")
    print("  Tried loading libunicode_string from multiple locations")
    print("  Unicode performance will be degraded")
    available = false
end

-- =============================================================================
-- Global Pool Management
-- =============================================================================

local g_pool = nil

local function ensure_pool()
    if not g_pool and available then
        -- Create default pool with capacity for 1000 strings
        g_pool = lib.ustring_pool_create(1000)
    end
    return g_pool
end

-- =============================================================================
-- UString Wrapper Class (with automatic memory management)
-- =============================================================================

local UString = {}
UString.__index = UString

-- Wrap a C UString handle
function UString.wrap(handle)
    if not handle or handle == nil then
        return nil
    end

    local obj = {
        handle = handle,
        owned = true -- We own this reference
    }

    -- Automatic garbage collection
    local mt = {
        __index = UString,
        __gc = function(self)
            if self.owned and self.handle then
                lib.ustring_release(self.handle)
                self.handle = nil
            end
        end
    }

    return setmetatable(obj, mt)
end

-- Create from UTF-8 string
function UString.from_utf8(utf8_str)
    if not available then
        error("Pooled Unicode library not available")
    end

    local pool = ensure_pool()
    local handle = lib.ustring_from_utf8(utf8_str or "", pool)
    return UString.wrap(handle)
end

-- Create from codepoint table
function UString.from_table(tbl)
    if not available then
        error("Pooled Unicode library not available")
    end

    if not tbl or #tbl == 0 then
        return UString.from_utf8("")
    end

    local pool = ensure_pool()
    local arr = ffi.new("int32_t[?]", #tbl)
    for i = 1, #tbl do
        arr[i - 1] = tbl[i]
    end

    local handle = lib.ustring_from_codepoints(arr, #tbl, pool)
    return UString.wrap(handle)
end

-- Create empty string
function UString.empty()
    if not available then
        error("Pooled Unicode library not available")
    end

    local pool = ensure_pool()
    local handle = lib.ustring_empty(pool)
    return UString.wrap(handle)
end

-- Create by repeating a character
function UString.repeat_char(codepoint, count)
    if not available then
        error("Pooled Unicode library not available")
    end

    local pool = ensure_pool()
    local handle = lib.ustring_repeat(codepoint, count, pool)
    return UString.wrap(handle)
end

-- Get length
function UString:length()
    if not self.handle then return 0 end
    return tonumber(lib.ustring_length(self.handle))
end

function UString:__len()
    return self:length()
end

-- Convert to UTF-8 string
function UString:to_utf8()
    if not self.handle then return "" end

    local utf8_ptr = lib.ustring_to_utf8(self.handle, nil)
    if not utf8_ptr then return "" end

    local result = ffi.string(utf8_ptr)
    ffi.C.free(utf8_ptr)
    return result
end

-- Convert to table of codepoints
function UString:to_table()
    if not self.handle then return {} end

    local len = lib.ustring_length(self.handle)
    if len == 0 then return {} end

    local cp_ptr = lib.ustring_codepoints(self.handle)
    if not cp_ptr then return {} end

    local result = {}
    for i = 0, len - 1 do
        result[i + 1] = cp_ptr[i]
    end
    return result
end

-- Get codepoint at index (0-based)
function UString:at(index)
    if not self.handle then return -1 end
    return lib.ustring_at(self.handle, index)
end

-- Check if empty
function UString:is_empty()
    if not self.handle then return true end
    return lib.ustring_is_empty(self.handle) == 1
end

-- Concatenate
function UString:concat(other)
    if not self.handle or not other or not other.handle then
        return UString.empty()
    end

    local pool = ensure_pool()
    local handle = lib.ustring_concat(self.handle, other.handle, pool)
    return UString.wrap(handle)
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

-- Substring (1-based BASIC indexing)
function UString:substring(start, length)
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_substring(self.handle, start, length, pool)
    return UString.wrap(handle)
end

function UString:mid(start, length)
    return self:substring(start, length)
end

-- LEFT$
function UString:left(count)
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_left(self.handle, count, pool)
    return UString.wrap(handle)
end

-- RIGHT$
function UString:right(count)
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_right(self.handle, count, pool)
    return UString.wrap(handle)
end

-- UPPER$
function UString:upper()
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_upper(self.handle, pool)
    return UString.wrap(handle)
end

-- LOWER$
function UString:lower()
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_lower(self.handle, pool)
    return UString.wrap(handle)
end

-- REVERSE$
function UString:reverse()
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_reverse(self.handle, pool)
    return UString.wrap(handle)
end

-- TRIM$
function UString:trim()
    if not self.handle then return UString.empty() end

    local pool = ensure_pool()
    local handle = lib.ustring_trim(self.handle, pool)
    return UString.wrap(handle)
end

-- Equals
function UString:equals(other)
    if not self.handle or not other or not other.handle then
        return false
    end
    return lib.ustring_equals(self.handle, other.handle) == 1
end

function UString:__eq(other)
    if type(other) == "string" then
        other = UString.from_utf8(other)
    end
    return self:equals(other)
end

-- Compare
function UString:compare(other)
    if not self.handle or not other or not other.handle then
        return 0
    end
    return lib.ustring_compare(self.handle, other.handle)
end

-- Find (INSTR)
function UString:find(needle, start_pos)
    if not self.handle or not needle or not needle.handle then
        return 0
    end
    start_pos = start_pos or 1
    return tonumber(lib.ustring_find(self.handle, needle.handle, start_pos))
end

function UString:instr(needle, start_pos)
    return self:find(needle, start_pos)
end

-- String representation
function UString:__tostring()
    return self:to_utf8()
end

-- Get refcount (for debugging)
function UString:refcount()
    if not self.handle then return 0 end
    return lib.ustring_refcount(self.handle)
end

-- =============================================================================
-- String Builder
-- =============================================================================

local StringBuilder = {}
StringBuilder.__index = StringBuilder

function StringBuilder.new(initial_capacity)
    if not available then
        error("Pooled Unicode library not available")
    end

    initial_capacity = initial_capacity or 256
    local pool = ensure_pool()
    local handle = lib.ustring_builder_create(initial_capacity, pool)

    local obj = {
        handle = handle,
    }

    return setmetatable(obj, StringBuilder)
end

function StringBuilder:append(str)
    if not self.handle then return self end

    if type(str) == "string" then
        str = UString.from_utf8(str)
    end

    if str and str.handle then
        lib.ustring_builder_append(self.handle, str.handle)
    end

    return self
end

function StringBuilder:append_char(codepoint)
    if not self.handle then return self end
    lib.ustring_builder_append_char(self.handle, codepoint)
    return self
end

function StringBuilder:build()
    if not self.handle then return UString.empty() end

    local handle = lib.ustring_builder_build(self.handle)
    self.handle = nil -- Builder is consumed
    return UString.wrap(handle)
end

-- =============================================================================
-- BASIC-Compatible Global Functions
-- =============================================================================

-- These match the BASIC function signatures

function unicode_from_utf8(utf8_str)
    return UString.from_utf8(utf8_str)
end

function unicode_to_utf8(ustr)
    if type(ustr) == "string" then
        return ustr
    end
    if not ustr or not ustr.to_utf8 then
        return ""
    end
    return ustr:to_utf8()
end

function unicode_len(ustr)
    if not ustr then return 0 end
    return ustr:length()
end

function unicode_concat(a, b)
    local ustr_a = type(a) == "string" and UString.from_utf8(a) or a
    local ustr_b = type(b) == "string" and UString.from_utf8(b) or b
    return ustr_a:concat(ustr_b)
end

function unicode_left(ustr, n)
    return ustr:left(n)
end

function unicode_right(ustr, n)
    return ustr:right(n)
end

function unicode_mid(ustr, start, length)
    return ustr:mid(start, length)
end

function unicode_upper(ustr)
    return ustr:upper()
end

function unicode_lower(ustr)
    return ustr:lower()
end

function unicode_reverse(ustr)
    return ustr:reverse()
end

function unicode_trim(ustr)
    return ustr:trim()
end

function unicode_ltrim(ustr)
    -- Simple implementation using trim logic
    if not ustr or ustr:is_empty() then
        return UString.empty()
    end

    local tbl = ustr:to_table()
    local start = 1
    while start <= #tbl and lib.unicode_is_space and lib.unicode_is_space(tbl[start]) == 1 do
        start = start + 1
    end

    if start > #tbl then
        return UString.empty()
    end

    return UString.from_table({ table.unpack(tbl, start) })
end

function unicode_rtrim(ustr)
    -- Simple implementation using trim logic
    if not ustr or ustr:is_empty() then
        return UString.empty()
    end

    local tbl = ustr:to_table()
    local finish = #tbl
    while finish >= 1 and lib.unicode_is_space and lib.unicode_is_space(tbl[finish]) == 1 do
        finish = finish - 1
    end

    if finish < 1 then
        return UString.empty()
    end

    return UString.from_table({ table.unpack(tbl, 1, finish) })
end

function unicode_string_equal(a, b)
    local ustr_a = type(a) == "string" and UString.from_utf8(a) or a
    local ustr_b = type(b) == "string" and UString.from_utf8(b) or b
    return ustr_a:equals(ustr_b)
end

function unicode_string_compare(a, b)
    local ustr_a = type(a) == "string" and UString.from_utf8(a) or a
    local ustr_b = type(b) == "string" and UString.from_utf8(b) or b
    return ustr_a:compare(ustr_b)
end

function unicode_instr(haystack, needle, start)
    return haystack:instr(needle, start)
end

function unicode_chr(codepoint)
    return UString.repeat_char(codepoint, 1)
end

function unicode_asc(ustr)
    if not ustr or ustr:is_empty() then
        return 0
    end
    return ustr:at(0)
end

function unicode_space(n)
    return UString.repeat_char(32, n)
end

function unicode_string(count, cp)
    if type(cp) == "table" then
        -- It's a UString - get first character
        if cp:is_empty() then
            return UString.empty()
        end
        cp = cp:at(0)
    end
    return UString.repeat_char(cp, count)
end

-- Check if value is a UString
function unicode_is_ustring(val)
    return type(val) == "table" and val.handle ~= nil
end

-- =============================================================================
-- Batch Operations
-- =============================================================================

function unicode_concat_many(...)
    local strings = { ... }
    if #strings == 0 then
        return UString.empty()
    end

    if #strings == 1 then
        return strings[1]
    end

    -- Build result by concatenating
    local result = strings[1]
    for i = 2, #strings do
        result = result:concat(strings[i])
    end
    return result
end

function unicode_string_builder()
    return StringBuilder.new()
end

-- =============================================================================
-- Statistics and Utilities
-- =============================================================================

function unicode_pool_stats()
    if not available or not g_pool then
        return { allocated = 0, pooled = 0, memory = 0 }
    end

    local allocated = ffi.new("size_t[1]")
    local pooled = ffi.new("size_t[1]")
    local memory = ffi.new("size_t[1]")

    lib.ustring_pool_stats(g_pool, allocated, pooled, memory)

    return {
        allocated = tonumber(allocated[0]),
        pooled = tonumber(pooled[0]),
        memory = tonumber(memory[0])
    }
end

function unicode_version()
    if not available then
        return "unavailable"
    end
    return ffi.string(lib.ustring_version())
end

function unicode_info()
    print("Pooled Unicode Library")
    print("  Available: " .. tostring(available))
    if available then
        print("  Version: " .. unicode_version())
        local stats = unicode_pool_stats()
        print("  Pool Statistics:")
        print("    Allocated: " .. stats.allocated .. " strings")
        print("    Pooled (reusable): " .. stats.pooled .. " strings")
        print("    Total memory: " .. stats.memory .. " bytes")
    end
end

-- =============================================================================
-- Module Exports
-- =============================================================================

return {
    available = available,
    lib = lib,
    UString = UString,
    StringBuilder = StringBuilder,

    -- Constructors
    from_utf8 = UString.from_utf8,
    from_table = UString.from_table,
    empty = UString.empty,
    repeat_char = UString.repeat_char,

    -- Builder
    builder = StringBuilder.new,

    -- Stats
    pool_stats = unicode_pool_stats,
    version = unicode_version,
    info = unicode_info,
}
