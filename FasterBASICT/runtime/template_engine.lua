--
-- template_engine.lua
-- Template Execution Engine for FasterBASIC Template Plugin
--
-- Executes parsed template AST against a context of variables, arrays, and records
--

local M = {}

-- Register this module IMMEDIATELY so other modules can require it
package.loaded['template_engine'] = M

-- Lazy-loaded parser module reference
local parser = nil

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

-- =============================================================================
-- Context Management
-- =============================================================================

-- Create a new template context
function M.create_context()
    return {
        vars = {},            -- Scalar variables
        arrays = {},          -- Array variables
        records = {},         -- Record definitions
        current_record = nil, -- Currently being defined record
        error_msg = nil,
        error_code = 0
    }
end

-- Global context (for Option 2 API)
M.global_context = M.create_context()

-- Set a scalar variable with HTML escaping (default safe)
function M.set_variable(ctx, name, value, escape_mode)
    escape_mode = escape_mode or "html"

    if escape_mode == "html" then
        value = M.html_escape(tostring(value))
    elseif escape_mode == "raw" then
        value = tostring(value)
    end

    ctx.vars[name] = value
end

-- Register an array
function M.set_array(ctx, name, array)
    ctx.arrays[name] = array
end

-- Begin record definition
function M.begin_record(ctx, name)
    ctx.current_record = {
        name = name,
        fields = {}
    }
end

-- End record definition
function M.end_record(ctx)
    if not ctx.current_record then
        ctx.error_msg = "TMPL_END called without TMPL_RECORD"
        ctx.error_code = 1
        return false
    end

    ctx.records[ctx.current_record.name] = ctx.current_record.fields
    ctx.current_record = nil
    return true
end

-- Add field to current record
function M.add_record_field(ctx, name, value, escape_mode)
    if not ctx.current_record then
        ctx.error_msg = "Cannot set field without TMPL_RECORD"
        ctx.error_code = 1
        return false
    end

    escape_mode = escape_mode or "html"

    if escape_mode == "html" then
        value = M.html_escape(tostring(value))
    elseif escape_mode == "raw" then
        value = tostring(value)
    end

    ctx.current_record.fields[name] = value
    return true
end

-- Clear context
function M.clear_context(ctx)
    ctx.vars = {}
    ctx.arrays = {}
    ctx.records = {}
    ctx.current_record = nil
    ctx.error_msg = nil
    ctx.error_code = 0
end

-- =============================================================================
-- HTML Escaping
-- =============================================================================

function M.html_escape(str)
    if not str then return "" end
    str = tostring(str)
    str = str:gsub("&", "&amp;")
    str = str:gsub("<", "&lt;")
    str = str:gsub(">", "&gt;")
    str = str:gsub('"', "&quot;")
    str = str:gsub("'", "&#39;")
    return str
end

-- =============================================================================
-- Variable Resolution
-- =============================================================================

-- Resolve a variable name with dot notation (e.g., "user.name")
function M.resolve_variable(ctx, name)
    -- Check for dot notation (record field access)
    if name:find(".", 1, true) then
        local parts = {}
        for part in name:gmatch("[^.]+") do
            table.insert(parts, part)
        end

        -- First part should be a record name
        local record_name = parts[1]
        local record = ctx.records[record_name]

        if not record then
            -- Maybe it's a variable?
            return ctx.vars[name] or ""
        end

        -- Navigate through fields
        local value = record
        for i = 2, #parts do
            local field = parts[i]
            if type(value) == "table" then
                value = value[field]
            else
                return "" -- Can't navigate further
            end
            if not value then
                return "" -- Missing field, safe navigation
            end
        end

        return tostring(value or "")
    else
        -- Simple variable lookup
        return tostring(ctx.vars[name] or "")
    end
end

-- =============================================================================
-- Condition Evaluation
-- =============================================================================

-- Evaluate a condition (simple implementation)
function M.evaluate_condition(ctx, condition)
    -- Strip whitespace
    condition = condition:match("^%s*(.-)%s*$")

    -- Simple variable existence check
    local value = M.resolve_variable(ctx, condition)

    -- Check for comparisons
    if condition:match("==") or condition:match("!=") or condition:match("<=") or
        condition:match(">=") or condition:match("<") or condition:match(">") then
        -- Parse comparison
        local left, op, right = condition:match("^(.-)%s*([=!<>]+)%s*(.-)$")

        if left and op and right then
            -- Resolve both sides
            local left_val = M.resolve_variable(ctx, left)
            local right_val = M.resolve_variable(ctx, right)

            -- Try to remove quotes from right side if it's a literal
            right_val = right_val:match('^"(.-)"$') or right_val:match("^'(.-)'$") or right_val

            -- Perform comparison
            if op == "==" or op == "=" then
                return left_val == right_val
            elseif op == "!=" or op == "<>" then
                return left_val ~= right_val
            elseif op == "<" then
                return tonumber(left_val) and tonumber(right_val) and tonumber(left_val) < tonumber(right_val)
            elseif op == ">" then
                return tonumber(left_val) and tonumber(right_val) and tonumber(left_val) > tonumber(right_val)
            elseif op == "<=" then
                return tonumber(left_val) and tonumber(right_val) and tonumber(left_val) <= tonumber(right_val)
            elseif op == ">=" then
                return tonumber(left_val) and tonumber(right_val) and tonumber(left_val) >= tonumber(right_val)
            end
        end
    end

    -- Default: check if variable exists and is truthy
    if value == "" or value == "0" or value == "false" or value == "nil" then
        return false
    end

    return true
