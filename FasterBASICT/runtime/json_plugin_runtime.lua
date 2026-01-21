--
-- json_plugin_runtime.lua
-- JSON Plugin Runtime for FasterBASIC
--
-- Provides JSON parsing, generation, and manipulation
--

local M = {}

-- =============================================================================
-- Simple JSON Parser/Generator (Pure Lua)
-- =============================================================================

-- JSON encoder
local function encode_string(str)
    local replacements = {
        ['"'] = '\\"',
        ['\\'] = '\\\\',
        ['/'] = '\\/',
        ['\b'] = '\\b',
        ['\f'] = '\\f',
        ['\n'] = '\\n',
        ['\r'] = '\\r',
        ['\t'] = '\\t'
    }
    return '"' .. str:gsub('["\\\b\f\n\r\t/]', replacements) .. '"'
end

local function encode_value(value, indent, current_indent)
    local value_type = type(value)

    if value_type == "string" then
        return encode_string(value)
    elseif value_type == "number" then
        return tostring(value)
    elseif value_type == "boolean" then
        return value and "true" or "false"
    elseif value == nil then
        return "null"
    elseif value_type == "table" then
        -- Check if it's an array or object
        local is_array = false
        local count = 0
        for k, v in pairs(value) do
            count = count + 1
            if type(k) == "number" and k == count then
                is_array = true
            else
                is_array = false
                break
            end
        end

        -- Empty table defaults to object
        if count == 0 then
            is_array = false
        end

        if is_array then
            -- Encode array
            local parts = {}
            for i = 1, count do
                table.insert(parts, encode_value(value[i], indent, current_indent + indent))
            end
            if indent > 0 then
                local spacing = string.rep(" ", current_indent + indent)
                local end_spacing = string.rep(" ", current_indent)
                return "[\n" .. spacing .. table.concat(parts, ",\n" .. spacing) .. "\n" .. end_spacing .. "]"
            else
                return "[" .. table.concat(parts, ",") .. "]"
            end
        else
            -- Encode object
            local parts = {}
            for k, v in pairs(value) do
                local key = encode_string(tostring(k))
                local val = encode_value(v, indent, current_indent + indent)
                table.insert(parts, key .. ":" .. val)
            end
            if indent > 0 then
                local spacing = string.rep(" ", current_indent + indent)
                local end_spacing = string.rep(" ", current_indent)
                return "{\n" .. spacing .. table.concat(parts, ",\n" .. spacing) .. "\n" .. end_spacing .. "}"
            else
                return "{" .. table.concat(parts, ",") .. "}"
            end
        end
    else
        error("Cannot encode type: " .. value_type)
    end
end

local function json_encode(value)
    return encode_value(value, 0, 0)
end

-- JSON decoder
local function skip_whitespace(str, pos)
    while pos <= #str do
        local char = str:sub(pos, pos)
        if char ~= ' ' and char ~= '\t' and char ~= '\n' and char ~= '\r' then
            break
        end
        pos = pos + 1
    end
    return pos
end

local function decode_string(str, pos)
    local result = ""
    pos = pos + 1 -- Skip opening quote

    while pos <= #str do
        local char = str:sub(pos, pos)

        if char == '"' then
            return result, pos + 1
        elseif char == '\\' then
            pos = pos + 1
            local escape = str:sub(pos, pos)
            if escape == 'n' then
                result = result .. '\n'
            elseif escape == 't' then
                result = result .. '\t'
            elseif escape == 'r' then
                result = result .. '\r'
            elseif escape == 'b' then
                result = result .. '\b'
            elseif escape == 'f' then
                result = result .. '\f'
            elseif escape == '"' or escape == '\\' or escape == '/' then
                result = result .. escape
            elseif escape == 'u' then
                -- Unicode escape (simplified - just pass through)
                local hex = str:sub(pos + 1, pos + 4)
                result = result .. string.char(tonumber(hex, 16) or 0)
                pos = pos + 4
            end
            pos = pos + 1
        else
            result = result .. char
            pos = pos + 1
        end
    end

    error("Unterminated string")
