-- unicode_unified.lua
-- Unified Unicode string library using memory addresses as handles
-- Zero overhead: pointer addresses are used directly as integer handles
-- No table lookups, no GC objects, just raw pointer manipulation

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
    const char* ustring_to_utf8_lua(UString str);
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

    // For pointer arithmetic
    typedef long intptr_t;
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
-- Pointer ↔ Handle Conversion (Zero Overhead)
-- =============================================================================

-- Convert C pointer to integer handle (just the memory address)
local function ptr_to_handle(ptr)
    if ptr == nil or ptr == ffi.null then
        return 0
    end
    return tonumber(ffi.cast("intptr_t", ptr))
end

-- Convert integer handle back to C pointer
local function handle_to_ptr(handle)
    if handle == 0 then
        return nil
    end
    return ffi.cast("UString", ffi.cast("intptr_t", handle))
end

-- =============================================================================
-- Global Pool Management
-- =============================================================================

local g_pool = lib.ustring_pool_create(1000)
local g_pool_handle = ptr_to_handle(ffi.cast("UStringPool", g_pool))

-- =============================================================================
-- Public API - All functions use integer handles (memory addresses)
-- =============================================================================

-- Create string from UTF-8
local function from_utf8(utf8_str)
    local ustring = lib.ustring_from_utf8(utf8_str or "", g_pool)
    return ptr_to_handle(ustring)
end

-- Create string from codepoint table
local function from_table(codepoint_table)
    if not codepoint_table or #codepoint_table == 0 then
        local ustring = lib.ustring_empty(g_pool)
        return ptr_to_handle(ustring)
    end

    local array = ffi.new("int32_t[?]", #codepoint_table)
    for i = 1, #codepoint_table do
        array[i - 1] = codepoint_table[i]
    end

    local ustring = lib.ustring_from_codepoints(array, #codepoint_table, g_pool)
    return ptr_to_handle(ustring)
end

-- Create empty string
local function empty()
    local ustring = lib.ustring_empty(g_pool)
    return ptr_to_handle(ustring)
end

-- Repeat a character
local function repeat_char(codepoint, count)
    local ustring = lib.ustring_repeat(codepoint, count, g_pool)
    return ptr_to_handle(ustring)
end

-- Convert handle to UTF-8 string (optimized: single FFI call)
local function to_utf8(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return ""
    end

    -- Use optimized version that returns static buffer (no manual free needed)
    local cstr = lib.ustring_to_utf8_lua(ustring)
    if not cstr then
        return ""
    end

    return ffi.string(cstr)
end

-- Convert handle to codepoint table
local function to_table(handle)
    local ustring = handle_to_ptr(handle)
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
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end
    return tonumber(lib.ustring_length(ustring))
end

-- Get character at position (1-based)
local function at(handle, pos)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end
    return lib.ustring_at(ustring, pos - 1) -- Convert to 0-based
end

-- Check if empty
local function is_empty(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return true
    end
    return lib.ustring_is_empty(ustring) ~= 0
end

-- Concatenate two strings
local function concat(handle1, handle2)
    local ustring1 = handle_to_ptr(handle1)
    local ustring2 = handle_to_ptr(handle2)

    if not ustring1 or not ustring2 then
        return 0
    end

    local result = lib.ustring_concat(ustring1, ustring2, g_pool)
    return ptr_to_handle(result)
end

-- Substring (1-based start, length)
local function substring(handle, start, len)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_substring(ustring, start, len, g_pool) -- C function expects 1-based
    return ptr_to_handle(result)
end

-- Left substring
local function left(handle, count)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_left(ustring, count, g_pool)
    return ptr_to_handle(result)
end

-- Right substring
local function right(handle, count)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_right(ustring, count, g_pool)
    return ptr_to_handle(result)
end

-- Uppercase
local function upper(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_upper(ustring, g_pool)
    return ptr_to_handle(result)
end

-- Lowercase
local function lower(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_lower(ustring, g_pool)
    return ptr_to_handle(result)
end

-- Reverse
local function reverse(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_reverse(ustring, g_pool)
    return ptr_to_handle(result)
end

-- Trim
local function trim(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end

    local result = lib.ustring_trim(ustring, g_pool)
    return ptr_to_handle(result)
end

-- Equality comparison
local function equals(handle1, handle2)
    local ustring1 = handle_to_ptr(handle1)
    local ustring2 = handle_to_ptr(handle2)

    if not ustring1 or not ustring2 then
        return false
    end

    return lib.ustring_equals(ustring1, ustring2) ~= 0
end

-- Compare (returns -1, 0, or 1)
local function compare(handle1, handle2)
    local ustring1 = handle_to_ptr(handle1)
    local ustring2 = handle_to_ptr(handle2)

    if not ustring1 or not ustring2 then
        return 0
    end

    return lib.ustring_compare(ustring1, ustring2)
end

-- Find substring (returns 1-based position, or 0 if not found)
local function find(haystack_handle, needle_handle, start_pos)
    start_pos = start_pos or 1

    local haystack = handle_to_ptr(haystack_handle)
    local needle = handle_to_ptr(needle_handle)

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
        ustrings[i - 1] = handle_to_ptr(handle_array[i])
    end

    local result = lib.ustring_concat_many(ustrings, #handle_array, g_pool)
    return ptr_to_handle(result)
end

-- Release a handle (IMPORTANT: must be called when done with string)
local function release(handle)
    local ustring = handle_to_ptr(handle)
    if ustring then
        lib.ustring_release(ustring)
    end
end

-- Release multiple handles
local function release_many(handle_array)
    for i = 1, #handle_array do
        release(handle_array[i])
    end
end

-- Retain (increment reference count) - returns same handle
local function retain(handle)
    local ustring = handle_to_ptr(handle)
    if ustring then
        lib.ustring_retain(ustring)
    end
    return handle
end

-- Get reference count
local function refcount(handle)
    local ustring = handle_to_ptr(handle)
    if not ustring then
        return 0
    end
    return lib.ustring_refcount(ustring)
end

-- =============================================================================
-- BASIC-Compatible API (uses handles)
-- =============================================================================

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

local function unicode_release(handle)
    release(handle)
end

local function unicode_release_many(handle_array)
    release_many(handle_array)
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
    print("Unified Unicode Library (Memory Addresses as Handles)")
    print("  Available: " .. tostring(available))
    print("  Version: " .. version())
    print("  Pool handle: 0x" .. string.format("%x", g_pool_handle))
    print("  Pool Statistics:")
    pool_stats()
    print()
    print("  Zero overhead design:")
    print("    • Handles ARE memory addresses (no table lookup)")
    print("    • Direct pointer casting (no wrapper objects)")
    print("    • Background thread memory reclamation")
    print("    • Zero Lua GC overhead")
end

-- Cleanup on exit
local function cleanup()
    -- User must release all handles manually before cleanup
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

    -- Core API (handles are memory addresses)
    from_utf8 = from_utf8,
    from_table = from_table,
    empty = empty,
    repeat_char = repeat_char,
    release = release,
    release_many = release_many,
    retain = retain,
    refcount = refcount,

    -- String operations (all work with handles)
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

    -- Low-level (for advanced use)
    ptr_to_handle = ptr_to_handle,
    handle_to_ptr = handle_to_ptr,
}
