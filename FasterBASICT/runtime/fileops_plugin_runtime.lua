--
-- fileops_plugin_runtime.lua
-- Runtime implementation for File Operations Plugin
--
-- Provides file system operations including directory listing, file manipulation,
-- path operations, and pattern matching.
--

-- =============================================================================
-- Module Setup
-- =============================================================================

local fileops = {}
fileops.last_error = 0
fileops.last_error_msg = ""

-- Platform detection
local path_sep = package.config:sub(1, 1) -- "/" on Unix, "\" on Windows
local is_windows = path_sep == "\\"

-- =============================================================================
-- Error Handling
-- =============================================================================

local function set_error(code, message)
    fileops.last_error = code
    fileops.last_error_msg = message or ""
end

local function clear_error()
    fileops.last_error = 0
    fileops.last_error_msg = ""
end

function fileops_geterror()
    return fileops.last_error
end

function fileops_geterrormsg()
    return fileops.last_error_msg
end

-- =============================================================================
-- Platform-Specific Commands
-- =============================================================================

local function execute_command(cmd)
    local handle = io.popen(cmd)
    if not handle then
        return nil, "Failed to execute command"
    end
    local result = handle:read("*a")
    local success = handle:close()
    return result, success
end

-- =============================================================================
-- Directory Operations
-- =============================================================================

function fileops_direxists(path)
    clear_error()

    -- Try to change to directory and back (portable way)
    local current_dir = fileops_workdir()
    local success = fileops_changedir(path)

    if success then
        fileops_changedir(current_dir)
        return true
    end

    return false
end

function fileops_dircreate(path)
    clear_error()

    local cmd
    if is_windows then
        cmd = 'mkdir "' .. path:gsub('/', '\\') .. '" 2>nul'
    else
        cmd = 'mkdir -p "' .. path .. '" 2>/dev/null'
    end

    local result, success = execute_command(cmd)

    if not success then
        set_error(1, "Failed to create directory: " .. path)
        return false
    end

    return true
end

function fileops_dirdelete(path)
    clear_error()

    local cmd
    if is_windows then
        cmd = 'rmdir "' .. path:gsub('/', '\\') .. '" 2>nul'
    else
        cmd = 'rmdir "' .. path .. '" 2>/dev/null'
    end

    local result, success = execute_command(cmd)

    if not success then
        set_error(1, "Failed to delete directory: " .. path)
        return false
    end

    return true
end

function fileops_dirlist(path, pattern)
    clear_error()
    pattern = pattern or "*"

    local items = {}
    local cmd

    if is_windows then
        cmd = 'dir /b "' .. path:gsub('/', '\\') .. '" 2>nul'
    else
        cmd = 'ls -1 "' .. path .. '" 2>/dev/null'
    end

    local handle = io.popen(cmd)
    if not handle then
        set_error(1, "Failed to list directory: " .. path)
        return "[]"
    end

    for line in handle:lines() do
        -- Filter by pattern if specified
        if pattern == "*" or fileops_patternmatch(line, pattern) then
            table.insert(items, line)
        end
    end

    handle:close()

    -- Return as JSON array
    local json = "["
    for i, item in ipairs(items) do
        if i > 1 then json = json .. "," end
        json = json .. '"' .. item:gsub('"', '\\"') .. '"'
    end
    json = json .. "]"

    return json
end

function fileops_dirlistcount(path, pattern)
    clear_error()
    pattern = pattern or "*"

    local count = 0
    local cmd

    if is_windows then
        cmd = 'dir /b "' .. path:gsub('/', '\\') .. '" 2>nul'
    else
        cmd = 'ls -1 "' .. path .. '" 2>/dev/null'
    end

    local handle = io.popen(cmd)
    if not handle then
        set_error(1, "Failed to list directory: " .. path)
        return 0
    end

    for line in handle:lines() do
        if pattern == "*" or fileops_patternmatch(line, pattern) then
            count = count + 1
        end
    end

    handle:close()
    return count
end

function fileops_dirlistitem(path, index, pattern)
    clear_error()
    pattern = pattern or "*"

    local current = 0
    local cmd

    if is_windows then
        cmd = 'dir /b "' .. path:gsub('/', '\\') .. '" 2>nul'
    else
        cmd = 'ls -1 "' .. path .. '" 2>/dev/null'
    end

    local handle = io.popen(cmd)
    if not handle then
        set_error(1, "Failed to list directory: " .. path)
        return ""
    end

    for line in handle:lines() do
        if pattern == "*" or fileops_patternmatch(line, pattern) then
            if current == index then
                handle:close()
                return line
            end
            current = current + 1
        end
    end

    handle:close()
    set_error(2, "Index out of range: " .. index)
    return ""
end

-- =============================================================================
-- File Operations
-- =============================================================================

function fileops_fileexists(path)
    clear_error()
    local file = io.open(path, "r")
    if file then
        file:close()
        return true
    end
    return false
