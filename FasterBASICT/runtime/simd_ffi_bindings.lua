-- simd_ffi_bindings.lua
-- FasterBASIC SIMD FFI Bindings
-- LuaJIT FFI declarations for the native SIMD library
--
-- This module provides zero-copy SIMD acceleration for PAIR and QUAD array operations.
-- It is loaded automatically when SIMD-capable types are detected.
--
-- Usage:
--   local simd = require("simd_ffi_bindings")
--   local arr = simd.alloc_pair_array(100)  -- Allocate aligned array for 100 PAIRs
--   simd.pair_array_add(result, a, b, 100)

local ffi = require("ffi")

-- =============================================================================
-- FFI C Declarations
-- =============================================================================

ffi.cdef [[
    // Memory allocation (for aligned arrays)
    void* malloc(size_t size);
    void free(void* ptr);
    int posix_memalign(void** memptr, size_t alignment, size_t size);

    // PAIR Operations (2 doubles per element)
    void simd_pair_array_add(double* result, const double* a, const double* b, int count);
    void simd_pair_array_sub(double* result, const double* a, const double* b, int count);
    void simd_pair_array_scale(double* result, const double* a, double scalar, int count);
    void simd_pair_array_add_scalar(double* result, const double* a, double scalar, int count);
    void simd_pair_array_sub_scalar(double* result, const double* a, double scalar, int count);

    // QUAD Operations (4 floats per element)
    void simd_quad_array_add(float* result, const float* a, const float* b, int count);
    void simd_quad_array_sub(float* result, const float* a, const float* b, int count);
    void simd_quad_array_scale(float* result, const float* a, float scalar, int count);
    void simd_quad_array_add_scalar(float* result, const float* a, float scalar, int count);
    void simd_quad_array_sub_scalar(float* result, const float* a, float scalar, int count);

    // Utility functions
    int simd_check_alignment(const void* ptr);
    int simd_get_alignment_requirement(void);
]]

-- =============================================================================
-- Library Loading
-- =============================================================================

local simd_lib = nil
local simd_available = false

-- Try to load the SIMD library
local function load_simd_library()
    -- Try different library names/paths
    local possible_paths = {
        "libbasic_simd.dylib",           -- macOS current directory
        "libbasic_simd.so",              -- Linux current directory
        "./libbasic_simd.dylib",         -- macOS relative
        "./libbasic_simd.so",            -- Linux relative
        "./runtime/libbasic_simd.dylib", -- macOS runtime directory
        "./runtime/libbasic_simd.so",    -- Linux runtime directory
    }

    for _, path in ipairs(possible_paths) do
        local success, lib = pcall(ffi.load, path)
        if success then
            simd_lib = lib
            simd_available = true
            return true
        end
    end

    return false
end