end

local decode_value -- Forward declaration

local function decode_array(str, pos)
    local result = {}
    pos = pos + 1 -- Skip opening bracket
    pos = skip_whitespace(str, pos)

    -- Empty array
    if str:sub(pos, pos) == ']' then
        return result, pos + 1
    end

    while pos <= #str do
        local value, new_pos = decode_value(str, pos)
        table.insert(result, value)
        pos = new_pos
        pos = skip_whitespace(str, pos)

        local char = str:sub(pos, pos)
        if char == ']' then
            return result, pos + 1
        elseif char == ',' then
            pos = pos + 1
            pos = skip_whitespace(str, pos)
        else
            error("Expected ',' or ']' in array")
        end
    end

    error("Unterminated array")
end

local function decode_object(str, pos)
    local result = {}
    pos = pos + 1 -- Skip opening brace
    pos = skip_whitespace(str, pos)

    -- Empty object
    if str:sub(pos, pos) == '}' then
        return result, pos + 1
    end

    while pos <= #str do
        pos = skip_whitespace(str, pos)

        -- Parse key (must be string)
        if str:sub(pos, pos) ~= '"' then
            error("Expected string key in object")
        end
        local key, new_pos = decode_string(str, pos)
        pos = new_pos
        pos = skip_whitespace(str, pos)

        -- Expect colon
        if str:sub(pos, pos) ~= ':' then
            error("Expected ':' after key in object")
        end
        pos = pos + 1
        pos = skip_whitespace(str, pos)

        -- Parse value
        local value
        value, pos = decode_value(str, pos)
        result[key] = value

        pos = skip_whitespace(str, pos)
        local char = str:sub(pos, pos)
        if char == '}' then
            return result, pos + 1
        elseif char == ',' then
            pos = pos + 1
        else
            error("Expected ',' or '}' in object")
        end
    end

    error("Unterminated object")
end

function decode_value(str, pos)
    pos = skip_whitespace(str, pos)
    local char = str:sub(pos, pos)

    if char == '"' then
        return decode_string(str, pos)
    elseif char == '{' then
        return decode_object(str, pos)
    elseif char == '[' then
        return decode_array(str, pos)
    elseif str:sub(pos, pos + 3) == 'null' then
        return nil, pos + 4
    elseif str:sub(pos, pos + 3) == 'true' then
        return true, pos + 4
    elseif str:sub(pos, pos + 4) == 'false' then
        return false, pos + 5
    else
        -- Try to parse number
        local num_str = str:match('^-?%d+%.?%d*[eE]?[+-]?%d*', pos)
        if num_str then
            return tonumber(num_str), pos + #num_str
        end
    end

    error("Invalid JSON value at position " .. pos)
end

local function json_decode(str)
    if not str or str == "" then
        error("Empty JSON string")
    end
    local value, pos = decode_value(str, 1)
    return value
end

-- =============================================================================
-- JSON Object Management
-- =============================================================================

local json_objects = {}
local next_handle = 1

local function get_object(handle)
    handle = tonumber(handle)
    if not handle or not json_objects[handle] then
        error("Invalid JSON handle: " .. tostring(handle))
    end
    return json_objects[handle]
end

local function navigate_path(obj, path)
    if not path or path == "" then
        return obj
    end

    local current = obj
    -- Split path by dots
    for part in path:gmatch("[^.]+") do
        -- Check if it's an array index [n]
        local array_index = part:match("^%[(%d+)%]$")
        if array_index then
            current = current[tonumber(array_index) + 1] -- Lua is 1-indexed
        else
            current = current[part]
        end

        if current == nil then
            return nil
        end
    end

    return current
end

