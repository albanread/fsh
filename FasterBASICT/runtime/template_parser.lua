--
-- template_parser.lua
-- Template Parser for FasterBASIC Template Plugin
--
-- Parses templates with {{variable}} substitution and <%directive%> control flow
--

local M = {}

-- Register this module IMMEDIATELY so other modules can require it
package.loaded['template_parser'] = M

-- Token types
local TOKEN_TEXT = "TEXT"
local TOKEN_VAR = "VAR"
local TOKEN_DIRECTIVE = "DIRECTIVE"

-- =============================================================================
-- Tokenizer
-- =============================================================================

-- Tokenize a template into TEXT, VAR ({{}}), and DIRECTIVE (<%%>) tokens
function M.tokenize(template)
    local tokens = {}
    local pos = 1
    local len = #template

    while pos <= len do
        -- Check for variable substitution {{...}}
        local var_start, var_end = template:find("{{", pos, true)
        if var_start then
            -- Add any text before the variable
            if var_start > pos then
                table.insert(tokens, {
                    type = TOKEN_TEXT,
                    value = template:sub(pos, var_start - 1)
                })
            end

            -- Find the closing }}
            local close_start, close_end = template:find("}}", var_end + 1, true)
            if close_start then
                local var_name = template:sub(var_end + 1, close_start - 1)
                -- Trim whitespace
                var_name = var_name:match("^%s*(.-)%s*$")
                table.insert(tokens, {
                    type = TOKEN_VAR,
                    value = var_name
                })
                pos = close_end + 1
            else
                -- No closing }}, treat as text
                table.insert(tokens, {
                    type = TOKEN_TEXT,
                    value = template:sub(var_start, var_start + 1)
                })
                pos = var_end + 1
            end
            -- Check for directive <%...%>
        elseif template:sub(pos, pos + 1) == "<%" then
            -- Find the closing %>
            local close_start, close_end = template:find("%>", pos + 2, true)
            if close_start then
                local directive = template:sub(pos + 2, close_start - 1)
                -- Trim whitespace
                directive = directive:match("^%s*(.-)%s*$")
                table.insert(tokens, {
                    type = TOKEN_DIRECTIVE,
                    value = directive,
                    line = M.count_lines(template, pos)
                })
                pos = close_end + 1
            else
                -- No closing %>, treat as text
                table.insert(tokens, {
                    type = TOKEN_TEXT,
                    value = "<%"
                })
                pos = pos + 2
            end
        else
            -- Regular text - find next special sequence or end
            local next_var = template:find("{{", pos, true)
            local next_dir = template:find("<%", pos, true)
            local next_pos = len + 1

            if next_var and (not next_dir or next_var < next_dir) then
                next_pos = next_var
            elseif next_dir then
                next_pos = next_dir
            end

            table.insert(tokens, {
                type = TOKEN_TEXT,
                value = template:sub(pos, next_pos - 1)
            })
            pos = next_pos
        end
    end

    return tokens
end

-- Count lines up to position (for error reporting)
function M.count_lines(str, pos)
    local line = 1
    for i = 1, pos do
        if str:sub(i, i) == "\n" then
            line = line + 1
        end
    end
    return line
end

-- =============================================================================
-- Parser - Build AST from tokens
-- =============================================================================

