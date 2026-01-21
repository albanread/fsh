--
-- template_plugin_runtime.lua
-- Template Plugin Runtime for FasterBASIC
--
-- Provides BASIC-friendly template API with global context
--

-- Lazy-loaded module references
local engine = nil
local parser = nil

-- Get engine module (lazy load on first access)
local function get_engine()
    if not engine then
        engine = _G.template_engine or package.loaded['template_engine']
        if not engine then
            error("template_engine module not found - ensure template_engine.lua is loaded first")
        end
    end
    return engine
end

-- Get parser module (lazy load on first access)
local function get_parser()
    if not parser then
        parser = _G.template_parser or package.loaded['template_parser']
        if not parser then
            error("template_parser module not found - ensure template_parser.lua is loaded first")
        end
    end
    return parser
end

local M = {}

-- Get global context (accessed via function to ensure engine is loaded)
local function get_ctx()
    return get_engine().global_context
end

-- =============================================================================
-- TMPL_LOAD - Load template from file
-- =============================================================================
function tmpl_load(filename)
    local ctx = get_ctx()
    if not filename or filename == "" then
        ctx.error_msg = "TMPL_LOAD: filename cannot be empty"
        ctx.error_code = 10
        return
    end

    get_engine().load_template(ctx, filename)
end

-- =============================================================================
-- TMPL_SET - Set scalar variable (default HTML-escaped)
-- =============================================================================
function tmpl_set(name, value)
    local ctx = get_ctx()
    if not name or name == "" then
        ctx.error_msg = "TMPL_SET: variable name cannot be empty"
        ctx.error_code = 11
        return
    end

    get_engine().set_variable(ctx, name, value, "html")
end

-- =============================================================================
-- TMPL_SET_HTML - Set scalar variable with explicit HTML escaping
-- =============================================================================
function tmpl_set_html(name, value)
    local ctx = get_ctx()
    if not name or name == "" then
        ctx.error_msg = "TMPL_SET_HTML: variable name cannot be empty"
        ctx.error_code = 11
        return
    end

    get_engine().set_variable(ctx, name, value, "html")
end

-- =============================================================================
-- TMPL_SET_RAW - Set scalar variable with NO escaping (for trusted content)
-- =============================================================================
function tmpl_set_raw(name, value)
    local ctx = get_ctx()
    if not name or name == "" then
        ctx.error_msg = "TMPL_SET_RAW: variable name cannot be empty"
        ctx.error_code = 11
        return
    end

    get_engine().set_variable(ctx, name, value, "raw")
end

-- =============================================================================
-- TMPL_ARRAY - Register an array variable
-- =============================================================================
function tmpl_array(name, array)
    local ctx = get_ctx()
    if not name or name == "" then
        ctx.error_msg = "TMPL_ARRAY: array name cannot be empty"
        ctx.error_code = 12
        return
    end

    if type(array) ~= "table" then
        ctx.error_msg = "TMPL_ARRAY: second argument must be an array"
        ctx.error_code = 13
        return
    end

    get_engine().set_array(ctx, name, array)
end

-- =============================================================================
-- TMPL_RECORD - Begin record definition
-- =============================================================================
function tmpl_record(name)
    local ctx = get_ctx()
    if not name or name == "" then
        ctx.error_msg = "TMPL_RECORD: record name cannot be empty"
        ctx.error_code = 14
        return
    end

    get_engine().begin_record(ctx, name)
end

-- =============================================================================
-- TMPL_END - End record definition
-- =============================================================================
function tmpl_end()
    get_engine().end_record(get_ctx())
end

-- =============================================================================
-- TMPL_FIELD - Add field to current record (during TMPL_RECORD...TMPL_END)
-- =============================================================================
function tmpl_field(name, value, escape_mode)
    local ctx = get_ctx()
    if not name or name == "" then
        ctx.error_msg = "TMPL_FIELD: field name cannot be empty"
        ctx.error_code = 15
        return
    end

    escape_mode = escape_mode or "html"
    get_engine().add_record_field(ctx, name, value, escape_mode)
end

-- =============================================================================
-- TMPL_RUN - Execute template and return result as string
-- =============================================================================
function tmpl_run_string()
    local ctx = get_ctx()
    local result, err = get_engine().run_template(ctx)

    if not result then
        -- Return empty string on error (error accessible via TMPL_ERROR)
        return ""
    end

    return result
end

-- =============================================================================
-- TMPL_SAVE - Save rendered output to file
-- =============================================================================
function tmpl_save(filename, content)
    local ctx = get_ctx()
    if not filename or filename == "" then
        ctx.error_msg = "TMPL_SAVE: filename cannot be empty"
        ctx.error_code = 16
        return
    end

    if not content then
        ctx.error_msg = "TMPL_SAVE: content cannot be nil"
        ctx.error_code = 17
        return
    end

    get_engine().save_output(ctx, filename, content)
end

-- =============================================================================
-- TMPL_ERROR - Get error code (0 = success)
-- =============================================================================
function tmpl_error()
    return get_ctx().error_code or 0
end

-- =============================================================================
-- TMPL_ERROR$ - Get error message
-- =============================================================================
function tmpl_error_str()
    return get_ctx().error_msg or ""
end

-- =============================================================================
-- TMPL_CLEAR - Clear all context (utility function)
-- =============================================================================
function tmpl_clear()
    get_engine().clear_context(get_ctx())
end

-- =============================================================================
-- TMPL_ESCAPE - Utility to manually HTML-escape a string
-- =============================================================================
function tmpl_escape_string(str)
    return get_engine().html_escape(str)
end

-- =============================================================================
-- TMPL_VERSION - Get plugin version
-- =============================================================================
function tmpl_version_string()
    return "1.0.0"
end

-- Return table for module system (optional, globals are already set)
return {
    tmpl_load = tmpl_load,
    tmpl_set = tmpl_set,
    tmpl_set_html = tmpl_set_html,
    tmpl_set_raw = tmpl_set_raw,
    tmpl_array = tmpl_array,
    tmpl_record = tmpl_record,
    tmpl_field = tmpl_field,
    tmpl_end = tmpl_end,
    tmpl_run_string = tmpl_run_string,
    tmpl_save = tmpl_save,
    tmpl_error = tmpl_error,
    tmpl_error_string = tmpl_error_string,
    tmpl_clear = tmpl_clear,
    tmpl_escape_string = tmpl_escape_string,
    tmpl_version_string = tmpl_version_string
}