local function set_by_path(obj, path, value)
    if not path or path == "" then
        error("Cannot set root object")
    end

    -- Split path into parts
    local parts = {}
    for part in path:gmatch("[^.]+") do
        table.insert(parts, part)
    end

    local current = obj
    for i = 1, #parts - 1 do
        local part = parts[i]
        local array_index = part:match("^%[(%d+)%]$")

        if array_index then
            local idx = tonumber(array_index) + 1
            if not current[idx] then
                current[idx] = {}
            end
            current = current[idx]
        else
            if not current[part] then
                current[part] = {}
            end
            current = current[part]
        end
    end

    -- Set the final value
    local last_part = parts[#parts]
    local array_index = last_part:match("^%[(%d+)%]$")
    if array_index then
        current[tonumber(array_index) + 1] = value
    else
        current[last_part] = value
    end
end

-- =============================================================================
-- Plugin Functions
-- =============================================================================

-- JSONCREATE() - Create new empty JSON object
function json_create()
    local handle = next_handle
    next_handle = next_handle + 1
    json_objects[handle] = {}
    return handle
end

-- JSONPARSE(json_string$) - Parse JSON string
function json_parse(json_string)
    if not json_string then
        error("JSONPARSE requires a JSON string")
    end

    local success, obj = pcall(json_decode, json_string)
    if not success then
        error("JSONPARSE: Invalid JSON - " .. tostring(obj))
    end

    local handle = next_handle
    next_handle = next_handle + 1
    json_objects[handle] = obj
    return handle
end

-- JSONLOAD(filename$) - Load JSON from file
function json_load(filename)
    if not filename then
        error("JSONLOAD requires a filename")
    end

    local file, err = io.open(filename, "r")
    if not file then
        error("JSONLOAD: Cannot open file - " .. tostring(err))
    end

    local content = file:read("*all")
    file:close()

    return json_parse(content)
end

-- JSONSTRINGIFY(handle) - Convert to JSON string
function json_stringify(handle)
    local obj = get_object(handle)
    local success, result = pcall(json_encode, obj)
    if not success then
        error("JSONSTRINGIFY: Encoding error - " .. tostring(result))
    end
    return result
end

-- JSONSAVE(handle, filename$) - Save JSON to file
function json_save(handle, filename)
    if not filename then
        error("JSONSAVE requires a filename")
    end

    local json_str = json_stringify(handle)

    local file, err = io.open(filename, "w")
    if not file then
        error("JSONSAVE: Cannot open file - " .. tostring(err))
    end

    file:write(json_str)
    file:close()

    return 1 -- Success
end

-- JSONGET(handle, path$) - Get value by path as string
function json_get(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)

    if value == nil then
        return ""
    elseif type(value) == "boolean" then
        return value and "true" or "false"
    elseif type(value) == "table" then
        return json_encode(value)
    else
        return tostring(value)
    end
end

-- JSONGETSTRING(handle, path$) - Get string value
function json_get_string(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)

    if value == nil then
        return ""
    else
        return tostring(value)
    end
end

-- JSONGETNUMBER(handle, path$) - Get numeric value
function json_get_number(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)

    if value == nil then
        return 0
    else
        return tonumber(value) or 0
    end
end

-- JSONGETBOOL(handle, path$) - Get boolean value
function json_get_bool(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)

    if value == nil then
        return 0
    elseif type(value) == "boolean" then
        return value and 1 or 0
    else
        return (value and value ~= 0 and value ~= "") and 1 or 0
    end
end

-- JSONSET(handle, path$, value$) - Set string value
function json_set(handle, path, value)
    local obj = get_object(handle)
    set_by_path(obj, path, tostring(value))
    return 1
end

-- JSONSETNUMBER(handle, path$, value) - Set numeric value
function json_set_number(handle, path, value)
    local obj = get_object(handle)
    set_by_path(obj, path, tonumber(value))
    return 1
end

-- JSONSETBOOL(handle, path$, value) - Set boolean value
function json_set_bool(handle, path, value)
    local obj = get_object(handle)
    set_by_path(obj, path, value ~= 0)
    return 1
end

-- JSONSETNULL(handle, path$) - Set null value
function json_set_null(handle, path)
    local obj = get_object(handle)
    set_by_path(obj, path, nil)
    return 1
end

-- JSONTYPE(handle, path$) - Get type of value
function json_type(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)

    if value == nil then
        return "null"
    end

    local lua_type = type(value)
    if lua_type == "table" then
        -- Check if array or object
        local is_array = false
        local count = 0
        for k, v in pairs(value) do
            count = count + 1
            if type(k) == "number" and k == count then
                is_array = true
            else
                is_array = false
                break
            end
        end
        return is_array and "array" or "object"
    elseif lua_type == "number" then
        return "number"
    elseif lua_type == "string" then
        return "string"
    elseif lua_type == "boolean" then
        return "boolean"
    else
        return "unknown"
    end
end

-- JSONEXISTS(handle, path$) - Check if path exists
function json_exists(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)
    return (value ~= nil) and 1 or 0
end

-- JSONCOUNT(handle, path$) - Count array/object items
function json_count(handle, path)
    local obj = get_object(handle)
    local value = navigate_path(obj, path)

    if type(value) ~= "table" then
        return 0
    end

    local count = 0
    for k, v in pairs(value) do
        count = count + 1
    end

    return count
end

-- JSONARRAYPUSH(handle, path$, value$) - Add string to array
function json_array_push(handle, path, value)
    local obj = get_object(handle)
    local arr = navigate_path(obj, path)

    if type(arr) ~= "table" then
        error("JSONARRAYPUSH: Path does not point to an array")
    end

    table.insert(arr, tostring(value))
    return 1
end

-- JSONARRAYPUSHNUM(handle, path$, value) - Add number to array
function json_array_push_number(handle, path, value)
    local obj = get_object(handle)
    local arr = navigate_path(obj, path)

    if type(arr) ~= "table" then
        error("JSONARRAYPUSHNUM: Path does not point to an array")
    end

    table.insert(arr, tonumber(value))
    return 1
end

-- JSONDELETE(handle, path$) - Delete a key/element
function json_delete(handle, path)
    if not path or path == "" then
        error("JSONDELETE: Cannot delete root")
    end

    local obj = get_object(handle)

    -- Split path into parts
    local parts = {}
    for part in path:gmatch("[^.]+") do
        table.insert(parts, part)
    end

    local current = obj
    for i = 1, #parts - 1 do
        local part = parts[i]
        local array_index = part:match("^%[(%d+)%]$")

        if array_index then
            current = current[tonumber(array_index) + 1]
        else
            current = current[part]
        end

        if current == nil then
            return 0 -- Path doesn't exist
        end
    end

    -- Delete the final key
    local last_part = parts[#parts]
    local array_index = last_part:match("^%[(%d+)%]$")
    if array_index then
        table.remove(current, tonumber(array_index) + 1)
    else
        current[last_part] = nil
    end

    return 1
end

-- JSONCLOSE(handle) - Close handle
function json_close(handle)
    handle = tonumber(handle)
    if handle and json_objects[handle] then
        json_objects[handle] = nil
        return 1
    end
    return 0
end

-- Export all functions
return {
    json_create = json_create,
    json_parse = json_parse,
    json_load = json_load,
    json_stringify = json_stringify,
    json_save = json_save,
    json_get = json_get,
    json_get_string = json_get_string,
    json_get_number = json_get_number,
    json_get_bool = json_get_bool,
    json_set = json_set,
    json_set_number = json_set_number,
    json_set_bool = json_set_bool,
    json_set_null = json_set_null,
    json_type = json_type,
    json_exists = json_exists,
    json_count = json_count,
    json_array_push = json_array_push,
    json_array_push_number = json_array_push_number,
    json_delete = json_delete,
    json_close = json_close
}
