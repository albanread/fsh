-- bitwise_ffi_bindings.lua
-- FasterBASIC - Bitwise Operations FFI Bindings
--
-- Provides LuaJIT FFI bindings to C++ bitwise operations for BASIC logical operators.
-- These functions work on 32-bit signed integers and match classic BASIC behavior.

local ffi = require('ffi')

-- =============================================================================
-- Bitwise Operations C API Declaration
-- =============================================================================

ffi.cdef [[
    // Bitwise operations (all take doubles, convert to int32, return int32)
    int32_t basic_band(double a, double b);  // Bitwise AND
    int32_t basic_bor(double a, double b);   // Bitwise OR
    int32_t basic_bxor(double a, double b);  // Bitwise XOR
    int32_t basic_bnot(double a);            // Bitwise NOT
    int32_t basic_beqv(double a, double b);  // Bitwise EQV (equivalence)
    int32_t basic_bimp(double a, double b);  // Bitwise IMP (implication)
    int32_t basic_shl(double a, double b);   // Left shift
    int32_t basic_shr(double a, double b);   // Right shift (arithmetic)
]]

-- =============================================================================
-- Load Library
-- =============================================================================

local lib = nil
local available = false

-- Try to load the bitwise library
-- Look in several locations: build directory, current directory, system paths
local function try_load_bitwise()
    local lib_names = {
        -- macOS
        'libbasic_bitwise.dylib',
        './libbasic_bitwise.dylib',
        '../build/libbasic_bitwise.dylib',
        'build/libbasic_bitwise.dylib',
        -- Linux
        'libbasic_bitwise.so',
        './libbasic_bitwise.so',
        '../build/libbasic_bitwise.so',
        'build/libbasic_bitwise.so',
        -- Windows
        'basic_bitwise.dll',
        './basic_bitwise.dll',
        '../build/basic_bitwise.dll',
        'build/basic_bitwise.dll',
    }

    for _, lib_name in ipairs(lib_names) do
        local ok, loaded = pcall(ffi.load, lib_name)
        if ok then
            return loaded
        end
    end

    return nil
end

lib = try_load_bitwise()

if lib then
    available = true
else
    print("Warning: Bitwise runtime library not available")
    print("  Tried loading libbasic_bitwise from multiple locations")
    print("  Falling back to Lua implementations (less efficient)")
    available = false
end

-- =============================================================================
-- Public API
-- =============================================================================

local M = {
    available = available
}

if available then
    -- Bitwise operations
    M.band = function(a, b) return lib.basic_band(a, b) end
    M.bor = function(a, b) return lib.basic_bor(a, b) end
    M.bxor = function(a, b) return lib.basic_bxor(a, b) end
    M.bnot = function(a) return lib.basic_bnot(a) end
    M.beqv = function(a, b) return lib.basic_beqv(a, b) end
    M.bimp = function(a, b) return lib.basic_bimp(a, b) end
    M.shl = function(a, b) return lib.basic_shl(a, b) end
    M.shr = function(a, b) return lib.basic_shr(a, b) end
else
    -- Fallback: use Lua implementations (slower but functional)
    local function to_int32(n)
        n = math.floor(n)
        -- Simulate 32-bit signed integer overflow
        if n >= 2147483648 then
            n = n - 4294967296
        elseif n < -2147483648 then
            n = n + 4294967296
        end
        return n
    end

    -- Fallback implementations using bit library if available
    local bit_ok, bit = pcall(require, 'bit')

    if bit_ok then
        -- Use LuaJIT bit library
        M.band = function(a, b) return bit.band(to_int32(a), to_int32(b)) end
        M.bor = function(a, b) return bit.bor(to_int32(a), to_int32(b)) end
        M.bxor = function(a, b) return bit.bxor(to_int32(a), to_int32(b)) end
        M.bnot = function(a) return bit.bnot(to_int32(a)) end
        M.beqv = function(a, b) return bit.bnot(bit.bxor(to_int32(a), to_int32(b))) end
        M.bimp = function(a, b) return bit.bor(bit.bnot(to_int32(a)), to_int32(b)) end
        M.shl = function(a, b)
            local shift = to_int32(b)
            if shift < 0 or shift >= 32 then return to_int32(a) end
            return bit.lshift(to_int32(a), shift)
        end
        M.shr = function(a, b)
            local shift = to_int32(b)
            if shift < 0 or shift >= 32 then return to_int32(a) end
            return bit.arshift(to_int32(a), shift)
        end
    else
        -- Pure Lua fallback (very slow, limited accuracy)
        print("Warning: Neither C++ bitwise library nor LuaJIT bit library available")
        print("         Bitwise operations will use slow pure Lua fallback")

        M.band = function(a, b)
            local ia, ib = to_int32(a), to_int32(b)
            local result = 0
            local bit = 1
            for i = 0, 31 do
                if (ia % 2 == 1 or ia % 2 == -1) and (ib % 2 == 1 or ib % 2 == -1) then
                    result = result + bit
                end
                ia = math.floor(ia / 2)
                ib = math.floor(ib / 2)
                bit = bit * 2
            end
            return to_int32(result)
        end

        M.bor = function(a, b)
            local ia, ib = to_int32(a), to_int32(b)
            local result = 0
            local bit = 1
            for i = 0, 31 do
                if (ia % 2 == 1 or ia % 2 == -1) or (ib % 2 == 1 or ib % 2 == -1) then
                    result = result + bit
                end
                ia = math.floor(ia / 2)
                ib = math.floor(ib / 2)
                bit = bit * 2
            end
            return to_int32(result)
        end

        M.bxor = function(a, b)
            local ia, ib = to_int32(a), to_int32(b)
            local result = 0
            local bit = 1
            for i = 0, 31 do
                local ba = (ia % 2 == 1 or ia % 2 == -1)
                local bb = (ib % 2 == 1 or ib % 2 == -1)
                if ba ~= bb then
                    result = result + bit
                end
                ia = math.floor(ia / 2)
                ib = math.floor(ib / 2)
                bit = bit * 2
            end
            return to_int32(result)
        end

        M.bnot = function(a)
            -- Bitwise NOT using two's complement: NOT(x) = -x - 1
            return to_int32(-to_int32(a) - 1)
        end

        M.beqv = function(a, b)
            return M.bnot(M.bxor(a, b))
        end

        M.bimp = function(a, b)
            return M.bor(M.bnot(a), b)
        end

        M.shl = function(a, b)
            local shift = to_int32(b)
            if shift < 0 or shift >= 32 then return to_int32(a) end
            return to_int32(to_int32(a) * (2 ^ shift))
        end

        M.shr = function(a, b)
            local shift = to_int32(b)
            if shift < 0 or shift >= 32 then return to_int32(a) end
            return to_int32(math.floor(to_int32(a) / (2 ^ shift)))
        end
    end
end

return M
