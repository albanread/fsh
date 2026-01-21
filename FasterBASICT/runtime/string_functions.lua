-- string_functions.lua
-- FasterBASICT Runtime String Functions Library
-- Provides BCX-compatible string manipulation functions

local string_lib = {}

-- Helper function to ensure string type
local function ensure_string(val)
    if type(val) == "string" then
        return val
    elseif type(val) == "number" then
        return tostring(val)
    elseif val == nil then
        return ""
    else
        return tostring(val)
    end
end

-- Helper function to ensure integer type
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
-- Core String Conversion Functions
-- =============================================================================

-- CHR$ - Convert ASCII/character code to character
function chr_STRING(code)
    code = ensure_int(code)
    return string.char(code)
end

-- CHR - Alias for CHR$
function chr(code)
    return chr_STRING(code)
end

-- ASC - Return ASCII/character code of first character
function asc(str)
    str = ensure_string(str)
    if #str == 0 then return 0 end
    return string.byte(str, 1)
end

-- STR$ - Convert number to string
function str_STRING(num)
    if type(num) == "string" then return num end
    if type(num) == "number" then return tostring(num) end
    return tostring(num or 0)
end

-- STR - Alias for STR$
function str(num)
    return str_STRING(num)
end

-- VAL - Convert string to number
function val(str)
    str = ensure_string(str)
    return tonumber(str) or 0
end

-- HEX$ - Convert number to hexadecimal string (alias with _STRING suffix)
function HEX_STRING(num, digits)
    return string_hex(num, digits)
end

-- BIN$ - Convert number to binary string (alias with _STRING suffix)
function BIN_STRING(num, digits)
    return string_bin(num, digits)
end

-- OCT$ - Convert number to octal string (alias with _STRING suffix)
function OCT_STRING(num, digits)
    return string_oct(num, digits)
end

-- =============================================================================
-- Core String Functions (already built-in to Lua, but provided for completeness)
-- =============================================================================

-- LEFT$ - Return leftmost characters
function string_left(str, count)
    str = ensure_string(str)
    count = ensure_int(count)
    if count <= 0 then return "" end
    return string.sub(str, 1, count)
end

-- RIGHT$ - Return rightmost characters
function string_right(str, count)
    str = ensure_string(str)
    count = ensure_int(count)
    if count <= 0 then return "" end
    local len = #str
    if count >= len then return str end
    return string.sub(str, len - count + 1)
end

-- MID$ - Return substring from middle
function string_mid(str, start, length)
    str = ensure_string(str)
    start = ensure_int(start)

    -- Handle optional length parameter
    if length == nil then
        return string.sub(str, start)
    end

    length = ensure_int(length)
    if length <= 0 then return "" end

    return string.sub(str, start, start + length - 1)
end

-- INSTR - Find position of substring
function string_instr(haystack, needle, start)
    haystack = ensure_string(haystack)
    needle = ensure_string(needle)
    start = start and ensure_int(start) or 1

    if start < 1 then start = 1 end

    local pos = string.find(haystack, needle, start, true) -- plain search
    return pos or 0                                        -- BASIC uses 0 for not found
end

-- =============================================================================
-- Case Conversion Functions
-- =============================================================================

-- UCASE$ - Convert to uppercase
function string_ucase(str)
    str = ensure_string(str)
    return string.upper(str)
end

-- LCASE$ - Convert to lowercase
function string_lcase(str)
    str = ensure_string(str)
    return string.lower(str)
end

-- =============================================================================
-- Trimming Functions
-- =============================================================================

-- LTRIM$ - Remove leading whitespace
function string_ltrim(str)
    str = ensure_string(str)
    return (string.gsub(str, "^%s+", ""))
end

-- RTRIM$ - Remove trailing whitespace
function string_rtrim(str)
    str = ensure_string(str)
    return (string.gsub(str, "%s+$", ""))
end

