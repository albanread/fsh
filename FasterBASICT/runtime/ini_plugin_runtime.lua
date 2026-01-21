--
-- ini_plugin_runtime.lua
-- INI Plugin Runtime for FasterBASIC
--
-- Provides INI file reading and writing for configuration files
--

local M = {}

-- =============================================================================
-- INI Object Management
-- =============================================================================

local ini_files = {}
local next_handle = 1

-- =============================================================================
-- INI Parser
-- =============================================================================

local function parse_ini_file(filename)
    local file, err = io.open(filename, "r")
    if not file then
        return nil, err
    end

    local data = {}
    local current_section = nil
    local section_order = {}

    for line in file:lines() do
        -- Trim whitespace
        line = line:match("^%s*(.-)%s*$")

        -- Skip empty lines and comments
        if line ~= "" and not line:match("^[;#]") then
            -- Check for section header
            local section = line:match("^%[(.+)%]$")
            if section then
                current_section = section
                if not data[section] then
                    data[section] = {}
                    table.insert(section_order, section)
                end
            else
                -- Parse key=value
                local key, value = line:match("^([^=]+)=(.*)$")
                if key and value then
                    key = key:match("^%s*(.-)%s*$")     -- Trim key
                    value = value:match("^%s*(.-)%s*$") -- Trim value

                    -- Remove quotes if present
                    if value:match('^".*"$') then
                        value = value:sub(2, -2)
                    elseif value:match("^'.*'$") then
                        value = value:sub(2, -2)
                    end

                    if current_section then
                        data[current_section][key] = value
                    end
                end
            end
        end
    end

    file:close()
    return data, section_order
end

-- =============================================================================
-- INI Writer
-- =============================================================================

local function write_ini_file(filename, data, section_order)
    local file, err = io.open(filename, "w")
    if not file then
        return false, err
    end

    -- Write sections in order
    for _, section in ipairs(section_order) do
        local section_data = data[section]
        if section_data then
            file:write("[" .. section .. "]\n")

            -- Write keys (sorted for consistency)
            local keys = {}
            for key in pairs(section_data) do
                table.insert(keys, key)
            end
            table.sort(keys)

            for _, key in ipairs(keys) do
                local value = section_data[key]
                -- Quote values that contain spaces or special characters
                if type(value) == "string" and (value:match("%s") or value:match("[;#]")) then
                    value = '"' .. value .. '"'
                end
                file:write(key .. "=" .. tostring(value) .. "\n")
            end

            file:write("\n")
        end
    end

    file:close()
    return true
end

-- =============================================================================
-- Plugin Functions
-- =============================================================================

-- INIOPEN(filename$) - Open/load INI file
function ini_open(filename)
    if not filename then
        error("INIOPEN requires a filename")
    end

    local data, section_order = parse_ini_file(filename)
    if not data then
        error("INIOPEN: Cannot open file - " .. tostring(section_order))
    end

    local handle = next_handle
    next_handle = next_handle + 1

    ini_files[handle] = {
        filename = filename,
        data = data,
        section_order = section_order,
        modified = false
    }

    return handle
end

-- INICREATE(filename$) - Create new INI file
function ini_create(filename)
    if not filename then
        error("INICREATE requires a filename")
    end

    local handle = next_handle
    next_handle = next_handle + 1

    ini_files[handle] = {
        filename = filename,
        data = {},
        section_order = {},
        modified = true
    }

    return handle
end

-- Helper to get INI object
local function get_ini(handle)
    handle = tonumber(handle)
    if not handle or not ini_files[handle] then
        error("Invalid INI handle: " .. tostring(handle))
    end
    return ini_files[handle]
end

-- INIGET(handle, section$, key$, default$) - Get string value
function ini_get(handle, section, key, default_value)
    local ini = get_ini(handle)

    if ini.data[section] and ini.data[section][key] then
        return tostring(ini.data[section][key])
    end

    return default_value or ""
end

-- INIGETINT(handle, section$, key$, default) - Get integer value
function ini_get_int(handle, section, key, default_value)
    local ini = get_ini(handle)

    if ini.data[section] and ini.data[section][key] then
        return tonumber(ini.data[section][key]) or default_value or 0
    end

    return default_value or 0
end

-- INIGETFLOAT(handle, section$, key$, default) - Get float value
function ini_get_float(handle, section, key, default_value)
    local ini = get_ini(handle)

    if ini.data[section] and ini.data[section][key] then
        return tonumber(ini.data[section][key]) or default_value or 0.0
    end

    return default_value or 0.0
end

-- INISET(handle, section$, key$, value$) - Set string value
function ini_set(handle, section, key, value)
    local ini = get_ini(handle)

    if not ini.data[section] then
        ini.data[section] = {}
        table.insert(ini.section_order, section)
    end

    ini.data[section][key] = tostring(value)
    ini.modified = true

    return 1
