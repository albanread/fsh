--
-- csv_plugin_runtime.lua
-- CSV Plugin Runtime for FasterBASIC
--
-- Provides CSV file reading and writing using C++ parser
--

local M = {}

-- Pure Lua CSV implementation - no FFI needed

-- =============================================================================
-- CSV Object Management
-- =============================================================================

local csv_readers = {}
local csv_writers = {}
local next_handle = 1

-- Current row data for each reader
local current_rows = {}

-- =============================================================================
-- Helper Functions
-- =============================================================================

-- Parse a CSV line handling quotes and escapes
local function parse_csv_line(line, delimiter)
    delimiter = delimiter or ','
    local fields = {}
    local field = ""
    local in_quotes = false
    local i = 1

    while i <= #line do
        local c = line:sub(i, i)

        if c == '"' then
            if in_quotes then
                -- Check if next char is also a quote (escaped)
                if i < #line and line:sub(i + 1, i + 1) == '"' then
                    field = field .. '"'
                    i = i + 1
                else
                    in_quotes = false
                end
            else
                in_quotes = true
            end
        elseif c == delimiter and not in_quotes then
            table.insert(fields, field)
            field = ""
        else
            field = field .. c
        end

        i = i + 1
    end

    table.insert(fields, field)
    return fields
end

-- Escape a field for CSV output
local function escape_csv_field(field, delimiter)
    delimiter = delimiter or ','
    field = tostring(field)

    -- Check if field needs quoting
    local needs_quotes = false
    if field:find(delimiter) or field:find('"') or field:find('\n') or field:find('\r') then
        needs_quotes = true
    end

    if not needs_quotes then
        return field
    end

    -- Escape quotes by doubling them
    field = field:gsub('"', '""')
    return '"' .. field .. '"'
end

-- =============================================================================
-- Plugin Functions
-- =============================================================================

-- CSVOPEN(filename$, has_header) - Open CSV file for reading
function csv_open(filename, has_header)
    if not filename then
        error("CSVOPEN requires a filename")
    end

    has_header = tonumber(has_header) or 0

    local file, err = io.open(filename, "r")
    if not file then
        error("CSVOPEN: Cannot open file - " .. tostring(err))
    end

    local handle = next_handle
    next_handle = next_handle + 1

    local reader = {
        file = file,
        filename = filename,
        has_header = (has_header ~= 0),
        delimiter = ',',
        headers = {},
        rows = {},
        current_row = 0,
        loaded = false
    }

    -- Load entire file into memory
    local first_line = true
    for line in file:lines() do
        if line and line ~= "" then
            local fields = parse_csv_line(line, reader.delimiter)

            if first_line and reader.has_header then
                reader.headers = fields
            else
                table.insert(reader.rows, fields)
            end
            first_line = false
        end
    end

    file:close()
    reader.loaded = true

    csv_readers[handle] = reader
    current_rows[handle] = nil

    return handle
end

-- CSVSETDELIMITER(handle, delimiter$) - Set delimiter (must be called before reading)
function csv_set_delimiter(handle, delimiter)
    handle = tonumber(handle)
    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    if not delimiter or delimiter == "" then
        delimiter = ','
    else
        delimiter = delimiter:sub(1, 1)
    end

    csv_readers[handle].delimiter = delimiter

    -- Re-parse if already loaded
    if csv_readers[handle].loaded then
        -- Would need to reload file - for now just set for future use
    end

    return 1
end

-- CSVREAD(handle) - Read next row
function csv_read(handle)
    handle = tonumber(handle)
    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local reader = csv_readers[handle]
    reader.current_row = reader.current_row + 1

    if reader.current_row <= #reader.rows then
        current_rows[handle] = reader.rows[reader.current_row]
        return 1
    else
        current_rows[handle] = nil
        return 0
    end
end

