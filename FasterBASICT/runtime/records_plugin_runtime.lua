-- records_plugin_runtime.lua
-- Runtime implementation for Records plugin
--
-- Provides simple random access record files using SQLite backend.
-- The user doesn't need to know about databases - it just works like
-- classic BASIC random files with TYPE integration.

local ffi = require("ffi")

-- =============================================================================
-- SQLite3 FFI Definitions
-- =============================================================================

ffi.cdef [[
    typedef struct sqlite3 sqlite3;
    typedef struct sqlite3_stmt sqlite3_stmt;

    int sqlite3_open(const char *filename, sqlite3 **ppDb);
    int sqlite3_close(sqlite3 *db);
    int sqlite3_exec(sqlite3 *db, const char *sql, void *callback, void *arg, char **errmsg);
    void sqlite3_free(void *ptr);
    const char *sqlite3_errmsg(sqlite3 *db);

    int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);
    int sqlite3_step(sqlite3_stmt *stmt);
    int sqlite3_finalize(sqlite3_stmt *stmt);
    int sqlite3_reset(sqlite3_stmt *stmt);

    int sqlite3_bind_int(sqlite3_stmt *stmt, int index, int value);
    int sqlite3_bind_double(sqlite3_stmt *stmt, int index, double value);
    int sqlite3_bind_text(sqlite3_stmt *stmt, int index, const char *value, int n, void *destructor);

    int sqlite3_column_int(sqlite3_stmt *stmt, int iCol);
    double sqlite3_column_double(sqlite3_stmt *stmt, int iCol);
    const unsigned char *sqlite3_column_text(sqlite3_stmt *stmt, int iCol);
    int sqlite3_column_type(sqlite3_stmt *stmt, int iCol);
]]

local sqlite3 = ffi.load("sqlite3")

-- =============================================================================
-- State Management
-- =============================================================================

local handles = {}
local next_handle = 1
local search_results = {}

-- =============================================================================
-- Helper Functions
-- =============================================================================

local function exec_sql(db, sql)
    local errmsg = ffi.new("char*[1]")
    local rc = sqlite3.sqlite3_exec(db, sql, nil, nil, errmsg)
    if rc ~= 0 then
        local msg = ffi.string(errmsg[0])
        sqlite3.sqlite3_free(errmsg[0])
        error("Records error: " .. msg)
    end
end

local function prepare_stmt(db, sql)
    local stmt = ffi.new("sqlite3_stmt*[1]")
    local rc = sqlite3.sqlite3_prepare_v2(db, sql, -1, stmt, nil)
    if rc ~= 0 then
        error("Records error: " .. ffi.string(sqlite3.sqlite3_errmsg(db)))
    end
    return stmt[0]
end

local function get_handle(handle)
    local h = handles[handle]
    if not h then
        error("Invalid record file handle: " .. tostring(handle))
    end
    return h
end

-- =============================================================================
-- RECOPEN(filename$, typename$, schema_table) - Open record file
-- =============================================================================
function rec_open(filename, typename, schema)
    if type(filename) ~= "string" then
        error("RECOPEN: filename must be a string")
    end
    if type(typename) ~= "string" then
        error("RECOPEN: typename must be a string")
    end
    if type(schema) ~= "table" or not schema.fields then
        error("RECOPEN: invalid schema (compiler should generate this)")
    end

    -- Open SQLite database
    local db = ffi.new("sqlite3*[1]")
    local rc = sqlite3.sqlite3_open(filename, db)
    if rc ~= 0 then
        error("RECOPEN: Failed to open " .. filename)
    end

    local dbh = db[0]

    -- Build CREATE TABLE statement from schema
    local cols = {}
    table.insert(cols, "_recnum INTEGER PRIMARY KEY")

    for _, field in ipairs(schema.fields) do
        local sqltype = field.sqltype or "TEXT"
        table.insert(cols, field.name .. " " .. sqltype)
    end

    local create_sql = string.format(
        "CREATE TABLE IF NOT EXISTS %s (%s)",
        typename,
        table.concat(cols, ", ")
    )

    exec_sql(dbh, create_sql)

    -- Create indexes for common searches
    for _, field in ipairs(schema.fields) do
        local idx_sql = string.format(
            "CREATE INDEX IF NOT EXISTS idx_%s_%s ON %s(%s)",
            typename, field.name, typename, field.name
        )
        pcall(function() exec_sql(dbh, idx_sql) end)
    end

    -- Store handle info
    local handle = next_handle
    next_handle = next_handle + 1

    handles[handle] = {
        db = dbh,
        filename = filename,
        typename = typename,
        schema = schema
    }

    return handle
end

-- =============================================================================
-- RECCLOSE(handle) - Close record file
-- =============================================================================
function rec_close(handle)
    local h = handles[handle]
    if not h then return end

    sqlite3.sqlite3_close(h.db)
    handles[handle] = nil
end

