-- unicode_handles.lua
-- Handle-based Unicode string library using integer handles instead of GC objects
-- This avoids the overhead of Lua object creation, metatables, and garbage collection

local ffi = require('ffi')

-- =============================================================================
-- FFI Definitions
-- =============================================================================

ffi.cdef [[
    typedef struct UnicodeString* UString;
    typedef struct UnicodeStringPool* UStringPool;

    // Pool management
    UStringPool ustring_pool_create(size_t initial_capacity);
    void ustring_pool_destroy(UStringPool pool);
    void ustring_pool_stats(UStringPool pool,
                           size_t* out_allocated,
                           size_t* out_pooled,
                           size_t* out_total_memory);

    // String creation (with background reclamation)
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

    // Utility
    const char* ustring_version();

    // Memory functions
    void free(void* ptr);
]]

-- =============================================================================
-- Library Loading
-- =============================================================================

local function try_load_library()
    local paths = {
        "./runtime/libunicode_string.dylib",
        "./libunicode_string.dylib",
        "runtime/libunicode_string.dylib",
        "libunicode_string.dylib",
        "./runtime/libunicode_string.so",
        "./libunicode_string.so",
        "libunicode_string.so",
    }

    for _, path in ipairs(paths) do
        local ok, lib = pcall(ffi.load, path)
        if ok then
            return lib
        end
    end

    return nil
end

local lib = try_load_library()
local available = (lib ~= nil)

if not available then
    return {
        available = false,
        error = "Could not load libunicode_string library"
    }
end

-- =============================================================================
-- Handle Management
-- =============================================================================

-- Global pool
local g_pool = lib.ustring_pool_create(1000)

-- Handle table: maps integer handle → C UString pointer
local handles = {}
local next_handle = 1

-- Create a new handle for a C UString
local function new_handle(ustring)
    if not ustring or ustring == nil then
        return 0 -- NULL handle
    end

    local handle = next_handle
    next_handle = next_handle + 1
    handles[handle] = ustring
    return handle
end

-- Get C UString from handle
local function get_ustring(handle)
    if handle == 0 then
        return nil
    end
    return handles[handle]
end

-- Release a handle and its associated C string
local function release_handle(handle)
    if handle == 0 then
        return
    end

    local ustring = handles[handle]
    if ustring then
        lib.ustring_release(ustring)
        handles[handle] = nil
    end
end

-- =============================================================================
-- Public API - All functions return integer handles
-- =============================================================================

-- Create string from UTF-8
local function from_utf8(utf8_str)
    local ustring = lib.ustring_from_utf8(utf8_str or "", g_pool)
    return new_handle(ustring)
end