end

-- =============================================================================
-- Template Execution
-- =============================================================================

-- Execute a template AST node
function M.execute_node(ctx, node, output)
    if node.type == "ROOT" then
        for _, child in ipairs(node.children) do
            M.execute_node(ctx, child, output)
        end
    elseif node.type == "TEXT" then
        table.insert(output, node.value)
    elseif node.type == "VAR" then
        local value = M.resolve_variable(ctx, node.name)
        table.insert(output, value)
    elseif node.type == "IF" then
        local condition_result = M.evaluate_condition(ctx, node.condition)

        if condition_result then
            for _, child in ipairs(node.then_block) do
                M.execute_node(ctx, child, output)
            end
        elseif node.else_block then
            for _, child in ipairs(node.else_block) do
                M.execute_node(ctx, child, output)
            end
        end
    elseif node.type == "FOR_EACH" then
        local array = ctx.arrays[node.array]

        if array then
            -- Iterate over array elements
            for i = 1, #array do
                -- Set loop variable in context (temporarily)
                local old_value = ctx.vars[node.var]
                ctx.vars[node.var] = tostring(array[i])

                -- Execute loop body
                for _, child in ipairs(node.body) do
                    M.execute_node(ctx, child, output)
                end

                -- Restore old value
                if old_value then
                    ctx.vars[node.var] = old_value
                else
                    ctx.vars[node.var] = nil
                end
            end
        end
    elseif node.type == "FOR" then
        -- Numeric FOR loop
        local start_val = tonumber(M.resolve_variable(ctx, node.start)) or tonumber(node.start) or 1
        local end_val = tonumber(M.resolve_variable(ctx, node.finish)) or tonumber(node.finish) or 10

        for i = start_val, end_val do
            -- Set loop variable
            local old_value = ctx.vars[node.var]
            ctx.vars[node.var] = tostring(i)

            -- Execute loop body
            for _, child in ipairs(node.body) do
                M.execute_node(ctx, child, output)
            end

            -- Restore old value
            if old_value then
                ctx.vars[node.var] = old_value
            else
                ctx.vars[node.var] = nil
            end
        end
    elseif node.type == "INCLUDE" then
        -- INCLUDE support (load and execute another template)
        local success, included_content = pcall(function()
            local file = io.open(node.filename, "r")
            if not file then
                return "<!-- Error: Could not open " .. node.filename .. " -->"
            end
            local content = file:read("*all")
            file:close()

            -- Parse and execute the included template
            local p = get_parser()
            local tokens = p.tokenize(content)
            local ast, err = p.parse(tokens)
            if not ast then
                return "<!-- Error parsing " .. node.filename .. ": " .. err .. " -->"
            end

            local inc_output = {}
            M.execute_node(ctx, ast, inc_output)
            return table.concat(inc_output)
        end)

        if success then
            table.insert(output, included_content)
        else
            table.insert(output, "<!-- Error including " .. node.filename .. " -->")
        end
    end
end

-- Execute a template string against a context
function M.execute(ctx, template_str)
    -- Parse template
    local p = get_parser()
    local tokens = p.tokenize(template_str)
    local ast, err = p.parse(tokens)

    if not ast then
        ctx.error_msg = err
        ctx.error_code = 2
        return nil, err
    end

    -- Execute AST
    local output = {}
    local success, exec_err = pcall(function()
        M.execute_node(ctx, ast, output)
    end)

    if not success then
        ctx.error_msg = "Runtime error: " .. tostring(exec_err)
        ctx.error_code = 3
        return nil, ctx.error_msg
    end

    ctx.error_code = 0
    ctx.error_msg = nil

    return table.concat(output)
end

-- =============================================================================
-- Template Loading and Saving
-- =============================================================================

-- Currently loaded template string
M.loaded_template = nil

-- Load a template from file
function M.load_template(ctx, filename)
    local file, err = io.open(filename, "r")
    if not file then
        ctx.error_msg = "Could not open template file: " .. filename
        ctx.error_code = 4
        return false
    end

    M.loaded_template = file:read("*all")
    file:close()

    ctx.error_code = 0
    ctx.error_msg = nil
    return true
end

-- Execute the loaded template
function M.run_template(ctx)
    if not M.loaded_template then
        ctx.error_msg = "No template loaded"
        ctx.error_code = 5
        return nil
    end

    return M.execute(ctx, M.loaded_template)
end

-- Save rendered output to file
function M.save_output(ctx, filename, content)
    local file, err = io.open(filename, "w")
    if not file then
        ctx.error_msg = "Could not write to file: " .. filename
        ctx.error_code = 6
        return false
    end

    file:write(content)
    file:close()

    ctx.error_code = 0
    ctx.error_msg = nil
    return true
end

-- Make module available as global for other runtime files
_G.template_engine = M

return M