-- =============================================================================
-- RECPUT(handle, recnum, record_table) - Store record
-- =============================================================================
function rec_put(handle, recnum, record)
    if type(recnum) ~= "number" then
        error("RECPUT: record number must be a number")
    end
    if type(record) ~= "table" then
        error("RECPUT: record must be a table")
    end

    recnum = math.floor(recnum)
    local h = get_handle(handle)

    -- Build INSERT OR REPLACE statement
    local fields = { "_recnum" }
    local placeholders = { "?" }

    for _, field in ipairs(h.schema.fields) do
        table.insert(fields, field.name)
        table.insert(placeholders, "?")
    end

    local sql = string.format(
        "INSERT OR REPLACE INTO %s (%s) VALUES (%s)",
        h.typename,
        table.concat(fields, ", "),
        table.concat(placeholders, ", ")
    )

    local stmt = prepare_stmt(h.db, sql)

    -- Bind record number
    sqlite3.sqlite3_bind_int(stmt, 1, recnum)

    -- Bind field values
    local idx = 2
    for _, field in ipairs(h.schema.fields) do
        local value = record[field.name]

        if field.sqltype == "INTEGER" then
            sqlite3.sqlite3_bind_int(stmt, idx, tonumber(value) or 0)
        elseif field.sqltype == "REAL" then
            sqlite3.sqlite3_bind_double(stmt, idx, tonumber(value) or 0.0)
        else
            sqlite3.sqlite3_bind_text(stmt, idx, tostring(value or ""), -1, nil)
        end

        idx = idx + 1
    end

    sqlite3.sqlite3_step(stmt)
    sqlite3.sqlite3_finalize(stmt)
end

-- =============================================================================
-- RECGET(handle, recnum, record_table) - Retrieve record
-- =============================================================================
function rec_get(handle, recnum, record)
    if type(recnum) ~= "number" then
        error("RECGET: record number must be a number")
    end
    if type(record) ~= "table" then
        error("RECGET: record must be a table")
    end

    recnum = math.floor(recnum)
    local h = get_handle(handle)

    -- Build SELECT statement
    local fields = {}
    for _, field in ipairs(h.schema.fields) do
        table.insert(fields, field.name)
    end

    local sql = string.format(
        "SELECT %s FROM %s WHERE _recnum = ?",
        table.concat(fields, ", "),
        h.typename
    )

    local stmt = prepare_stmt(h.db, sql)
    sqlite3.sqlite3_bind_int(stmt, 1, recnum)

    if sqlite3.sqlite3_step(stmt) == 100 then -- SQLITE_ROW
        -- Fill record table
        for i, field in ipairs(h.schema.fields) do
            local col_idx = i - 1

            if field.sqltype == "INTEGER" then
                record[field.name] = sqlite3.sqlite3_column_int(stmt, col_idx)
            elseif field.sqltype == "REAL" then
                record[field.name] = sqlite3.sqlite3_column_double(stmt, col_idx)
            else
                local text = sqlite3.sqlite3_column_text(stmt, col_idx)
                record[field.name] = text ~= nil and ffi.string(text) or ""
            end
        end
    else
        error("RECGET: Record " .. recnum .. " not found")
    end

    sqlite3.sqlite3_finalize(stmt)
end

-- =============================================================================
-- RECADD(handle, record_table) - Add with auto-assigned number
-- =============================================================================
function rec_add(handle, record)
    local h = get_handle(handle)

    -- Find next available record number
    local sql = string.format(
        "SELECT IFNULL(MAX(_recnum), 0) + 1 FROM %s",
        h.typename
    )
    local stmt = prepare_stmt(h.db, sql)

    local new_recnum = 1
    if sqlite3.sqlite3_step(stmt) == 100 then
        new_recnum = sqlite3.sqlite3_column_int(stmt, 0)
    end
    sqlite3.sqlite3_finalize(stmt)

    -- Store the record
    rec_put(handle, new_recnum, record)

    return new_recnum
end

-- =============================================================================
-- RECDELETE(handle, recnum) - Delete record
-- =============================================================================
function rec_delete(handle, recnum)
    if type(recnum) ~= "number" then
        error("RECDELETE: record number must be a number")
    end

    recnum = math.floor(recnum)
    local h = get_handle(handle)

    local sql = string.format("DELETE FROM %s WHERE _recnum = ?", h.typename)
    local stmt = prepare_stmt(h.db, sql)
    sqlite3.sqlite3_bind_int(stmt, 1, recnum)
    sqlite3.sqlite3_step(stmt)
    sqlite3.sqlite3_finalize(stmt)
end

-- =============================================================================
-- RECEXISTS(handle, recnum) - Check if record exists
-- =============================================================================
function rec_exists(handle, recnum)
    if type(recnum) ~= "number" then
        return false
    end

    recnum = math.floor(recnum)
    local h = get_handle(handle)

    local sql = string.format("SELECT 1 FROM %s WHERE _recnum = ? LIMIT 1", h.typename)
    local stmt = prepare_stmt(h.db, sql)
    sqlite3.sqlite3_bind_int(stmt, 1, recnum)

    local exists = sqlite3.sqlite3_step(stmt) == 100
    sqlite3.sqlite3_finalize(stmt)

    return exists
end