end

function fileops_filecopy(source, dest)
    clear_error()

    -- Read source file
    local src_file = io.open(source, "rb")
    if not src_file then
        set_error(1, "Cannot open source file: " .. source)
        return false
    end

    local content = src_file:read("*a")
    src_file:close()

    -- Write to destination
    local dest_file = io.open(dest, "wb")
    if not dest_file then
        set_error(2, "Cannot create destination file: " .. dest)
        return false
    end

    dest_file:write(content)
    dest_file:close()

    return true
end

function fileops_filemove(source, dest)
    clear_error()

    -- Try os.rename first (fastest)
    local success = os.rename(source, dest)

    if success then
        return true
    end

    -- Fall back to copy + delete
    if fileops_filecopy(source, dest) then
        return fileops_filedelete(source)
    end

    set_error(1, "Failed to move file: " .. source .. " to " .. dest)
    return false
end

function fileops_filedelete(path)
    clear_error()

    local success = os.remove(path)

    if not success then
        set_error(1, "Failed to delete file: " .. path)
        return false
    end

    return true
end

function fileops_filetouch(path)
    clear_error()

    -- Check if file exists
    local file = io.open(path, "r")
    if file then
        -- File exists, close and update modification time
        file:close()

        -- Use platform-specific touch command to update timestamp
        local cmd
        if is_windows then
            -- Windows: copy /b file+,, updates timestamp
            cmd = 'copy /b "' .. path:gsub('/', '\\') .. '"+,, 2>nul'
        else
            -- Unix: touch command
            cmd = 'touch "' .. path .. '" 2>/dev/null'
        end

        local result, success = execute_command(cmd)
        if not success then
            set_error(1, "Failed to update file timestamp: " .. path)
            return false
        end
        return true
    else
        -- File doesn't exist, create empty file
        file = io.open(path, "w")
        if not file then
            set_error(1, "Cannot create file: " .. path)
            return false
        end
        file:close()
        return true
    end
end

function fileops_filesize(path)
    clear_error()

    local file = io.open(path, "rb")
    if not file then
        set_error(1, "Cannot open file: " .. path)
        return -1
    end

    local size = file:seek("end")
    file:close()

    return size or -1
end

function fileops_filemodtime(path)
    clear_error()

    local cmd
    if is_windows then
        -- Windows: use forfiles or stat-like commands
        -- This is a simplified version
        cmd = 'forfiles /m "' .. path:gsub('/', '\\') .. '" /c "cmd /c echo @fdate @ftime" 2>nul'
    else
        -- Unix: use stat command
        cmd = 'stat -f %m "' .. path .. '" 2>/dev/null || stat -c %Y "' .. path .. '" 2>/dev/null'
    end

    local handle = io.popen(cmd)
    if not handle then
        set_error(1, "Cannot get modification time: " .. path)
        return -1
    end

    local result = handle:read("*l")
    handle:close()

    if not result or result == "" then
        set_error(1, "Cannot get modification time: " .. path)
        return -1
    end

    -- For Unix, we get epoch time directly
    if not is_windows then
        return tonumber(result) or -1
    end

    -- For Windows, we'd need more complex parsing
    set_error(1, "Modification time not fully supported on Windows")
    return -1
end

function fileops_fileisdir(path)
    clear_error()
    return fileops_direxists(path)
end

function fileops_fileisfile(path)
    clear_error()
    return fileops_fileexists(path) and not fileops_direxists(path)
end

-- =============================================================================
-- Path Manipulation
-- =============================================================================

function fileops_pathbasename(path)
    clear_error()

    -- Handle both / and \ separators
    local name = path:match("([^/\\]+)$")
    return name or path
end

function fileops_pathdirname(path)
    clear_error()

    -- Handle both / and \ separators
    local dir = path:match("^(.+)[/\\][^/\\]+$")
    return dir or "."
end

function fileops_pathjoin(part1, part2)
    clear_error()

    -- Remove trailing separator from part1
    part1 = part1:gsub("[/\\]+$", "")

    -- Remove leading separator from part2
    part2 = part2:gsub("^[/\\]+", "")

    return part1 .. path_sep .. part2
end

function fileops_pathnormalize(path)
    clear_error()

    -- Convert to platform separator
    local normalized = path:gsub("[/\\]+", path_sep)

    -- Remove duplicate separators
    normalized = normalized:gsub(path_sep .. "+", path_sep)

    -- Handle . and .. (simplified)
    local parts = {}
    for part in normalized:gmatch("[^" .. path_sep .. "]+") do
        if part == ".." then
            if #parts > 0 and parts[#parts] ~= ".." then
                table.remove(parts)
            else
                table.insert(parts, part)
            end
        elseif part ~= "." then
            table.insert(parts, part)
        end
    end

    local result = table.concat(parts, path_sep)

    -- Preserve leading separator for absolute paths
    if normalized:sub(1, 1) == path_sep then
        result = path_sep .. result
    end

    return result == "" and "." or result
