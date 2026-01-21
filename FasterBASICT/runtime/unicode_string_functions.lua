--
-- unicode_string_functions.lua
-- FasterBASIC Unicode String Functions Library
--
-- Provides Unicode-aware string manipulation functions for OPTION UNICODE mode.
-- All functions work on codepoint arrays (tables of UTF-32 integers) instead of Lua strings.
--

local unicode_string_lib = {}

-- Ensure unicode module is available
if not unicode then
    error("unicode_string_functions requires unicode module to be loaded first")
end

-- =============================================================================
-- Helper Functions
-- =============================================================================

local function ensure_codepoints(val)
    if type(val) == "table" then
        return val
    elseif type(val) == "string" then
        -- Convert UTF-8 string to codepoints
        return unicode.from_utf8(val)
    elseif type(val) == "number" then
        -- Single codepoint
        return { val }
    else
        return {}
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

    -- Convert to codepoint arrays if needed
    a = ensure_codepoints(a)
    b = ensure_codepoints(b)

    -- Check lengths first (fast path)
    if #a ~= #b then
        return false
    end

    -- Compare each codepoint
    for i = 1, #a do
        if a[i] ~= b[i] then
            return false
        end
    end

    return true
end

-- Compare two Unicode strings lexicographically
-- Returns: -1 if a < b, 0 if a == b, 1 if a > b
function unicode_string_compare(a, b)
    a = ensure_codepoints(a)
    b = ensure_codepoints(b)

    local len = math.min(#a, #b)
    for i = 1, len do
        if a[i] < b[i] then
            return -1
        elseif a[i] > b[i] then
            return 1
        end
    end

    -- All compared codepoints are equal, check lengths
    if #a < #b then
        return -1
    elseif #a > #b then
        return 1
    else
        return 0
    end
end

-- =============================================================================
-- Number to String Conversion Functions
-- =============================================================================

-- BIN$ - Convert number to binary string (returns codepoint array)
function string_bin(num, digits)
    num = ensure_int(num)
    digits = digits and ensure_int(digits) or 0

    -- Handle negative numbers (convert to unsigned 32-bit)
    if num < 0 then
        num = num + 0x100000000
    end

    local bin = ""
    local n = num

    if n == 0 then
        bin = "0"
    else
        while n > 0 do
            bin = tostring(n % 2) .. bin
            n = math.floor(n / 2)
        end
    end

    -- Pad with zeros if needed
    if digits > 0 and #bin < digits then
        bin = string.rep("0", digits - #bin) .. bin
    end

    -- Convert ASCII string to codepoint array
    return unicode.from_utf8(bin)
end

-- HEX$ - Convert number to hexadecimal string (returns codepoint array)
function string_hex(num, digits)
    num = ensure_int(num)
    digits = digits and ensure_int(digits) or 0

    -- Handle negative numbers (convert to unsigned 32-bit)
    if num < 0 then
        num = num + 0x100000000
    end

    local hex = string.format("%X", num)

    -- Pad with zeros if needed
    if digits > 0 and #hex < digits then
        hex = string.rep("0", digits - #hex) .. hex
    end

    return unicode.from_utf8(hex)
end

-- OCT$ - Convert number to octal string (returns codepoint array)
function string_oct(num, digits)
    num = ensure_int(num)
    digits = digits and ensure_int(digits) or 0

    -- Handle negative numbers (convert to unsigned 32-bit)
    if num < 0 then
        num = num + 0x100000000
    end

    local oct = string.format("%o", num)

    -- Pad with zeros if needed
    if digits > 0 and #oct < digits then
        oct = string.rep("0", digits - #oct) .. oct
    end

    return unicode.from_utf8(oct)
end

-- =============================================================================
-- String Manipulation Functions (on codepoint arrays)
-- =============================================================================

-- REVERSE$ - Reverse codepoint array
function string_reverse(codepoints)
    codepoints = ensure_codepoints(codepoints)
    return unicode.reverse(codepoints)
end

-- REPLACE$ - Replace all occurrences of old_pattern with new_pattern
function string_replace(codepoints, old_pattern, new_pattern)
    codepoints = ensure_codepoints(codepoints)
    old_pattern = ensure_codepoints(old_pattern)
    new_pattern = ensure_codepoints(new_pattern)

    if #old_pattern == 0 then
        return codepoints
    end

    local result = {}
    local i = 1

    while i <= #codepoints do
        -- Check if pattern matches at current position
        local match = false
        if i <= #codepoints - #old_pattern + 1 then
            match = true
            for j = 1, #old_pattern do
                if codepoints[i + j - 1] ~= old_pattern[j] then
                    match = false
                    break
                end
            end
        end

        if match then
            -- Replace with new pattern
            for j = 1, #new_pattern do
                result[#result + 1] = new_pattern[j]
            end
            i = i + #old_pattern
        else
            -- Copy current codepoint
            result[#result + 1] = codepoints[i]
            i = i + 1
        end
    end

    return result
end

-- TALLY - Count occurrences of pattern
function string_tally(codepoints, pattern)
    codepoints = ensure_codepoints(codepoints)
    pattern = ensure_codepoints(pattern)

    if #pattern == 0 then
        return 0
    end

    local count = 0
    local i = 1

    while i <= #codepoints - #pattern + 1 do
        local match = true
        for j = 1, #pattern do
            if codepoints[i + j - 1] ~= pattern[j] then
                match = false
                break
            end
        end
        if match then
            count = count + 1
            i = i + #pattern -- Skip past this occurrence
        else
            i = i + 1
        end
    end

    return count
end

-- INSERT$ - Insert codepoints at position
function string_insert(codepoints, pos, insert_codepoints)
    codepoints = ensure_codepoints(codepoints)
    pos = ensure_int(pos)
    insert_codepoints = ensure_codepoints(insert_codepoints)

    -- Clamp position to valid range (1-based)
    if pos < 1 then pos = 1 end
    if pos > #codepoints + 1 then pos = #codepoints + 1 end

    local result = {}

    -- Copy before insertion point
    for i = 1, pos - 1 do
        result[#result + 1] = codepoints[i]
    end

    -- Insert new codepoints
    for i = 1, #insert_codepoints do
        result[#result + 1] = insert_codepoints[i]
    end

    -- Copy after insertion point
    for i = pos, #codepoints do
        result[#result + 1] = codepoints[i]
    end

    return result
end

-- DELETE$ - Delete substring from codepoint array
function string_delete(codepoints, pos, length)
    codepoints = ensure_codepoints(codepoints)
    pos = ensure_int(pos)
    length = ensure_int(length)

    if pos < 1 or length <= 0 then
        return codepoints
    end

    local result = {}

    -- Copy before deletion
    for i = 1, pos - 1 do
        result[#result + 1] = codepoints[i]
    end

    -- Copy after deletion
    for i = pos + length, #codepoints do
        result[#result + 1] = codepoints[i]
    end

    return result
end

-- REMOVE$ - Remove all occurrences of pattern
function string_remove(codepoints, pattern)
    codepoints = ensure_codepoints(codepoints)
    pattern = ensure_codepoints(pattern)

    if #pattern == 0 then
        return codepoints
    end

    local result = {}
    local i = 1

    while i <= #codepoints do
        local match = false

        if i <= #codepoints - #pattern + 1 then
            match = true
            for j = 1, #pattern do
                if codepoints[i + j - 1] ~= pattern[j] then
                    match = false
                    break
                end
            end
        end

        if match then
            i = i + #pattern -- Skip pattern
        else
            result[#result + 1] = codepoints[i]
            i = i + 1
        end
    end

    return result
end

-- EXTRACT$ - Extract substring by start and end positions
function string_extract(codepoints, start_pos, end_pos)
    codepoints = ensure_codepoints(codepoints)
    start_pos = ensure_int(start_pos)
    end_pos = ensure_int(end_pos)

    if start_pos < 1 then start_pos = 1 end
    if end_pos > #codepoints then end_pos = #codepoints end
    if start_pos > end_pos then return {} end

    local result = {}
    for i = start_pos, end_pos do
        result[#result + 1] = codepoints[i]
    end

    return result
end

-- =============================================================================
-- Search Functions
-- =============================================================================

-- INSTRREV - Find position of substring from right
function string_instrrev(haystack, needle, start)
    haystack = ensure_codepoints(haystack)
    needle = ensure_codepoints(needle)

    if #needle == 0 then
        return 0
    end

    -- If start is -1 or nil, search from end
    if start == nil or start == -1 then
        start = #haystack
    else
        start = ensure_int(start)
    end

    -- Search backwards
    for i = start, 1, -1 do
        if i <= #haystack - #needle + 1 then
            local match = true
            for j = 1, #needle do
                if haystack[i + j - 1] ~= needle[j] then
                    match = false
                    break
                end
            end
            if match then
                return i
            end
        end
    end

    return 0 -- Not found
end

-- =============================================================================
-- Padding and Alignment Functions
-- =============================================================================

-- LPAD$ - Left pad codepoint array to width
function string_lpad(codepoints, width, padChar)
    codepoints = ensure_codepoints(codepoints)
    width = ensure_int(width)

    -- Determine pad codepoint
    local pad_codepoint = 32 -- Default: space
    if padChar ~= nil then
        if type(padChar) == "table" and #padChar > 0 then
            pad_codepoint = padChar[1]
        elseif type(padChar) == "number" then
            pad_codepoint = padChar
        elseif type(padChar) == "string" then
            local pad_array = unicode.from_utf8(padChar)
            if #pad_array > 0 then
                pad_codepoint = pad_array[1]
            end
        end
    end

    local len = #codepoints
    if len >= width then
        return codepoints
    end

    local result = {}
    local padding_needed = width - len

    -- Add padding
    for i = 1, padding_needed do
        result[i] = pad_codepoint
    end

    -- Add original codepoints
    for i = 1, len do
        result[padding_needed + i] = codepoints[i]
    end

    return result
end

-- RPAD$ - Right pad codepoint array to width
function string_rpad(codepoints, width, padChar)
    codepoints = ensure_codepoints(codepoints)
    width = ensure_int(width)

    -- Determine pad codepoint
    local pad_codepoint = 32 -- Default: space
    if padChar ~= nil then
        if type(padChar) == "table" and #padChar > 0 then
            pad_codepoint = padChar[1]
        elseif type(padChar) == "number" then
            pad_codepoint = padChar
        elseif type(padChar) == "string" then
            local pad_array = unicode.from_utf8(padChar)
            if #pad_array > 0 then
                pad_codepoint = pad_array[1]
            end
        end
    end

    local len = #codepoints
    if len >= width then
        return codepoints
    end

    local result = {}

    -- Add original codepoints
    for i = 1, len do
        result[i] = codepoints[i]
    end

    -- Add padding
    for i = len + 1, width do
        result[i] = pad_codepoint
    end

    return result
end

-- CENTER$ - Center codepoint array in field
function string_center(codepoints, width, padChar)
    codepoints = ensure_codepoints(codepoints)
    width = ensure_int(width)

    -- Determine pad codepoint
    local pad_codepoint = 32 -- Default: space
    if padChar ~= nil then
        if type(padChar) == "table" and #padChar > 0 then
            pad_codepoint = padChar[1]
        elseif type(padChar) == "number" then
            pad_codepoint = padChar
        elseif type(padChar) == "string" then
            local pad_array = unicode.from_utf8(padChar)
            if #pad_array > 0 then
                pad_codepoint = pad_array[1]
            end
        end
    end

    local len = #codepoints
    if len >= width then
        return codepoints
    end

    local total_padding = width - len
    local left_padding = math.floor(total_padding / 2)
    local right_padding = total_padding - left_padding

    local result = {}

    -- Add left padding
    for i = 1, left_padding do
        result[#result + 1] = pad_codepoint
    end

    -- Add original codepoints
    for i = 1, len do
        result[#result + 1] = codepoints[i]
    end

    -- Add right padding
    for i = 1, right_padding do
        result[#result + 1] = pad_codepoint
    end

    return result
end

-- =============================================================================
-- Additional String Functions (using existing unicode module where possible)
-- =============================================================================

-- These functions delegate to the unicode module's implementations
-- which are already defined in unicode_ffi_bindings.lua

-- LEFT$ - Already in unicode.left
function string_left(codepoints, count)
    return unicode.left(ensure_codepoints(codepoints), ensure_int(count))
end

-- RIGHT$ - Already in unicode.right
function string_right(codepoints, count)
    return unicode.right(ensure_codepoints(codepoints), ensure_int(count))
end

-- MID$ - Already in unicode.mid
function string_mid(codepoints, start, length)
    codepoints = ensure_codepoints(codepoints)
    start = ensure_int(start)

    if length == nil then
        -- MID$ without length - from start to end
        length = #codepoints - start + 1
    else
        length = ensure_int(length)
    end

    return unicode.mid(codepoints, start, length)
end

-- INSTR - Already in unicode.instr
function string_instr(haystack, needle, start)
    haystack = ensure_codepoints(haystack)
    needle = ensure_codepoints(needle)
    start = start and ensure_int(start) or 1

    return unicode.instr_start(start, haystack, needle)
end

-- UCASE$ - Already in unicode.upper (via C++ FFI)
function string_ucase(codepoints)
    return unicode.upper(ensure_codepoints(codepoints))
end

-- LCASE$ - Already in unicode.lower (via C++ FFI)
function string_lcase(codepoints)
    return unicode.lower(ensure_codepoints(codepoints))
end

-- LTRIM$ - Already in unicode.ltrim
function string_ltrim(codepoints)
    return unicode.ltrim(ensure_codepoints(codepoints))
end

-- RTRIM$ - Already in unicode.rtrim
function string_rtrim(codepoints)
    return unicode.rtrim(ensure_codepoints(codepoints))
end

-- TRIM$ - Already in unicode.trim
function string_trim(codepoints)
    return unicode.trim(ensure_codepoints(codepoints))
end

-- SPACE$ - Already in unicode.space
function string_space(count)
    return unicode.space(ensure_int(count))
end

-- STRING$ - Repeat character (codepoint)
function string_repeat(count, char)
    count = ensure_int(count)

    if count <= 0 then
        return {}
    end

    local codepoint
    if type(char) == "number" then
        codepoint = char
    elseif type(char) == "table" and #char > 0 then
        codepoint = char[1]
    elseif type(char) == "string" then
        local char_array = unicode.from_utf8(char)
        if #char_array > 0 then
            codepoint = char_array[1]
        else
            codepoint = 32 -- Default to space
        end
    else
        codepoint = 32
    end

    return unicode.string_repeat(count, codepoint)
end

-- JOIN$ - Join codepoint arrays with separator
function string_join(array, separator)
    if type(array) ~= "table" then
        return {}
    end

    separator = ensure_codepoints(separator)

    local result = {}
    for i, item in ipairs(array) do
        if i > 1 and #separator > 0 then
            -- Add separator
            for j = 1, #separator do
                result[#result + 1] = separator[j]
            end
        end

        local item_codepoints = ensure_codepoints(item)
        for j = 1, #item_codepoints do
            result[#result + 1] = item_codepoints[j]
        end
    end

    return result
end

-- SPLIT$ - Split codepoint array into array of codepoint arrays
function string_split(codepoints, delimiter)
    codepoints = ensure_codepoints(codepoints)
    delimiter = ensure_codepoints(delimiter)

    if #delimiter == 0 then
        -- Split into individual codepoints
        local result = {}
        for i = 1, #codepoints do
            result[i] = { codepoints[i] }
        end
        return result
    end

    local result = {}
    local current = {}
    local i = 1

    while i <= #codepoints do
        -- Check if delimiter matches at current position
        local match = false
        if i <= #codepoints - #delimiter + 1 then
            match = true
            for j = 1, #delimiter do
                if codepoints[i + j - 1] ~= delimiter[j] then
                    match = false
                    break
                end
            end
        end

        if match then
            -- Found delimiter - save current segment
            result[#result + 1] = current
            current = {}
            i = i + #delimiter
        else
            -- Add codepoint to current segment
            current[#current + 1] = codepoints[i]
            i = i + 1
        end
    end

    -- Add final segment
    result[#result + 1] = current

    return result
end

-- =============================================================================
-- Export all functions to global scope (for BASIC runtime)
-- =============================================================================

-- BASIC convention: All function names are UPPERCASE
_G.BIN_STRING = string_bin
_G.HEX_STRING = string_hex
_G.OCT_STRING = string_oct
_G.REVERSE_STRING = string_reverse
_G.REPLACE_STRING = string_replace
_G.TALLY = string_tally
_G.INSERT_STRING = string_insert
_G.DELETE_STRING = string_delete
_G.REMOVE_STRING = string_remove
_G.EXTRACT_STRING = string_extract
_G.INSTRREV_STRING = string_instrrev
_G.LPAD_STRING = string_lpad
_G.RPAD_STRING = string_rpad
_G.CENTER_STRING = string_center
_G.LEFT_STRING = string_left
_G.RIGHT_STRING = string_right
_G.MID_STRING = string_mid
_G.INSTR_STRING = string_instr
_G.UCASE_STRING = string_ucase
_G.LCASE_STRING = string_lcase
_G.LTRIM_STRING = string_ltrim
_G.RTRIM_STRING = string_rtrim
_G.TRIM_STRING = string_trim
_G.SPACE_STRING = string_space
_G.STRING_REPEAT = string_repeat
_G.JOIN_STRING = string_join
_G.SPLIT_STRING = string_split

-- Export with lowercase names for internal Lua usage
_G.string_bin = string_bin
_G.string_hex = string_hex
_G.string_oct = string_oct
_G.string_left = string_left
_G.string_right = string_right
_G.string_mid = string_mid
_G.string_instr = string_instr
_G.string_ucase = string_ucase
_G.string_lcase = string_lcase
_G.string_ltrim = string_ltrim
_G.string_rtrim = string_rtrim
_G.string_trim = string_trim
_G.string_space = string_space
_G.string_repeat = string_repeat
_G.string_replace = string_replace
_G.string_reverse = string_reverse
_G.string_tally = string_tally
_G.string_insert = string_insert
_G.string_delete = string_delete
_G.string_remove = string_remove
_G.string_extract = string_extract
_G.string_instrrev = string_instrrev
_G.string_lpad = string_lpad
_G.string_rpad = string_rpad
_G.string_center = string_center
_G.string_join = string_join
_G.string_split = string_split
_G.unicode_string_equal = unicode_string_equal
_G.unicode_string_compare = unicode_string_compare

return unicode_string_lib