-- CSVEOF(handle) - Check if at end
function csv_eof(handle)
    handle = tonumber(handle)
    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local reader = csv_readers[handle]
    return (reader.current_row >= #reader.rows) and 1 or 0
end

-- CSVGET(handle, column) - Get field from current row
function csv_get(handle, column)
    handle = tonumber(handle)
    column = tonumber(column)

    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local row = current_rows[handle]
    if not row then
        return ""
    end

    -- Convert to 1-based index
    local index = column + 1
    return row[index] or ""
end

-- CSVGETBYNAME(handle, name$) - Get field by header name
function csv_get_by_name(handle, name)
    handle = tonumber(handle)

    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local reader = csv_readers[handle]
    if not reader.has_header then
        error("CSVGETBYNAME: File has no headers")
    end

    -- Find column index
    local col_index = -1
    for i, header in ipairs(reader.headers) do
        if header == name then
            col_index = i - 1 -- Convert to 0-based
            break
        end
    end

    if col_index < 0 then
        return ""
    end

    return csv_get(handle, col_index)
end

-- CSVCOLCOUNT(handle) - Get number of columns
function csv_col_count(handle)
    handle = tonumber(handle)
    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local reader = csv_readers[handle]
    if reader.has_header and #reader.headers > 0 then
        return #reader.headers
    elseif #reader.rows > 0 then
        return #reader.rows[1]
    end

    return 0
end

-- CSVHEADER(handle, index) - Get header name
function csv_header(handle, index)
    handle = tonumber(handle)
    index = tonumber(index)

    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local reader = csv_readers[handle]
    if not reader.has_header then
        return ""
    end

    -- Convert to 1-based index
    local lua_index = index + 1
    return reader.headers[lua_index] or ""
end

-- CSVFINDCOL(handle, name$) - Find column index by name
function csv_find_col(handle, name)
    handle = tonumber(handle)

    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    local reader = csv_readers[handle]
    if not reader.has_header then
        return -1
    end

    for i, header in ipairs(reader.headers) do
        if header == name then
            return i - 1 -- Return 0-based index
        end
    end

    return -1
end

-- CSVRESET(handle) - Reset to beginning
function csv_reset(handle)
    handle = tonumber(handle)
    if not handle or not csv_readers[handle] then
        error("Invalid CSV handle")
    end

    csv_readers[handle].current_row = 0
    current_rows[handle] = nil
    return 1
end

-- CSVCREATE(filename$) - Create CSV for writing
function csv_create(filename)
    if not filename then
        error("CSVCREATE requires a filename")
    end

    local file, err = io.open(filename, "w")
    if not file then
        error("CSVCREATE: Cannot create file - " .. tostring(err))
    end

    local handle = next_handle
    next_handle = next_handle + 1

    local writer = {
        file = file,
        filename = filename,
        delimiter = ',',
        closed = false
    }

    csv_writers[handle] = writer
    return handle
end

-- CSVWRITEROW(handle, fields$) - Write row (internal, comma-separated fields)
-- This is called from a wrapper function that formats the fields
function csv_write_row(handle, fields_str)
    handle = tonumber(handle)

    if not handle or not csv_writers[handle] then
        error("Invalid CSV write handle")
    end

    local writer = csv_writers[handle]
    if writer.closed then
        error("CSV file is closed")
    end

    -- Parse the comma-separated fields string
    local fields = {}
    for field in fields_str:gmatch("([^,]+)") do
        table.insert(fields, field)
    end

    -- Write escaped fields
    local output = {}
    for i, field in ipairs(fields) do
        table.insert(output, escape_csv_field(field, writer.delimiter))
    end

    writer.file:write(table.concat(output, writer.delimiter) .. "\n")
    return 1
end

-- Helper function to write a row from a table
function csv_write_fields(handle, fields)
    handle = tonumber(handle)

    if not handle or not csv_writers[handle] then
        error("Invalid CSV write handle")
    end

    local writer = csv_writers[handle]
    if writer.closed then
        error("CSV file is closed")
    end

    -- Write escaped fields
    local output = {}
    for i, field in ipairs(fields) do
        table.insert(output, escape_csv_field(field, writer.delimiter))
    end

    writer.file:write(table.concat(output, writer.delimiter) .. "\n")
    return 1
end

-- CSVCLOSE(handle) - Close CSV file
function csv_close(handle)
    handle = tonumber(handle)

    -- Try to close reader
    if csv_readers[handle] then
        -- File already closed during load
        csv_readers[handle] = nil
        current_rows[handle] = nil
        return 1
    end

    -- Try to close writer
    if csv_writers[handle] then
        local writer = csv_writers[handle]
        if not writer.closed and writer.file then
            writer.file:close()
            writer.closed = true
        end
        csv_writers[handle] = nil
        return 1
    end

    return 0
end

-- Export all functions
return {
    csv_open = csv_open,
    csv_set_delimiter = csv_set_delimiter,
    csv_read = csv_read,
    csv_eof = csv_eof,
    csv_get = csv_get,
    csv_get_by_name = csv_get_by_name,
    csv_col_count = csv_col_count,
    csv_header = csv_header,
    csv_find_col = csv_find_col,
    csv_reset = csv_reset,
    csv_create = csv_create,
    csv_write_row = csv_write_row,
    csv_write_fields = csv_write_fields,
    csv_close = csv_close
}