-- Create string from codepoint table
local function from_table(codepoint_table)
    if not codepoint_table or #codepoint_table == 0 then
        local ustring = lib.ustring_empty(g_pool)
        return new_handle(ustring)
    end

    local array = ffi.new("int32_t[?]", #codepoint_table)
    for i = 1, #codepoint_table do
        array[i - 1] = codepoint_table[i]
    end

    local ustring = lib.ustring_from_codepoints(array, #codepoint_table, g_pool)
    return new_handle(ustring)
end

-- Create empty string
local function empty()
    local ustring = lib.ustring_empty(g_pool)
    return new_handle(ustring)
end

-- Repeat a character
local function repeat_char(codepoint, count)
    local ustring = lib.ustring_repeat(codepoint, count, g_pool)
    return new_handle(ustring)
end

-- Convert handle to UTF-8 string
local function to_utf8(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return ""
    end

    local len_out = ffi.new("size_t[1]")
    local utf8_ptr = lib.ustring_to_utf8(ustring, len_out)

    if utf8_ptr == nil then
        return ""
    end

    local result = ffi.string(utf8_ptr, len_out[0])
    ffi.C.free(utf8_ptr)
    return result
end

-- Convert handle to codepoint table
local function to_table(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return {}
    end

    local len = lib.ustring_length(ustring)
    if len == 0 then
        return {}
    end

    local codepoints_ptr = lib.ustring_codepoints(ustring)
    local result = {}
    for i = 0, len - 1 do
        result[i + 1] = codepoints_ptr[i]
    end
    return result
end

-- Get length
local function length(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end
    return tonumber(lib.ustring_length(ustring))
end

-- Get character at position (1-based)
local function at(handle, pos)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end
    return lib.ustring_at(ustring, pos - 1) -- Convert to 0-based
end

-- Check if empty
local function is_empty(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return true
    end
    return lib.ustring_is_empty(ustring) ~= 0
end

-- Concatenate two strings
local function concat(handle1, handle2)
    local ustring1 = get_ustring(handle1)
    local ustring2 = get_ustring(handle2)

    if not ustring1 or not ustring2 then
        return 0
    end

    local result = lib.ustring_concat(ustring1, ustring2, g_pool)
    return new_handle(result)
end

-- Substring (1-based start, length)
local function substring(handle, start, len)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_substring(ustring, start - 1, len, g_pool) -- Convert to 0-based
    return new_handle(result)
end

-- Left substring
local function left(handle, count)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_left(ustring, count, g_pool)
    return new_handle(result)
end

-- Right substring
local function right(handle, count)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_right(ustring, count, g_pool)
    return new_handle(result)
end

-- Uppercase
local function upper(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_upper(ustring, g_pool)
    return new_handle(result)
end

-- Lowercase
local function lower(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_lower(ustring, g_pool)
    return new_handle(result)
end

-- Reverse
local function reverse(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_reverse(ustring, g_pool)
    return new_handle(result)
end

-- Trim
local function trim(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_trim(ustring, g_pool)
    return new_handle(result)
end

-- Equality comparison
local function equals(handle1, handle2)
    local ustring1 = get_ustring(handle1)
    local ustring2 = get_ustring(handle2)

    if not ustring1 or not ustring2 then
        return false
    end

    return lib.ustring_equals(ustring1, ustring2) ~= 0
end

-- Compare (returns -1, 0, or 1)
local function compare(handle1, handle2)
    local ustring1 = get_ustring(handle1)
    local ustring2 = get_ustring(handle2)

    if not ustring1 or not ustring2 then
        return 0
    end

    return lib.ustring_compare(ustring1, ustring2)
end

-- Find substring (returns 1-based position, or 0 if not found)
local function find(haystack_handle, needle_handle, start_pos)
    start_pos = start_pos or 1

    local haystack = get_ustring(haystack_handle)
    local needle = get_ustring(needle_handle)

    if not haystack or not needle then
        return 0
    end

    local pos = lib.ustring_find(haystack, needle, start_pos - 1) -- Convert to 0-based
    if pos == 0 or pos == ffi.cast("size_t", -1) then
        return 0
    end
    return tonumber(pos) + 1 -- Convert back to 1-based
end

-- Concatenate multiple handles
local function concat_many(handle_array)
    if not handle_array or #handle_array == 0 then
        return empty()
    end

    local ustrings = ffi.new("UString[?]", #handle_array)
    for i = 1, #handle_array do
        ustrings[i - 1] = get_ustring(handle_array[i])
    end

    local result = lib.ustring_concat_many(ustrings, #handle_array, g_pool)
    return new_handle(result)
end

-- Retain (increment reference count) - returns same handle
local function retain(handle)
    local ustring = get_ustring(handle)
    if ustring then
        lib.ustring_retain(ustring)
    end
    return handle
end

-- Get reference count
local function refcount(handle)
    local ustring = get_ustring(handle)
    if not ustring then
        return 0
    end
    return lib.ustring_refcount(ustring)
end

-- =============================================================================
-- BASIC-Compatible API (uses handles)
-- =============================================================================

-- These match the BASIC function signatures but use handles

local function unicode_from_utf8(utf8_str)
    return from_utf8(utf8_str)
end

local function unicode_to_utf8(handle)
    return to_utf8(handle)
end

local function unicode_len(handle)
    return length(handle)
end

local function unicode_concat(h1, h2)
    return concat(h1, h2)
end

local function unicode_left(handle, n)
    return left(handle, n)
end

local function unicode_right(handle, n)
    return right(handle, n)
end

local function unicode_mid(handle, start, len)
    return substring(handle, start, len)
end

local function unicode_upper(handle)
    return upper(handle)
end

local function unicode_lower(handle)
    return lower(handle)
end

local function unicode_reverse(handle)
    return reverse(handle)
end

local function unicode_trim(handle)
    return trim(handle)
end

local function unicode_string_equal(h1, h2)
    return equals(h1, h2)
end

local function unicode_string_compare(h1, h2)
    return compare(h1, h2)
end

local function unicode_instr(haystack, needle, start)
    return find(haystack, needle, start)
end

local function unicode_chr(codepoint)
    return repeat_char(codepoint, 1)
end

local function unicode_asc(handle, pos)
    pos = pos or 1
    return at(handle, pos)
end

local function unicode_space(count)
    return repeat_char(32, count) -- ASCII space
end

local function unicode_string(handle, count)
    if type(handle) == "number" and count then
        -- BASIC STRING$(count, char)
        return repeat_char(handle, count)
    elseif type(handle) == "number" then
        -- BASIC STRING$(char)
        return repeat_char(handle, 1)
    end
    return 0
end

-- Release a handle (IMPORTANT: must be called when done with string)
local function unicode_release(handle)
    release_handle(handle)
end

-- Release multiple handles
local function unicode_release_many(handle_array)
    for i = 1, #handle_array do
        release_handle(handle_array[i])
    end
end

-- =============================================================================
-- Utility Functions
-- =============================================================================

local function pool_stats()
    local allocated = ffi.new("size_t[1]")
    local pooled = ffi.new("size_t[1]")
    local total_mem = ffi.new("size_t[1]")

    lib.ustring_pool_stats(g_pool, allocated, pooled, total_mem)

    print(string.format("  Allocated: %d strings", tonumber(allocated[0])))
    print(string.format("  Pooled (reusable): %d strings", tonumber(pooled[0])))
    print(string.format("  Total memory: %d bytes", tonumber(total_mem[0])))
end

local function version()
    local ver = lib.ustring_version()
    if ver ~= nil then
        return ffi.string(ver)
    end
    return "unknown"
end

local function info()
    print("Handle-based Unicode Library (Zero-GC)")
    print("  Available: " .. tostring(available))
    print("  Version: " .. version())
    print("  Active handles: " .. next_handle - 1)
    print("  Pool Statistics:")
    pool_stats()
end

-- Cleanup on exit
local function cleanup()
    -- Release all remaining handles
    for handle, ustring in pairs(handles) do
        lib.ustring_release(ustring)
    end
    handles = {}

    -- Destroy pool
    if g_pool then
        lib.ustring_pool_destroy(g_pool)
        g_pool = nil
    end
end

-- =============================================================================
-- Module Exports
-- =============================================================================

return {
    available = available,

    -- Handle management
    from_utf8 = from_utf8,
    from_table = from_table,
    empty = empty,
    repeat_char = repeat_char,
    release = unicode_release,
    release_many = unicode_release_many,
    retain = retain,
    refcount = refcount,

    -- String operations (all return handles)
    to_utf8 = to_utf8,
    to_table = to_table,
    length = length,
    at = at,
    is_empty = is_empty,
    concat = concat,
    substring = substring,
    left = left,
    right = right,
    upper = upper,
    lower = lower,
    reverse = reverse,
    trim = trim,
    equals = equals,
    compare = compare,
    find = find,
    concat_many = concat_many,

    -- BASIC-compatible API
    unicode_from_utf8 = unicode_from_utf8,
    unicode_to_utf8 = unicode_to_utf8,
    unicode_len = unicode_len,
    unicode_concat = unicode_concat,
    unicode_left = unicode_left,
    unicode_right = unicode_right,
    unicode_mid = unicode_mid,
    unicode_upper = unicode_upper,
    unicode_lower = unicode_lower,
    unicode_reverse = unicode_reverse,
    unicode_trim = unicode_trim,
    unicode_string_equal = unicode_string_equal,
    unicode_string_compare = unicode_string_compare,
    unicode_instr = unicode_instr,
    unicode_chr = unicode_chr,
    unicode_asc = unicode_asc,
    unicode_space = unicode_space,
    unicode_string = unicode_string,
    unicode_release = unicode_release,

    -- Utilities
    pool_stats = pool_stats,
    version = version,
    info = info,
    cleanup = cleanup,
}