end

-- INISETINT(handle, section$, key$, value) - Set integer value
function ini_set_int(handle, section, key, value)
    local ini = get_ini(handle)

    if not ini.data[section] then
        ini.data[section] = {}
        table.insert(ini.section_order, section)
    end

    ini.data[section][key] = tostring(tonumber(value) or 0)
    ini.modified = true

    return 1
end

-- INISETFLOAT(handle, section$, key$, value) - Set float value
function ini_set_float(handle, section, key, value)
    local ini = get_ini(handle)

    if not ini.data[section] then
        ini.data[section] = {}
        table.insert(ini.section_order, section)
    end

    ini.data[section][key] = tostring(tonumber(value) or 0.0)
    ini.modified = true

    return 1
end

-- INIDELETE(handle, section$, key$) - Delete a key
function ini_delete(handle, section, key)
    local ini = get_ini(handle)

    if ini.data[section] and ini.data[section][key] then
        ini.data[section][key] = nil
        ini.modified = true
        return 1
    end

    return 0
end

-- INIDELETESECTION(handle, section$) - Delete entire section
function ini_delete_section(handle, section)
    local ini = get_ini(handle)

    if ini.data[section] then
        ini.data[section] = nil

        -- Remove from section order
        for i, s in ipairs(ini.section_order) do
            if s == section then
                table.remove(ini.section_order, i)
                break
            end
        end

        ini.modified = true
        return 1
    end

    return 0
end

-- INIEXISTS(handle, section$, key$) - Check if key exists
function ini_exists(handle, section, key)
    local ini = get_ini(handle)

    if ini.data[section] and ini.data[section][key] ~= nil then
        return 1
    end

    return 0
end

-- INISECTIONEXISTS(handle, section$) - Check if section exists
function ini_section_exists(handle, section)
    local ini = get_ini(handle)

    if ini.data[section] then
        return 1
    end

    return 0
end

-- INISAVE(handle) - Save changes to file
function ini_save(handle)
    local ini = get_ini(handle)

    local success, err = write_ini_file(ini.filename, ini.data, ini.section_order)
    if not success then
        error("INISAVE: Cannot save file - " .. tostring(err))
    end

    ini.modified = false
    return 1
end

-- INISAVEAS(handle, filename$) - Save to different file
function ini_save_as(handle, filename)
    if not filename then
        error("INISAVEAS requires a filename")
    end

    local ini = get_ini(handle)

    local success, err = write_ini_file(filename, ini.data, ini.section_order)
    if not success then
        error("INISAVEAS: Cannot save file - " .. tostring(err))
    end

    ini.filename = filename
    ini.modified = false
    return 1
end

-- INISECTIONS(handle) - Get count of sections
function ini_sections(handle)
    local ini = get_ini(handle)
    return #ini.section_order
end

-- INIKEYS(handle, section$) - Get count of keys in section
function ini_keys(handle, section)
    local ini = get_ini(handle)

    if not ini.data[section] then
        return 0
    end

    local count = 0
    for _ in pairs(ini.data[section]) do
        count = count + 1
    end

    return count
end

-- INIGETSECTION(handle, index) - Get section name by index
function ini_get_section(handle, index)
    local ini = get_ini(handle)

    -- Convert to 1-based index
    local lua_index = tonumber(index) + 1

    return ini.section_order[lua_index] or ""
end

-- INIGETKEY(handle, section$, index) - Get key name by index
function ini_get_key(handle, section, index)
    local ini = get_ini(handle)

    if not ini.data[section] then
        return ""
    end

    -- Collect keys and sort for consistent ordering
    local keys = {}
    for key in pairs(ini.data[section]) do
        table.insert(keys, key)
    end
    table.sort(keys)

    -- Convert to 1-based index
    local lua_index = tonumber(index) + 1

    return keys[lua_index] or ""
end

-- INICLOSE(handle) - Close INI file
function ini_close(handle)
    handle = tonumber(handle)

    if handle and ini_files[handle] then
        ini_files[handle] = nil
        return 1
    end

    return 0
end

-- Export all functions
return {
    ini_open = ini_open,
    ini_create = ini_create,
    ini_get = ini_get,
    ini_get_int = ini_get_int,
    ini_get_float = ini_get_float,
    ini_set = ini_set,
    ini_set_int = ini_set_int,
    ini_set_float = ini_set_float,
    ini_delete = ini_delete,
    ini_delete_section = ini_delete_section,
    ini_exists = ini_exists,
    ini_section_exists = ini_section_exists,
    ini_save = ini_save,
    ini_save_as = ini_save_as,
    ini_sections = ini_sections,
    ini_keys = ini_keys,
    ini_get_section = ini_get_section,
    ini_get_key = ini_get_key,
    ini_close = ini_close
}