-- Parse tokens into an Abstract Syntax Tree
function M.parse(tokens)
    local ast = {
        type = "ROOT",
        children = {}
    }

    local pos = 1
    local stack = { ast }

    while pos <= #tokens do
        local token = tokens[pos]
        local current = stack[#stack]

        if token.type == TOKEN_TEXT then
            -- Add text node
            if #token.value > 0 then
                table.insert(current.children, {
                    type = "TEXT",
                    value = token.value
                })
            end
            pos = pos + 1
        elseif token.type == TOKEN_VAR then
            -- Add variable node
            table.insert(current.children, {
                type = "VAR",
                name = token.value
            })
            pos = pos + 1
        elseif token.type == TOKEN_DIRECTIVE then
            local directive = token.value:upper()

            -- Parse IF directive
            if directive:match("^IF%s+") then
                local condition = directive:match("^IF%s+(.+)$")
                local if_node = {
                    type = "IF",
                    condition = condition,
                    then_block = {},
                    else_block = nil,
                    line = token.line
                }
                table.insert(current.children, if_node)
                table.insert(stack, if_node.then_block)
                pos = pos + 1

                -- Parse ELSE directive
            elseif directive == "ELSE" then
                -- Pop current block (should be then_block)
                table.remove(stack)
                -- Current should now be the IF node's parent
                local parent = stack[#stack]
                if #parent.children > 0 and parent.children[#parent.children].type == "IF" then
                    local if_node = parent.children[#parent.children]
                    if_node.else_block = {}
                    table.insert(stack, if_node.else_block)
                else
                    return nil, "ELSE without matching IF at line " .. (token.line or "?")
                end
                pos = pos + 1

                -- Parse END IF directive
            elseif directive == "END IF" or directive == "ENDIF" then
                -- Pop current block
                table.remove(stack)
                -- If we were in else_block, we're done; if in then_block without else, also done
                if #stack == 0 then
                    return nil, "END IF without matching IF at line " .. (token.line or "?")
                end
                pos = pos + 1

                -- Parse FOR EACH directive
            elseif directive:match("^FOR%s+EACH%s+") then
                local var_name, array_name = directive:match("^FOR%s+EACH%s+(%w+)%s+IN%s+(%w+)$")
                if not var_name then
                    return nil, "Invalid FOR EACH syntax at line " .. (token.line or "?") ..
                        " (expected: FOR EACH item IN array)"
                end
                local for_node = {
                    type = "FOR_EACH",
                    var = var_name,
                    array = array_name,
                    body = {},
                    line = token.line
                }
                table.insert(current.children, for_node)
                table.insert(stack, for_node.body)
                pos = pos + 1

                -- Parse numeric FOR directive
            elseif directive:match("^FOR%s+") and not directive:match("^FOR%s+EACH%s+") then
                local var_name, start_val, end_val = directive:match("^FOR%s+(%w+)%s*=%s*(%S+)%s+TO%s+(%S+)$")
                if not var_name then
                    return nil, "Invalid FOR syntax at line " .. (token.line or "?") ..
                        " (expected: FOR i = start TO end)"
                end
                local for_node = {
                    type = "FOR",
                    var = var_name,
                    start = start_val,
                    finish = end_val,
                    body = {},
                    line = token.line
                }
                table.insert(current.children, for_node)
                table.insert(stack, for_node.body)
                pos = pos + 1

                -- Parse NEXT directive
            elseif directive:match("^NEXT") then
                -- Pop the FOR/FOR EACH body
                table.remove(stack)
                if #stack == 0 then
                    return nil, "NEXT without matching FOR at line " .. (token.line or "?")
                end
                pos = pos + 1

                -- Parse REM (comment) directive
            elseif directive:match("^REM%s+") or directive == "REM" then
                -- Ignore comments
                pos = pos + 1

                -- Parse INCLUDE directive (optional - placeholder for now)
            elseif directive:match("^INCLUDE%s+") then
                local filename = directive:match("^INCLUDE%s+(.+)$")
                -- Remove quotes if present
                filename = filename:match('^"(.-)"$') or filename:match("^'(.-)'$") or filename
                table.insert(current.children, {
                    type = "INCLUDE",
                    filename = filename,
                    line = token.line
                })
                pos = pos + 1
            else
                return nil, "Unknown directive '" .. token.value .. "' at line " .. (token.line or "?")
            end
        else
            pos = pos + 1
        end
    end

    -- Check for unclosed blocks
    if #stack > 1 then
        return nil, "Unclosed block (missing END IF or NEXT)"
    end

    return ast
end

-- =============================================================================
-- Utility Functions
-- =============================================================================

-- Debug: dump tokens
function M.dump_tokens(tokens)
    for i, token in ipairs(tokens) do
        print(string.format("[%d] %s: %q", i, token.type, token.value or ""))
    end
end

-- Debug: dump AST
function M.dump_ast(node, indent)
    indent = indent or 0
    local prefix = string.rep("  ", indent)

    if node.type == "ROOT" then
        print(prefix .. "ROOT")
        for _, child in ipairs(node.children) do
            M.dump_ast(child, indent + 1)
        end
    elseif node.type == "TEXT" then
        print(prefix .. "TEXT: " .. node.value:gsub("\n", "\\n"))
    elseif node.type == "VAR" then
        print(prefix .. "VAR: " .. node.name)
    elseif node.type == "IF" then
        print(prefix .. "IF: " .. node.condition)
        print(prefix .. "  THEN:")
        for _, child in ipairs(node.then_block) do
            M.dump_ast(child, indent + 2)
        end
        if node.else_block then
            print(prefix .. "  ELSE:")
            for _, child in ipairs(node.else_block) do
                M.dump_ast(child, indent + 2)
            end
        end
    elseif node.type == "FOR_EACH" then
        print(prefix .. "FOR EACH " .. node.var .. " IN " .. node.array)
        for _, child in ipairs(node.body) do
            M.dump_ast(child, indent + 1)
        end
    elseif node.type == "FOR" then
        print(prefix .. "FOR " .. node.var .. " = " .. node.start .. " TO " .. node.finish)
        for _, child in ipairs(node.body) do
            M.dump_ast(child, indent + 1)
        end
    elseif node.type == "INCLUDE" then
        print(prefix .. "INCLUDE: " .. node.filename)
    end
end

-- Make module available as global for other runtime files
_G.template_parser = M

return M