-- Attempt to load library (silent failure is OK - we'll use fallback)
load_simd_library()

-- =============================================================================
-- Memory Allocation Helpers
-- =============================================================================

-- Allocate aligned memory for SIMD operations
-- Returns a cdata pointer and a gc object to ensure cleanup
local function alloc_aligned(size_bytes, alignment)
    local ptr_holder = ffi.new("void*[1]")
    local result = ffi.C.posix_memalign(ptr_holder, alignment, size_bytes)

    if result ~= 0 then
        error("Failed to allocate aligned memory")
    end

    local ptr = ptr_holder[0]

    -- Register finalizer to free memory
    ffi.gc(ptr, ffi.C.free)

    return ptr
end

-- =============================================================================
-- Public API
-- =============================================================================

local M = {}

-- Check if SIMD library is available
function M.is_available()
    return simd_available
end

-- Get alignment requirement (returns 16)
function M.get_alignment()
    if simd_available then
        return simd_lib.simd_get_alignment_requirement()
    else
        return 16 -- Standard NEON alignment
    end
end

-- Check if a pointer is properly aligned
function M.check_alignment(ptr)
    if simd_available then
        return simd_lib.simd_check_alignment(ptr) == 1
    else
        -- Manual check
        local addr = tonumber(ffi.cast("intptr_t", ptr))
        return (addr % 16) == 0
    end
end

-- Allocate an aligned PAIR array (2 doubles per element)
-- Returns a double* cdata with automatic cleanup
function M.alloc_pair_array(count)
    local size = count * 2 * ffi.sizeof("double")
    local ptr = alloc_aligned(size, 16)
    return ffi.cast("double*", ptr)
end

-- Allocate an aligned QUAD array (4 floats per element)
-- Returns a float* cdata with automatic cleanup
function M.alloc_quad_array(count)
    local size = count * 4 * ffi.sizeof("float")
    local ptr = alloc_aligned(size, 16)
    return ffi.cast("float*", ptr)
end

-- =============================================================================
-- PAIR Operations (2 doubles)
-- =============================================================================

-- C() = A() + B()
function M.pair_array_add(result, a, b, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_pair_array_add(result, a, b, count)
end

-- C() = A() - B()
function M.pair_array_sub(result, a, b, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_pair_array_sub(result, a, b, count)
end

-- B() = A() * scalar
function M.pair_array_scale(result, a, scalar, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_pair_array_scale(result, a, scalar, count)
end

-- B() = A() + scalar
function M.pair_array_add_scalar(result, a, scalar, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_pair_array_add_scalar(result, a, scalar, count)
end

-- B() = A() - scalar
function M.pair_array_sub_scalar(result, a, scalar, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_pair_array_sub_scalar(result, a, scalar, count)
end

-- =============================================================================
-- QUAD Operations (4 floats)
-- =============================================================================

-- C() = A() + B()
function M.quad_array_add(result, a, b, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_quad_array_add(result, a, b, count)
end

-- C() = A() - B()
function M.quad_array_sub(result, a, b, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_quad_array_sub(result, a, b, count)
end

-- B() = A() * scalar
function M.quad_array_scale(result, a, scalar, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_quad_array_scale(result, a, scalar, count)
end

-- B() = A() + scalar
function M.quad_array_add_scalar(result, a, scalar, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_quad_array_add_scalar(result, a, scalar, count)
end

-- B() = A() - scalar
function M.quad_array_sub_scalar(result, a, scalar, count)
    if not simd_available then
        error("SIMD library not available")
    end
    simd_lib.simd_quad_array_sub_scalar(result, a, scalar, count)
end

-- =============================================================================
-- Fallback Implementations (Pure Lua)
-- =============================================================================
-- These are used when the native SIMD library is not available

function M.pair_array_add_fallback(result, a, b, count)
    for i = 0, count - 1 do
        local idx = i * 2
        result[idx] = a[idx] + b[idx]
        result[idx + 1] = a[idx + 1] + b[idx + 1]
    end
end

function M.pair_array_sub_fallback(result, a, b, count)
    for i = 0, count - 1 do
        local idx = i * 2
        result[idx] = a[idx] - b[idx]
        result[idx + 1] = a[idx + 1] - b[idx + 1]
    end
end

function M.pair_array_scale_fallback(result, a, scalar, count)
    for i = 0, count - 1 do
        local idx = i * 2
        result[idx] = a[idx] * scalar
        result[idx + 1] = a[idx + 1] * scalar
    end
end

function M.pair_array_add_scalar_fallback(result, a, scalar, count)
    for i = 0, count - 1 do
        local idx = i * 2
        result[idx] = a[idx] + scalar
        result[idx + 1] = a[idx + 1] + scalar
    end
end

function M.pair_array_sub_scalar_fallback(result, a, scalar, count)
    for i = 0, count - 1 do
        local idx = i * 2
        result[idx] = a[idx] - scalar
        result[idx + 1] = a[idx + 1] - scalar
    end
end

function M.quad_array_add_fallback(result, a, b, count)
    for i = 0, count - 1 do
        local idx = i * 4
        result[idx] = a[idx] + b[idx]
        result[idx + 1] = a[idx + 1] + b[idx + 1]
        result[idx + 2] = a[idx + 2] + b[idx + 2]
        result[idx + 3] = a[idx + 3] + b[idx + 3]
    end
end

function M.quad_array_sub_fallback(result, a, b, count)
    for i = 0, count - 1 do
        local idx = i * 4
        result[idx] = a[idx] - b[idx]
        result[idx + 1] = a[idx + 1] - b[idx + 1]
        result[idx + 2] = a[idx + 2] - b[idx + 2]
        result[idx + 3] = a[idx + 3] - b[idx + 3]
    end
end

function M.quad_array_scale_fallback(result, a, scalar, count)
    for i = 0, count - 1 do
        local idx = i * 4
        result[idx] = a[idx] * scalar
        result[idx + 1] = a[idx + 1] * scalar
        result[idx + 2] = a[idx + 2] * scalar
        result[idx + 3] = a[idx + 3] * scalar
    end
end

function M.quad_array_add_scalar_fallback(result, a, scalar, count)
    for i = 0, count - 1 do
        local idx = i * 4
        result[idx] = a[idx] + scalar
        result[idx + 1] = a[idx + 1] + scalar
        result[idx + 2] = a[idx + 2] + scalar
        result[idx + 3] = a[idx + 3] + scalar
    end
end

function M.quad_array_sub_scalar_fallback(result, a, scalar, count)
    for i = 0, count - 1 do
        local idx = i * 4
        result[idx] = a[idx] - scalar
        result[idx + 1] = a[idx + 1] - scalar
        result[idx + 2] = a[idx + 2] - scalar
        result[idx + 3] = a[idx + 3] - scalar
    end
end

-- =============================================================================
-- Module Info
-- =============================================================================

function M.version()
    return "1.0.0"
end

function M.info()
    return {
        version = M.version(),
        available = simd_available,
        alignment = M.get_alignment(),
        library = simd_lib and "loaded" or "not loaded",
        operations = {
            pair = { "add", "sub", "scale", "add_scalar", "sub_scalar" },
            quad = { "add", "sub", "scale", "add_scalar", "sub_scalar" }
        }
    }
end

return M