-- TRIM$ - Remove leading and trailing whitespace
function string_trim(str)
    str = ensure_string(str)
    return (string.gsub(str, "^%s+", "")):gsub("%s+$", "")
end

-- =============================================================================
-- String Generation Functions
-- =============================================================================

-- SPACE$ - Create string of spaces
function string_space(count)
    count = ensure_int(count)
    if count <= 0 then return "" end
    return string.rep(" ", count)
end

-- STRING$ - Repeat character or string
function string_repeat(count, char)
    count = ensure_int(count)
    char = ensure_string(char)

    if count <= 0 then return "" end
    if #char == 0 then return "" end

    -- If char is a number, treat it as ASCII code
    if type(char) == "number" then
        char = string.char(char)
    elseif #char > 1 then
        -- If multiple characters, use first one (BCX behavior)
        char = string.sub(char, 1, 1)
    end

    return string.rep(char, count)
end

-- =============================================================================
-- String Manipulation Functions
-- =============================================================================

-- REPLACE$ - Replace all occurrences of substring
function string_replace(str, oldStr, newStr)
    str = ensure_string(str)
    oldStr = ensure_string(oldStr)
    newStr = ensure_string(newStr)

    if #oldStr == 0 then return str end

    -- Escape special pattern characters for plain text replacement
    local escaped_old = oldStr:gsub("([%.%+%-%*%?%[%]%^%$%(%)%%])", "%%%1")
    return (string.gsub(str, escaped_old, newStr))
end

-- REVERSE$ - Reverse string
function string_reverse(str)
    str = ensure_string(str)
    return string.reverse(str)
end

-- TALLY - Count occurrences of substring
function string_tally(str, pattern)
    str = ensure_string(str)
    pattern = ensure_string(pattern)

    if #pattern == 0 then return 0 end

    local count = 0
    local pos = 1

    while true do
        local found = string.find(str, pattern, pos, true) -- plain search
        if not found then break end
        count = count + 1
        pos = found + 1
    end

    return count
end

-- INSERT$ - Insert substring at position
function string_insert(str, pos, insertStr)
    str = ensure_string(str)
    pos = ensure_int(pos)
    insertStr = ensure_string(insertStr)

    -- Clamp position to valid range (1-based)
    if pos < 1 then pos = 1 end
    if pos > #str + 1 then pos = #str + 1 end

    local before = string.sub(str, 1, pos - 1)
    local after = string.sub(str, pos)

    return before .. insertStr .. after
end

-- DELETE$ - Delete substring from string
function string_delete(str, pos, length)
    str = ensure_string(str)
    pos = ensure_int(pos)
    length = ensure_int(length)

    if pos < 1 or length <= 0 then return str end

    local before = string.sub(str, 1, pos - 1)
    local after = string.sub(str, pos + length)

    return before .. after
end

-- REMOVE$ - Remove all occurrences of pattern
function string_remove(str, pattern)
    str = ensure_string(str)
    pattern = ensure_string(pattern)

    if #pattern == 0 then return str end

    -- Escape special pattern characters for plain text replacement
    local escaped_pattern = pattern:gsub("([%.%+%-%*%?%[%]%^%$%(%)%%])", "%%%1")
    return (string.gsub(str, escaped_pattern, ""))
end

-- EXTRACT$ - Extract substring by start and end positions
function string_extract(str, startPos, endPos)
    str = ensure_string(str)
    startPos = ensure_int(startPos)
    endPos = ensure_int(endPos)

    if startPos < 1 then startPos = 1 end
    if endPos > #str then endPos = #str end
    if startPos > endPos then return "" end

    return string.sub(str, startPos, endPos)
end

-- =============================================================================
-- Search Functions
-- =============================================================================

-- INSTRREV - Find position of substring from right
function string_instrrev(haystack, needle, start)
    haystack = ensure_string(haystack)
    needle = ensure_string(needle)

    if #needle == 0 then return 0 end

    -- If start is -1 or nil, search from end
    if start == nil or start == -1 then
        start = #haystack
    else
        start = ensure_int(start)
    end

    -- Search backwards
    for i = start, 1, -1 do
        local found = string.find(haystack, needle, i, true)
        if found == i then
            return i
        end
    end

    return 0 -- Not found
