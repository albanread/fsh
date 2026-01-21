--
-- environment_plugin_runtime.lua
-- Environment Plugin Runtime for FasterBASIC
--
-- Provides functions to get and set environment variables using Lua's os.getenv and os.setenv
--

local M = {}

-- Cache for environment variables (used for enumeration)
local env_cache = nil

-- Helper function to build environment cache
local function build_env_cache()
    if env_cache then
        return env_cache
    end

    env_cache = {}

    -- On Unix-like systems, we can access _ENV or use shell commands
    -- For portability, we'll use a limited approach
    -- Note: Lua doesn't provide direct access to enumerate all env vars
    -- So we maintain a cache of variables we've set or accessed

    -- Try to get some common environment variables to populate cache
    local common_vars = {
        "PATH", "HOME", "USER", "SHELL", "TERM", "LANG", "LC_ALL",
        "PWD", "OLDPWD", "HOSTNAME", "LOGNAME", "TMPDIR", "TMP", "TEMP",
        "EDITOR", "VISUAL", "PAGER", "DISPLAY", "MANPATH", "CLASSPATH",
        "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "PYTHONPATH",
        "JAVA_HOME", "GOPATH", "CARGO_HOME", "RUSTUP_HOME",
        "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME",
        "USERPROFILE", "APPDATA", "LOCALAPPDATA", "PROGRAMFILES",
        "SYSTEMROOT", "WINDIR", "COMSPEC", "OS", "PROCESSOR_ARCHITECTURE"
    }

    for _, name in ipairs(common_vars) do
        local value = os.getenv(name)
        if value then
            table.insert(env_cache, { name = name, value = value })
        end
    end

    return env_cache
end

-- Clear the cache (called when environment is modified)
local function clear_env_cache()
    env_cache = nil
end

-- Track custom set variables
local custom_vars = {}

-- ============================================================================
-- env_get(name) - Get environment variable value
-- ============================================================================
function env_get(name)
    if type(name) ~= "string" then
        error("GETENV$: Variable name must be a string")
    end

    -- First check custom variables (for variables set by SETENV)
    if custom_vars[name] then
        return custom_vars[name]
    end

    -- Then check system environment
    local value = os.getenv(name)

    -- Return empty string if not found (consistent with BASIC conventions)
    return value or ""
end

-- ============================================================================
-- env_set(name, value) - Set environment variable
-- ============================================================================
function env_set(name, value)
    if type(name) ~= "string" then
        error("SETENV: Variable name must be a string")
    end

    if type(value) ~= "string" then
        value = tostring(value)
    end

    -- Store in custom variables
    custom_vars[name] = value

    -- Clear cache since environment changed
    clear_env_cache()

    -- Note: Lua's os.setenv is not standard and may not be available
    -- We simulate it by tracking in custom_vars
    -- For actual process environment modification, we'd need FFI or C extension
end

-- ============================================================================
-- env_unset(name) - Unset/remove environment variable
-- ============================================================================
function env_unset(name)
    if type(name) ~= "string" then
        error("UNSETENV: Variable name must be a string")
    end

    -- Remove from custom variables
    custom_vars[name] = nil

    -- Clear cache since environment changed
    clear_env_cache()

    -- Note: We can't actually remove from system environment in pure Lua
end

-- ============================================================================
-- env_exists(name) - Check if environment variable exists
-- ============================================================================
function env_exists(name)
    if type(name) ~= "string" then
        error("ENVEXISTS: Variable name must be a string")
    end

    -- Check custom variables first
    if custom_vars[name] then
        return 1
    end

    -- Check system environment
    local value = os.getenv(name)
    return (value ~= nil) and 1 or 0
end

-- ============================================================================
-- env_list() - List all environment variables
-- ============================================================================
function env_list()
    print("Environment Variables:")
    print("=====================")

    -- Build or refresh cache
    local cache = build_env_cache()

    -- First, print system environment variables
    for _, entry in ipairs(cache) do
        print(entry.name .. " = " .. entry.value)
    end

    -- Then print custom variables
    for name, value in pairs(custom_vars) do
        -- Check if it's not already in the cache
        local found = false
        for _, entry in ipairs(cache) do
            if entry.name == name then
                found = true
                break
            end
        end
        if not found then
            print(name .. " = " .. value .. " (custom)")
        end
    end

    print("=====================")
end

-- ============================================================================
-- env_count() - Get count of environment variables
-- ============================================================================
function env_count()
    local cache = build_env_cache()
    local count = #cache

    -- Add custom variables that aren't in cache
    for name, _ in pairs(custom_vars) do
        local found = false
        for _, entry in ipairs(cache) do
            if entry.name == name then
                found = true
                break
            end
        end
        if not found then
            count = count + 1
        end
    end

    return count
end

-- ============================================================================
-- env_name(index) - Get environment variable name by index (1-based)
-- ============================================================================
function env_name(index)
    if type(index) ~= "number" then
        error("ENVNAME$: Index must be a number")
    end

    local cache = build_env_cache()

    -- Create a combined list of all variables
    local all_vars = {}

    -- Add cached system variables
    for _, entry in ipairs(cache) do
        table.insert(all_vars, entry.name)
    end

    -- Add custom variables not in cache
    for name, _ in pairs(custom_vars) do
        local found = false
        for _, entry in ipairs(cache) do
            if entry.name == name then
                found = true
                break
            end
        end
        if not found then
            table.insert(all_vars, name)
        end
    end

    -- Check bounds
    if index < 1 or index > #all_vars then
        return "" -- Return empty string for out of bounds
    end

    return all_vars[index]
end

-- Return module for potential require() usage
return M