end

function fileops_pathext(path)
    clear_error()

    local basename = fileops_pathbasename(path)
    local ext = basename:match("(%.[^.]+)$")
    return ext or ""
end

function fileops_pathwithoutext(path)
    clear_error()

    local without_ext = path:match("^(.+)%.[^.]+$")
    return without_ext or path
end

function fileops_pathabsolute(path)
    clear_error()

    -- If already absolute, return as-is
    if path:sub(1, 1) == "/" or path:match("^%a:") then
        return path
    end

    -- Otherwise, prepend current directory
    local current = fileops_workdir()
    return fileops_pathjoin(current, path)
end

function fileops_pathseparator()
    clear_error()
    return path_sep
end

-- =============================================================================
-- Pattern Matching (Glob)
-- =============================================================================

function fileops_patternmatch(filename, pattern)
    clear_error()

    -- Convert glob pattern to Lua pattern
    local lua_pattern = "^" .. pattern:gsub("[%(%)%.%%%+%-%[%]%^%$]", "%%%1")
    lua_pattern = lua_pattern:gsub("%*", ".*")
    lua_pattern = lua_pattern:gsub("%?", ".")
    lua_pattern = lua_pattern .. "$"

    return filename:match(lua_pattern) ~= nil
end

-- =============================================================================
-- Utilities
-- =============================================================================

function fileops_tempfile(prefix, extension)
    clear_error()
    prefix = prefix or "tmp"
    extension = extension or ".tmp"

    -- Generate random filename
    math.randomseed(os.time() * 1000 + os.clock() * 1000000)
    local random_part = string.format("%08x", math.random(0, 0xFFFFFFFF))

    local temp_dir
    if is_windows then
        temp_dir = os.getenv("TEMP") or os.getenv("TMP") or "C:\\Temp"
    else
        temp_dir = os.getenv("TMPDIR") or "/tmp"
    end

    local temp_path = fileops_pathjoin(temp_dir, prefix .. "_" .. random_part .. extension)

    -- Create the file
    local file = io.open(temp_path, "w")
    if not file then
        set_error(1, "Cannot create temporary file")
        return ""
    end

    file:close()
    return temp_path
end

function fileops_workdir()
    clear_error()

    local cmd
    if is_windows then
        cmd = "cd"
    else
        cmd = "pwd"
    end

    local handle = io.popen(cmd)
    if not handle then
        set_error(1, "Cannot get current directory")
        return ""
    end

    local result = handle:read("*l")
    handle:close()

    return result or ""
end

function fileops_changedir(path)
    clear_error()

    local cmd
    if is_windows then
        cmd = 'cd /d "' .. path:gsub('/', '\\') .. '" 2>nul && cd'
    else
        cmd = 'cd "' .. path .. '" 2>/dev/null && pwd'
    end

    local handle = io.popen(cmd)
    if not handle then
        set_error(1, "Cannot change directory: " .. path)
        return false
    end

    local result = handle:read("*l")
    handle:close()

    if not result or result == "" then
        set_error(1, "Cannot change directory: " .. path)
        return false
    end

    -- Actually change the Lua process directory
    -- Note: This doesn't actually change the process directory in Lua 5.1
    -- It only verifies the directory exists
    return true
end

-- =============================================================================
-- FasterBASIC String Function Wrappers
-- =============================================================================
-- FasterBASIC expects string-returning functions to have _STRING suffix

function DIRLIST_STRING(path, pattern)
    return fileops_dirlist(path, pattern)
end

function DIRLISTITEM_STRING(path, index, pattern)
    return fileops_dirlistitem(path, index, pattern)
end

function PATHBASENAME_STRING(path)
    return fileops_pathbasename(path)
end

function PATHDIRNAME_STRING(path)
    return fileops_pathdirname(path)
end

function PATHJOIN_STRING(part1, part2)
    return fileops_pathjoin(part1, part2)
end

function PATHNORMALIZE_STRING(path)
    return fileops_pathnormalize(path)
end

function PATHEXT_STRING(path)
    return fileops_pathext(path)
end

function PATHWITHOUTEXT_STRING(path)
    return fileops_pathwithoutext(path)
end

function PATHABSOLUTE_STRING(path)
    return fileops_pathabsolute(path)
end

function PATHSEPARATOR_STRING()
    return fileops_pathseparator()
end

function TEMPFILE_STRING(prefix, extension)
    return fileops_tempfile(prefix, extension)
end

function WORKDIR_STRING()
    return fileops_workdir()
end

function FILEOPSERRMSG_STRING()
    return fileops_geterrormsg()
end

-- Note: FILETOUCH is a boolean function, no _STRING wrapper needed

-- =============================================================================
-- Module Return
-- =============================================================================

return fileops