end

-- =============================================================================
-- Padding and Alignment Functions
-- =============================================================================

-- LPAD$ - Left pad string to width
function string_lpad(str, width, padChar)
    str = ensure_string(str)
    width = ensure_int(width)
    padChar = padChar and ensure_string(padChar) or " "

    if #padChar == 0 then padChar = " " end
    if #padChar > 1 then padChar = string.sub(padChar, 1, 1) end

    local len = #str
    if len >= width then return str end

    local padding = string.rep(padChar, width - len)
    return padding .. str
end

-- RPAD$ - Right pad string to width
function string_rpad(str, width, padChar)
    str = ensure_string(str)
    width = ensure_int(width)
    padChar = padChar and ensure_string(padChar) or " "

    if #padChar == 0 then padChar = " " end
    if #padChar > 1 then padChar = string.sub(padChar, 1, 1) end

    local len = #str
    if len >= width then return str end

    local padding = string.rep(padChar, width - len)
    return str .. padding
end

-- CENTER$ - Center string in field
function string_center(str, width, padChar)
    str = ensure_string(str)
    width = ensure_int(width)
    padChar = padChar and ensure_string(padChar) or " "

    if #padChar == 0 then padChar = " " end
    if #padChar > 1 then padChar = string.sub(padChar, 1, 1) end

    local len = #str
    if len >= width then return str end

    local total_padding = width - len
    local left_padding = math.floor(total_padding / 2)
    local right_padding = total_padding - left_padding

    return string.rep(padChar, left_padding) .. str .. string.rep(padChar, right_padding)
end

-- =============================================================================
-- Number to String Conversion Functions
-- =============================================================================

-- HEX$ - Convert number to hexadecimal string
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

    return hex
end

-- BIN$ - Convert number to binary string
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

    return bin
end

-- OCT$ - Convert number to octal string
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

    return oct
end

-- =============================================================================
-- Array/String Operations
-- =============================================================================

-- JOIN$ - Join string array elements with separator
function string_join(array, separator)
    separator = ensure_string(separator)

    -- Handle different array types
    if type(array) ~= "table" then
        return ensure_string(array)
    end

    local result = {}
    for i, v in ipairs(array) do
        result[i] = ensure_string(v)
    end

    return table.concat(result, separator)
end

-- SPLIT$ - Split string into array
function string_split(str, delimiter)
    str = ensure_string(str)
    delimiter = ensure_string(delimiter)

    if #delimiter == 0 then
        -- Split into individual characters
        local result = {}
        for i = 1, #str do
            result[i] = string.sub(str, i, i)
        end
        return result
    end

    local result = {}
    local pos = 1
    local index = 1

    while true do
        local found = string.find(str, delimiter, pos, true) -- plain search
        if not found then
            result[index] = string.sub(str, pos)
            break
        end

        result[index] = string.sub(str, pos, found - 1)
        index = index + 1
        pos = found + #delimiter
    end

    return result
end

-- =============================================================================
-- Export all functions to global scope (for BASIC runtime)
-- =============================================================================

-- Make all functions globally available for BASIC programs
-- BASIC convention: All function names are UPPERCASE
_G.CHR_STRING = chr_STRING
_G.CHR = chr
_G.ASC = asc
_G.STR_STRING = str_STRING
_G.STR = str
_G.VAL = val
_G.HEX_STRING = HEX_STRING
_G.BIN_STRING = BIN_STRING
_G.OCT_STRING = OCT_STRING
-- Export with lowercase names (internal Lua convention)
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
_G.string_hex = string_hex
_G.string_bin = string_bin
_G.string_oct = string_oct
_G.string_join = string_join
_G.string_split = string_split

return string_lib