-- =============================================================================
-- RECCOUNT(handle) - Get total record count
-- =============================================================================
function rec_count(handle)
    local h = get_handle(handle)

    local sql = string.format("SELECT COUNT(*) FROM %s", h.typename)
    local stmt = prepare_stmt(h.db, sql)

    local count = 0
    if sqlite3.sqlite3_step(stmt) == 100 then
        count = sqlite3.sqlite3_column_int(stmt, 0)
    end

    sqlite3.sqlite3_finalize(stmt)
    return count
end

-- =============================================================================
-- RECFIRST(handle) - Get first record number
-- =============================================================================
function rec_first(handle)
    local h = get_handle(handle)

    local sql = string.format("SELECT MIN(_recnum) FROM %s", h.typename)
    local stmt = prepare_stmt(h.db, sql)

    local first = 0
    if sqlite3.sqlite3_step(stmt) == 100 then
        if sqlite3.sqlite3_column_type(stmt, 0) ~= 5 then -- Not NULL
            first = sqlite3.sqlite3_column_int(stmt, 0)
        end
    end

    sqlite3.sqlite3_finalize(stmt)
    return first
end

-- =============================================================================
-- RECLAST(handle) - Get last record number
-- =============================================================================
function rec_last(handle)
    local h = get_handle(handle)

    local sql = string.format("SELECT MAX(_recnum) FROM %s", h.typename)
    local stmt = prepare_stmt(h.db, sql)

    local last = 0
    if sqlite3.sqlite3_step(stmt) == 100 then
        if sqlite3.sqlite3_column_type(stmt, 0) ~= 5 then -- Not NULL
            last = sqlite3.sqlite3_column_int(stmt, 0)
        end
    end

    sqlite3.sqlite3_finalize(stmt)
    return last
end

-- =============================================================================
-- RECFIND(handle, field$, value) - Find first matching record
-- =============================================================================
function rec_find(handle, field, value)
    if type(field) ~= "string" then
        error("RECFIND: field name must be a string")
    end

    local h = get_handle(handle)

    local sql = string.format(
        "SELECT _recnum FROM %s WHERE %s = ? LIMIT 1",
        h.typename, field
    )
    local stmt = prepare_stmt(h.db, sql)

    -- Bind value based on type
    if type(value) == "number" then
        if math.floor(value) == value then
            sqlite3.sqlite3_bind_int(stmt, 1, value)
        else
            sqlite3.sqlite3_bind_double(stmt, 1, value)
        end
    else
        sqlite3.sqlite3_bind_text(stmt, 1, tostring(value), -1, nil)
    end

    local recnum = 0
    if sqlite3.sqlite3_step(stmt) == 100 then
        recnum = sqlite3.sqlite3_column_int(stmt, 0)
    end

    sqlite3.sqlite3_finalize(stmt)
    return recnum
end

-- =============================================================================
-- RECFINDALL(handle, field$, value) - Find all matching records
-- =============================================================================
function rec_findall(handle, field, value)
    if type(field) ~= "string" then
        error("RECFINDALL: field name must be a string")
    end

    local h = get_handle(handle)

    local sql = string.format(
        "SELECT _recnum FROM %s WHERE %s = ? ORDER BY _recnum",
        h.typename, field
    )
    local stmt = prepare_stmt(h.db, sql)

    -- Bind value
    if type(value) == "number" then
        if math.floor(value) == value then
            sqlite3.sqlite3_bind_int(stmt, 1, value)
        else
            sqlite3.sqlite3_bind_double(stmt, 1, value)
        end
    else
        sqlite3.sqlite3_bind_text(stmt, 1, tostring(value), -1, nil)
    end

    -- Collect results
    local results = {}
    while sqlite3.sqlite3_step(stmt) == 100 do
        table.insert(results, sqlite3.sqlite3_column_int(stmt, 0))
    end

    sqlite3.sqlite3_finalize(stmt)

    -- Store for RECRESULT
    search_results = results

    return #results
end

-- =============================================================================
-- RECRESULT(index) - Get record number from search results
-- =============================================================================
function rec_result(index)
    if type(index) ~= "number" then
        error("RECRESULT: index must be a number")
    end

    index = math.floor(index) + 1 -- Convert to 1-based

    if index < 1 or index > #search_results then
        error("RECRESULT: index out of range")
    end

    return search_results[index]
end

-- =============================================================================
-- RECCLEAR(handle) - Delete all records
-- =============================================================================
function rec_clear(handle)
    local h = get_handle(handle)
    local sql = string.format("DELETE FROM %s", h.typename)
    exec_sql(h.db, sql)
end

-- =============================================================================
-- RECCOMMIT(handle) - Force write to disk
-- =============================================================================
function rec_commit(handle)
    local h = get_handle(handle)
    exec_sql(h.db, "PRAGMA wal_checkpoint")
end

-- =============================================================================
-- Cleanup
-- =============================================================================

local function cleanup()
    for handle, h in pairs(handles) do
        sqlite3.sqlite3_close(h.db)
    end
    handles = {}
end

if not _RECORDS_PLUGIN_LOADED then
    _RECORDS_PLUGIN_LOADED = true
    _RECORDS_CLEANUP = cleanup
end
