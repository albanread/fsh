//
// fasterbasic_lua_codegen.cpp
// FasterBASIC - IR to Lua Code Generator Implementation
//

#include "fasterbasic_lua_codegen.h"
#include "../runtime/ConstantsManager.h"
#include "modular_commands.h"
#include "plugin_loader.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>
#include <numeric>

namespace FasterBASIC {

// =============================================================================
// LuaCodeGenStats Implementation
// =============================================================================

void LuaCodeGenStats::print() const {
    std::cout << "=== Lua Code Generation Stats ===" << std::endl;
    std::cout << "IR Instructions: " << irInstructions << std::endl;
    std::cout << "Lua Lines Generated: " << linesGenerated << std::endl;
    std::cout << "Variables: " << variablesUsed << std::endl;
    std::cout << "Arrays: " << arraysUsed << std::endl;
    std::cout << "Labels: " << labelsGenerated << std::endl;
    std::cout << "Generation Time: " << generationTimeMs << " ms" << std::endl;
}

// =============================================================================
// Helper Functions
// =============================================================================

// Mangle BASIC identifier names with type suffixes to be Lua-compatible
static std::string mangleName(const std::string& name) {
    if (name.empty()) return name;

    char suffix = name.back();
    std::string base = name;

    switch (suffix) {
        case '$':
            base.pop_back();
            return base + "_STRING";
        case '%':
            base.pop_back();
            return base + "_INT";
        case '#':
            base.pop_back();
            return base + "_DOUBLE";
        case '!':
            base.pop_back();
            return base + "_FLOAT";
        case '&':
            base.pop_back();
            return base + "_LONG";
        default:
            return name;
    }
}

// =============================================================================
// LuaCodeGenerator Implementation
// =============================================================================

LuaCodeGenerator::LuaCodeGenerator()
    : m_usesConstants(false)
    , m_unicodeMode(false)
    , m_arrayBase(1)
    , m_bufferMode(false)
    , m_errorTracking(false)
    , m_usesSIMD(false) {
}

LuaCodeGenerator::LuaCodeGenerator(const LuaCodeGenConfig& config)
    : m_config(config)
    , m_usesConstants(false)
    , m_unicodeMode(false)
    , m_arrayBase(1)
    , m_bufferMode(false)
    , m_errorTracking(false)
    , m_usesSIMD(false) {
}

LuaCodeGenerator::~LuaCodeGenerator() {
}

std::string LuaCodeGenerator::generate(const IRCode& irCode) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Reset state
    m_output.str("");
    m_output.clear();
    m_stats = LuaCodeGenStats{};
    m_code = &irCode;  // Store pointer to IR code for accessing metadata
    m_variables.clear();
    m_arrays.clear();
    m_labels.clear();
    m_stringTable.clear();
    m_exprStack.clear();
    m_labelAddresses.clear();
    m_forLoopStack.clear();
    m_doLoopStack.clear();
    m_tempVarCounter = 0;
    m_gosubReturnCounter = 0;
    m_gosubReturnIds.clear();
    m_exprOptimizer.reset();
    m_useExpressionMode = true;
    m_indentOffset = 0;
    m_arrayInfo.clear();
    m_functionDefs.clear();
    m_currentFunction = nullptr;
    m_lastEmittedOpcode = IROpcode::NOP;
    m_arrayBase = irCode.arrayBase;  // Copy OPTION BASE setting from IR
    m_unicodeMode = irCode.unicodeMode;  // Copy OPTION UNICODE setting from IR
    m_bufferMode = m_config.enableBufferMode;  // Copy buffer mode setting from config
    m_errorTracking = irCode.errorTracking;  // Copy OPTION ERROR setting from IR
    m_forceYieldEnabled = irCode.forceYieldEnabled;  // Copy OPTION FORCE_YIELD setting from IR
    m_forceYieldBudget = irCode.forceYieldBudget;  // Copy OPTION FORCE_YIELD budget from IR
    m_exprOptimizer.setUnicodeMode(m_unicodeMode);  // Set Unicode mode for proper string comparison
    m_lastEmittedLine = 0;  // Track last emitted line number
    m_constantsManager = irCode.constantsManager;  // Copy constants manager pointer for inlining
    m_variableAccess.clear();
    m_hotVariables.clear();
    m_coldVariableIDs.clear();
    m_usedLocalSlots = 0;
    m_usesSIMD = false;  // Reset SIMD detection flag

    m_stats.irInstructions = irCode.instructions.size();

    // First pass: detect SIMD usage
    for (const auto& instr : irCode.instructions) {
        if (instr.opcode >= IROpcode::SIMD_PAIR_ARRAY_ADD && 
            instr.opcode <= IROpcode::SIMD_QUAD_ARRAY_SUB_SCALAR) {
            m_usesSIMD = true;
            break;
        }
    }

    // Second pass: collect symbols and resolve labels
    resolveLabels(irCode);

    // Third pass: collect function/sub definitions
    collectFunctionDefinitions(irCode);

    // Fourth pass: analyze variable access patterns for hot/cold caching
    if (m_config.useVariableCache) {
        analyzeVariableAccess(irCode);
        selectHotVariables();
    }

    // Generate code sections
    emitHeader();
    emitVariableDeclarations();
    emitArrayDeclarations();
    emitDataSection(irCode);
    emitTypeDefinitions(irCode);
    emitUserFunctions(irCode);
    emitMainFunction(irCode);
    emitFooter();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.generationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return m_output.str();
}

// =============================================================================
// Header/Footer Generation
// =============================================================================

void LuaCodeGenerator::emitHeader() {
    emitLine("-- FasterBASIC Generated Lua Code");
    emitLine("-- Optimized for LuaJIT trace compilation");
    emitLine("");

    if (m_config.useLuaJITHints) {
        emitLine("-- LuaJIT optimization hints");
        emitLine("local ffi = require('ffi')");
        emitLine("");

        emitLine("-- Bitwise operations (check if already injected by runtime)");
        emitLine("local bitwise = bitwise or require('runtime.bitwise_ffi_bindings')");
        emitLine("");
        
        emitLine("-- String functions library (BCX-compatible extended functions)");
        emitLine("local string_ok, string_lib = pcall(require, 'runtime.string_functions')");
        emitLine("if not string_ok then");
        emitLine("    -- Fallback: try loading from current directory");
        emitLine("    string_ok, string_lib = pcall(dofile, 'runtime/string_functions.lua')");
        emitLine("end");
        emitLine("");
        
        emitLine("-- Math functions library (BCX-compatible extended functions)");
        emitLine("local math_ok, math_lib = pcall(require, 'runtime.math_functions')");
        emitLine("if not math_ok then");
        emitLine("    -- Fallback: try loading from current directory");
        emitLine("    math_ok, math_lib = pcall(dofile, 'runtime/math_functions.lua')");
        emitLine("end");
        emitLine("");
    }

    // Constants are now inlined directly, no runtime needed

    // Unicode support if OPTION UNICODE is enabled
    if (m_unicodeMode) {
        emitLine("-- Unicode runtime: Unified handle-based library with rope optimization");
        emitLine("local unicode_ok, unicode = pcall(require, 'runtime.unicode_unified')");
        emitLine("if not unicode_ok then");
        emitLine("    -- Fallback: try loading from current directory");
        emitLine("    unicode_ok, unicode = pcall(dofile, 'runtime/unicode_unified.lua')");
        emitLine("end");
        emitLine("if not unicode_ok or not unicode or not unicode.available then");
        emitLine("    error('OPTION UNICODE requires unicode_unified.lua library')");
        emitLine("end");
        emitLine("");
        emitLine("-- Define basic_print for Unicode mode (handles integer handles)");
        emitLine("function basic_print(val)");
        emitLine("    if type(val) == 'number' and val > 0 and val < 2^48 then");
        emitLine("        -- Likely a handle - try to convert to UTF-8");
        emitLine("        local str = unicode.unicode_to_utf8(val)");
        emitLine("        if str then");
        emitLine("            io.write(str)");
        emitLine("        else");
        emitLine("            io.write(tostring(val))");
        emitLine("        end");
        emitLine("    elseif type(val) == 'table' then");
        emitLine("        -- Old table-based Unicode (fallback)");
        emitLine("        local str = unicode.to_utf8 and unicode.to_utf8(val) or tostring(val)");
        emitLine("        io.write(str)");
        emitLine("    else");
        emitLine("        io.write(tostring(val))");
        emitLine("    end");
        emitLine("end");
        emitLine("");
    } else {
        emitLine("-- Define basic_print for ASCII mode");
        emitLine("function basic_print(val)");
        emitLine("    io.write(tostring(val))");
        emitLine("end");
        emitLine("");
    }

    emitLine("-- FFI support for high-performance numeric arrays");
    emitLine("local ffi_ok, ffi = pcall(require, 'ffi')");
    emitLine("local use_ffi = ffi_ok and ffi and jit and jit.status()");
    emitLine("");
    emitLine("-- FFI array creation helper");
    emitLine("local function create_ffi_array(size, element_type)");
    emitLine("    if not use_ffi then return nil end");
    emitLine("    local ok, result = pcall(function()");
    emitLine("        local ctype = element_type or 'double'");
    emitLine("        return {");
    emitLine("            data = ffi.new(ctype .. '[?]', size),");
    emitLine("            size = size,");
    emitLine("            type = ctype");
    emitLine("        }");
    emitLine("    end)");
    emitLine("    return ok and result or nil");
    emitLine("end");
    emitLine("");
    emitLine("-- Array type detection helper");
    emitLine("local function detect_array_type(type_suffix)");
    emitLine("    if type_suffix == '%' then return 'int32_t' end  -- INTEGER");
    emitLine("    if type_suffix == '&' then return 'int64_t' end  -- LONG");
    emitLine("    if type_suffix == '!' then return 'float' end    -- SINGLE");
    emitLine("    if type_suffix == '#' then return 'double' end   -- DOUBLE");
    emitLine("    if type_suffix == '$' then return nil end        -- STRING (no FFI)");
    emitLine("    return 'double' -- Default to DOUBLE for untyped numeric");
    emitLine("end");
    emitLine("");
    
    // SIMD support for ARM NEON acceleration (if program uses SIMD operations)
    if (m_usesSIMD) {
        emitLine("-- SIMD runtime: ARM NEON acceleration for array operations");
        emitLine("local simd_ok, _SIMD = pcall(require, 'runtime.simd_ffi_bindings')");
        emitLine("if not simd_ok then");
        emitLine("    -- Fallback: try loading from current directory");
        emitLine("    simd_ok, _SIMD = pcall(dofile, 'runtime/simd_ffi_bindings.lua')");
        emitLine("end");
        emitLine("if not simd_ok then");
        emitLine("    -- SIMD library not available - operations will use pure Lua fallback");
        emitLine("    _SIMD = nil");
        emitLine("end");
        emitLine("");
    }
    
    // Load string and math functions libraries even when not using LuaJIT hints
    if (!m_config.useLuaJITHints) {
        emitLine("-- String functions library (BCX-compatible extended functions)");
        emitLine("local string_ok, string_lib = pcall(require, 'runtime.string_functions')");
        emitLine("if not string_ok then");
        emitLine("    -- Fallback: try loading from current directory");
        emitLine("    string_ok, string_lib = pcall(dofile, 'runtime/string_functions.lua')");
        emitLine("end");
        emitLine("");
        
        emitLine("-- Math functions library (BCX-compatible extended functions)");
        emitLine("local math_ok, math_lib = pcall(require, 'runtime.math_functions')");
        emitLine("if not math_ok then");
        emitLine("    -- Fallback: try loading from current directory");
        emitLine("    math_ok, math_lib = pcall(dofile, 'runtime/math_functions.lua')");
        emitLine("end");
        emitLine("");
    }
    
    // Note: Plugin runtime files are now loaded directly into the Lua state
    // at initialization time, not emitted into generated code
    
    emitLine("-- Runtime support functions");
    emitLine("-- Note: basic_print is provided by the runtime (FBRunner3)");
    emitLine("-- It prints to the runtime text grid at the current cursor position");
    emitLine("");
    
    emitLine("-- Timer system initialization");
    emitLine("basic_timer_init()");
    emitLine("");
    emitLine("-- Timer event handler tables (must be global for C++ timer runtime)");
    emitLine("_handler_functions = {}");
    emitLine("_handler_coroutines = {}");
    emitLine("");
    emitLine("-- OPTION FORCE_YIELD configuration");
    if (m_forceYieldEnabled) {
        emitLine("local _force_yield_enabled = true");
        emitLine("local _force_yield_budget = " + std::to_string(m_forceYieldBudget));
    } else {
        emitLine("local _force_yield_enabled = false");
        emitLine("local _force_yield_budget = 0");
    }
    emitLine("");
    emitLine("-- Track current handler and frame counter for WAIT support");
    emitLine("local _current_handler = nil");
    emitLine("local _current_frame = 0");
    emitLine("local _yielded_handlers = {}  -- Handlers waiting for WAIT to complete");
    emitLine("local _main_coroutine = nil  -- Main script coroutine");
    emitLine("local _main_wait_until_frame = nil  -- Frame when main script should resume");
    emitLine("");
    emitLine("-- Smart WAIT wrappers that yield in handlers, block in main program");
    emitLine("local function basic_wait_frame()");
    emitLine("    if _current_handler then");
    emitLine("        local resume_frame = _current_frame + 1");
    emitLine("        coroutine.yield('wait_frames', resume_frame)");
    emitLine("    else");
    emitLine("        wait_frame()");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("local function basic_wait_frames(count)");
    emitLine("    if _current_handler then");
    emitLine("        -- Handler waiting - yield the handler coroutine");
    emitLine("        local resume_frame = _current_frame + count");
    emitLine("        coroutine.yield('wait_frames', resume_frame)");
    emitLine("    else");
    emitLine("        -- Main script waiting - yield the main coroutine");
    emitLine("        _main_wait_until_frame = _current_frame + count");
    emitLine("        coroutine.yield('wait_frames', _main_wait_until_frame)");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("local function basic_wait_ms(milliseconds)");
    emitLine("    if _current_handler then");
    emitLine("        local frames = math.ceil(milliseconds / 16.67)");
    emitLine("        local resume_frame = _current_frame + frames");
    emitLine("        coroutine.yield('wait_frames', resume_frame)");
    emitLine("    else");
    emitLine("        wait_ms(milliseconds)");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("local function basic_sleep(seconds)");
    emitLine("    -- Convert seconds to milliseconds and yield appropriately");
    emitLine("    if _current_handler then");
    emitLine("        -- In handler: yield with frame-based timing");
    emitLine("        local milliseconds = seconds * 1000");
    emitLine("        local frames = math.ceil(milliseconds / 16.67)");
    emitLine("        local resume_frame = _current_frame + frames");
    emitLine("        coroutine.yield('wait_frames', resume_frame)");
    emitLine("    else");
    emitLine("        -- In main program: yield for event loop to handle");
    emitLine("        local milliseconds = seconds * 1000");
    emitLine("        local frames = math.ceil(milliseconds / 16.67)");
    emitLine("        _main_wait_until_frame = _current_frame + frames");
    emitLine("        coroutine.yield('wait_frames', _main_wait_until_frame)");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("-- Enhanced event-checker coroutine with WAIT support");
    emitLine("local _event_checker = coroutine.create(function()");
    emitLine("    while true do");
    emitLine("        _current_frame = _current_frame + 1");
    emitLine("        ");
    emitLine("        -- 1. Resume main script if it's waiting and ready");
    emitLine("        if _main_coroutine and _main_wait_until_frame then");
    emitLine("            if _current_frame >= _main_wait_until_frame then");
    emitLine("                _main_wait_until_frame = nil");
    emitLine("                local ok, yield_type, resume_condition = coroutine.resume(_main_coroutine)");
    emitLine("                if not ok then");
    emitLine("                    error('Main script error: ' .. tostring(yield_type))");
    emitLine("                elseif yield_type == 'wait_frames' then");
    emitLine("                    -- Main script yielded again with new wait");
    emitLine("                    _main_wait_until_frame = resume_condition");
    emitLine("                end");
    emitLine("            end");
    emitLine("        end");
    emitLine("        ");
    emitLine("        -- 2. Check for new timer events (millisecond and frame-based)");
    emitLine("        -- Note: Frame-based timers (including EVERY 1 FRAMES) are pushed from C++");
    emitLine("        -- via basic_timer_on_frame_completed() called by the render thread");
    emitLine("        ");
    emitLine("        local event = basic_timer_try_dequeue()");
    emitLine("        if event then");
    emitLine("            local handler_name = event.handler");
    emitLine("            local handler_coro = _handler_coroutines[handler_name]");
    emitLine("            if handler_coro then");
    emitLine("                -- Check if coroutine is dead and recreate if needed");
    emitLine("                if coroutine.status(handler_coro) == 'dead' then");
    emitLine("                    handler_coro = coroutine.create(_handler_functions[handler_name])");
    emitLine("                    _handler_coroutines[handler_name] = handler_coro");
    emitLine("                end");
    emitLine("                ");
    emitLine("                -- Set up forced yield if enabled");
    emitLine("                if _force_yield_enabled then");
    emitLine("                    local instruction_count = 0");
    emitLine("                    debug.sethook(handler_coro, function()");
    emitLine("                        instruction_count = instruction_count + 1");
    emitLine("                        if instruction_count >= _force_yield_budget then");
    emitLine("                            error('__FORCED_YIELD__')");
    emitLine("                        end");
    emitLine("                    end, '', 1)");
    emitLine("                end");
    emitLine("                ");
    emitLine("                -- Resume handler coroutine");
    emitLine("                _current_handler = handler_name");
    emitLine("                local ok, yield_type, resume_condition = coroutine.resume(handler_coro)");
    emitLine("                _current_handler = nil");
    emitLine("                ");
    emitLine("                -- Clear forced yield hook");
    emitLine("                if _force_yield_enabled then");
    emitLine("                    debug.sethook(handler_coro, nil)");
    emitLine("                end");
    emitLine("                ");
    emitLine("                if not ok then");
    emitLine("                    -- Check if it was a forced yield");
    emitLine("                    if yield_type == '__FORCED_YIELD__' then");
    emitLine("                        -- Handler was preempted, save for resumption");
    emitLine("                        table.insert(_yielded_handlers, {");
    emitLine("                            handler_name = handler_name,");
    emitLine("                            coro = handler_coro,");
    emitLine("                            yield_type = 'preempted',");
    emitLine("                            resume_frame = _current_frame + 1");
    emitLine("                        })");
    emitLine("                    else");
    emitLine("                        print('Timer handler error (' .. handler_name .. '): ' .. tostring(yield_type))");
    emitLine("                    end");
    emitLine("                elseif yield_type == 'wait_frames' then");
    emitLine("                    -- Handler yielded due to WAIT - track it for later resumption");
    emitLine("                    table.insert(_yielded_handlers, {");
    emitLine("                        handler_name = handler_name,");
    emitLine("                        coro = handler_coro,");
    emitLine("                        yield_type = 'wait_frames',");
    emitLine("                        resume_frame = resume_condition");
    emitLine("                    })");
    emitLine("                end");
    emitLine("            end");
    emitLine("        end");
    emitLine("        ");
    emitLine("        -- 3. Check yielded handlers for resume");
    emitLine("        local i = 1");
    emitLine("        while i <= #_yielded_handlers do");
    emitLine("            local yielded = _yielded_handlers[i]");
    emitLine("            local should_resume = false");
    emitLine("            ");
    emitLine("            if yielded.yield_type == 'wait_frames' then");
    emitLine("                should_resume = (_current_frame >= yielded.resume_frame)");
    emitLine("            elseif yielded.yield_type == 'preempted' then");
    emitLine("                -- Always resume preempted handlers next frame");
    emitLine("                should_resume = (_current_frame >= yielded.resume_frame)");
    emitLine("            end");
    emitLine("            ");
    emitLine("            if should_resume then");
    emitLine("                -- Set up forced yield if enabled");
    emitLine("                if _force_yield_enabled then");
    emitLine("                    local instruction_count = 0");
    emitLine("                    debug.sethook(yielded.coro, function()");
    emitLine("                        instruction_count = instruction_count + 1");
    emitLine("                        if instruction_count >= _force_yield_budget then");
    emitLine("                            error('__FORCED_YIELD__')");
    emitLine("                        end");
    emitLine("                    end, '', 1)");
    emitLine("                end");
    emitLine("                ");
    emitLine("                -- Time to resume this handler");
    emitLine("                _current_handler = yielded.handler_name");
    emitLine("                local ok, yield_type, resume_condition = coroutine.resume(yielded.coro)");
    emitLine("                _current_handler = nil");
    emitLine("                ");
    emitLine("                -- Clear forced yield hook");
    emitLine("                if _force_yield_enabled then");
    emitLine("                    debug.sethook(yielded.coro, nil)");
    emitLine("                end");
    emitLine("                ");
    emitLine("                if not ok then");
    emitLine("                    if yield_type == '__FORCED_YIELD__' then");
    emitLine("                        -- Update resume frame for next attempt");
    emitLine("                        yielded.resume_frame = _current_frame + 1");
    emitLine("                        i = i + 1");
    emitLine("                    else");
    emitLine("                        -- Error during resume");
    emitLine("                        print('Timer handler error (' .. yielded.handler_name .. '): ' .. tostring(yield_type))");
    emitLine("                        table.remove(_yielded_handlers, i)");
    emitLine("                    end");
    emitLine("                elseif coroutine.status(yielded.coro) == 'dead' then");
    emitLine("                    -- Handler completed");
    emitLine("                    table.remove(_yielded_handlers, i)");
    emitLine("                elseif yield_type == 'wait_frames' then");
    emitLine("                    -- Handler yielded again - update resume condition");
    emitLine("                    yielded.resume_frame = resume_condition");
    emitLine("                    i = i + 1");
    emitLine("                else");
    emitLine("                    -- Handler yielded for unknown reason - remove it");
    emitLine("                    table.remove(_yielded_handlers, i)");
    emitLine("                end");
    emitLine("            else");
    emitLine("                i = i + 1");
    emitLine("            end");
    emitLine("        end");
    emitLine("        ");
    emitLine("        coroutine.yield()");
    emitLine("    end");
    emitLine("end)");
    emitLine("");
    emitLine("-- Debug hook to check for timer events and resume main coroutine");
    emitLine("local _timer_check_interval = 1000  -- Default: check every 1000 instructions");
    emitLine("local function _event_checker_hook(event, line)");
    emitLine("    -- Check for Control+C interruption");
    emitLine("    if check_should_stop then check_should_stop() end");
    emitLine("    -- Resume event checker to process timer events (if not already running)");
    emitLine("    if coroutine.status(_event_checker) == 'suspended' then");
    emitLine("        local ok, err = coroutine.resume(_event_checker)");
    emitLine("        if not ok and err then");
    emitLine("            io.stderr:write('Event checker error: ' .. tostring(err) .. '\\n')");
    emitLine("        end");
    emitLine("    end");
    emitLine("    ");
    emitLine("    -- Resume main coroutine if it yielded for loop check (not waiting)");
    emitLine("    if not _main_wait_until_frame and coroutine.status(_main_coroutine) == 'suspended' then");
    emitLine("        local ok, yield_type, resume_condition = coroutine.resume(_main_coroutine)");
    emitLine("        if not ok then");
    emitLine("            io.stderr:write('Main script error: ' .. tostring(yield_type) .. '\\n')");
    emitLine("            error(yield_type)");
    emitLine("        elseif yield_type == 'wait_frames' then");
    emitLine("            _main_wait_until_frame = resume_condition");
    emitLine("        end");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("-- Function to set timer check interval");
    emitLine("local function _set_timer_interval(interval)");
    emitLine("    _timer_check_interval = interval");
    emitLine("    debug.sethook(_event_checker_hook, '', interval)");
    emitLine("end");
    emitLine("");
    emitLine("-- Install the debug hook with default interval");
    emitLine("debug.sethook(_event_checker_hook, '', _timer_check_interval)");
    emitLine("");

    emitLine("local function basic_input()");
    emitLine("    return tonumber(io.read()) or 0");
    emitLine("end");
    emitLine("");

    emitLine("local function basic_rnd()");
    emitLine("    return math.random()");
    emitLine("end");
    emitLine("");

    emitLine("-- BASIC Boolean Conversion Functions");
    emitLine("-- BASIC uses 0 for FALSE and -1 for TRUE");
    emitLine("-- Lua uses false and true, where ONLY false and nil are falsy (0 is truthy!)");
    emitLine("");
    emitLine("-- Convert BASIC boolean (0/-1 or any number) to Lua boolean");
    emitLine("-- Also handles Lua booleans (true/false) from comparison operators");
    emitLine("local function basicBoolToLua(val)");
    emitLine("    if type(val) == 'boolean' then return val end");
    emitLine("    return val ~= 0");
    emitLine("end");
    emitLine("");
    emitLine("-- Convert Lua boolean to BASIC boolean (-1 for true, 0 for false)");
    emitLine("local function luaBoolToBasic(val)");
    emitLine("    return val and -1 or 0");
    emitLine("end");
    emitLine("");

    emitLine("-- String Buffer System for Efficient MID$ Assignment");
    emitLine("-- Creates a mutable character array for efficient string manipulation");
    emitLine("local function create_string_buffer(initial_string)");
    emitLine("    if not initial_string then initial_string = '' end");
    emitLine("    local buffer = {}");
    emitLine("    -- Convert string to character array for O(1) access");
    emitLine("    for i = 1, #initial_string do");
    emitLine("        buffer[i] = initial_string:sub(i, i)");
    emitLine("    end");
    emitLine("    buffer._length = #initial_string");
    emitLine("    buffer._is_buffer = true");
    emitLine("    return buffer");
    emitLine("end");
    emitLine("");
    emitLine("-- Convert string buffer back to regular string");
    emitLine("local function buffer_to_string(buffer)");
    emitLine("    if not buffer or not buffer._is_buffer then");
    emitLine("        return tostring(buffer or '')");
    emitLine("    end");
    emitLine("    return table.concat(buffer, '', 1, buffer._length)");
    emitLine("end");
    emitLine("");
    emitLine("-- Efficient MID$ assignment using string buffer");
    emitLine("local function mid_assign_buffer(buffer, pos, len, replacement)");
    emitLine("    if not buffer._is_buffer then");
    emitLine("        error('mid_assign_buffer requires a string buffer')");
    emitLine("    end");
    emitLine("    if pos < 1 then pos = 1 end");
    emitLine("    if len < 1 then return end");
    emitLine("    ");
    emitLine("    -- Extend buffer if necessary");
    emitLine("    local end_pos = pos + len - 1");
    emitLine("    if end_pos > buffer._length then");
    emitLine("        -- Extend buffer with spaces");
    emitLine("        for i = buffer._length + 1, end_pos do");
    emitLine("            buffer[i] = ' '");
    emitLine("        end");
    emitLine("        buffer._length = end_pos");
    emitLine("    end");
    emitLine("    ");
    emitLine("    -- Perform efficient character-by-character assignment");
    emitLine("    local rep_len = math.min(len, #replacement)");
    emitLine("    for i = 1, rep_len do");
    emitLine("        buffer[pos + i - 1] = replacement:sub(i, i)");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("-- Check if a value is a string buffer");
    emitLine("local function is_string_buffer(value)");
    emitLine("    return type(value) == 'table' and value._is_buffer == true");
    emitLine("end");
    emitLine("");
    emitLine("-- Ensure a string value is converted to a buffer if needed");
    emitLine("local function ensure_string_buffer(value)");
    emitLine("    if is_string_buffer(value) then");
    emitLine("        return value");
    emitLine("    else");
    emitLine("        return create_string_buffer(tostring(value))");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("-- Auto-convert variable to buffer mode for efficient MID$ assignment");
    emitLine("local function auto_convert_to_buffer(value)");
    if (m_bufferMode) {
        emitLine("    -- Buffer mode enabled: always convert to buffers");
        emitLine("    return ensure_string_buffer(value)");
    } else {
        emitLine("    -- Buffer mode disabled: return as-is");
        emitLine("    return value");
    }
    emitLine("end");
    emitLine("");

    emitLine("-- MID$ assignment function with intelligent buffer support");
    emitLine("-- Simulates: MID$(original$, pos, len) = replacement$");

    if (m_unicodeMode) {
        emitLine("-- Unicode mode: strings are tables of codepoints (mutable!)");
        emitLine("-- We can do TRUE in-place modification for better performance");
        emitLine("local function basic_mid_assign(original, pos, len, replacement)");
        emitLine("    -- Handle edge cases");
        emitLine("    if pos < 1 then pos = 1 end");
        emitLine("    if len < 1 then return original end");
        emitLine("    ");
        emitLine("    -- If position is beyond the string, return original unchanged");
        emitLine("    if pos > #original then return original end");
        emitLine("    ");
        emitLine("    -- Modify the table IN PLACE (tables are mutable!)");
        emitLine("    local replaceLen = math.min(len, #replacement)");
        emitLine("    for i = 1, replaceLen do");
        emitLine("        original[pos + i - 1] = replacement[i]");
        emitLine("    end");
        emitLine("    ");
        emitLine("    -- Return the modified table (same reference)");
        emitLine("    return original");
        emitLine("end");
    } else {
        emitLine("-- Standard mode: intelligent buffer vs string reconstruction");
        emitLine("local function basic_mid_assign(original, pos, len, replacement)");
        emitLine("    -- Handle edge cases");
        emitLine("    if pos < 1 then pos = 1 end");
        emitLine("    if len < 1 then return original end");
        emitLine("    ");
        emitLine("    -- Check if original is already a buffer");
        emitLine("    if is_string_buffer(original) then");
        emitLine("        -- Use efficient buffer assignment");
        emitLine("        mid_assign_buffer(original, pos, len, tostring(replacement))");
        emitLine("        return original");
        
        if (m_bufferMode) {
            emitLine("    elseif type(original) == 'string' then");
            emitLine("        -- Buffer mode enabled: convert to buffer for efficient assignment");
            emitLine("        local buffer = create_string_buffer(original)");
            emitLine("        mid_assign_buffer(buffer, pos, len, tostring(replacement))");
            emitLine("        return buffer");
        }
        
        emitLine("    end");
        emitLine("    ");
        emitLine("    -- For regular strings, use reconstruction (preserves current behavior)");
        emitLine("    local startPos = pos");
        emitLine("    ");
        emitLine("    -- If position is beyond the string, return original unchanged");
        emitLine("    if startPos > #original then return original end");
        emitLine("    ");
        emitLine("    -- Build the result string");
        emitLine("    local result = ''");
        emitLine("    ");
        emitLine("    -- Part 1: Copy everything before the replacement position");
        emitLine("    if startPos > 1 then");
        emitLine("        result = original:sub(1, startPos - 1)");
        emitLine("    end");
        emitLine("    ");
        emitLine("    -- Part 2: Replace characters (up to min of len or replacement length)");
        emitLine("    local replaceLen = math.min(len, #replacement)");
        emitLine("    result = result .. replacement:sub(1, replaceLen)");
        emitLine("    ");
        emitLine("    -- Part 3: Keep unreplaced characters in the target range");
        emitLine("    if replaceLen < len then");
        emitLine("        local keepStart = startPos + replaceLen");
        emitLine("        local keepEnd = math.min(startPos + len - 1, #original)");
        emitLine("        if keepStart <= keepEnd then");
        emitLine("            result = result .. original:sub(keepStart, keepEnd)");
        emitLine("        end");
        emitLine("    end");
        emitLine("    ");
        emitLine("    -- Part 4: Copy everything after the target range");
        emitLine("    local endPos = startPos + len");
        emitLine("    if endPos <= #original then");
        emitLine("        result = result .. original:sub(endPos)");
        emitLine("    end");
        emitLine("    ");
        emitLine("    return result");
        emitLine("end");
    }
    emitLine("");

    emitLine("-- Custom math functions for BBC BASIC compatibility");
    emitLine("local function basic_sgn(x)");
    emitLine("    if x > 0 then return 1");
    emitLine("    elseif x < 0 then return -1");
    emitLine("    else return 0 end");
    emitLine("end");
    emitLine("");
    emitLine("local function basic_fix(x)");
    emitLine("    -- Truncate towards zero (different from math.floor)");
    emitLine("    if x >= 0 then return math.floor(x)");
    emitLine("    else return math.ceil(x) end");
    emitLine("end");
    emitLine("");
    emitLine("local function basic_mod(x, y)");
    emitLine("    -- Enhanced MOD function");
    emitLine("    if y then");
    emitLine("        -- Standard modulo: x MOD y");
    emitLine("        return x % y");
    emitLine("    elseif type(x) == 'table' then");
    emitLine("        -- Vector magnitude: MOD(array)");
    emitLine("        local sum = 0");
    emitLine("        for i = 1, #x do");
    emitLine("            sum = sum + x[i] * x[i]");
    emitLine("        end");
    emitLine("        return math.sqrt(sum)");
    emitLine("    else");
    emitLine("        return 0  -- Invalid usage");
    emitLine("    end");
    emitLine("end");
    emitLine("");

    emitLine("-- PRINT USING formatter");
    emitLine("local function basic_print_using(format, ...)");
    emitLine("    local values = {...}");
    emitLine("    local result = ''");
    emitLine("    local valueIndex = 1");
    emitLine("    local i = 1");
    emitLine("    ");
    emitLine("    while i <= #format do");
    emitLine("        local ch = format:sub(i, i)");
    emitLine("        ");
    emitLine("        if ch == '&' then");
    emitLine("            -- Whole string");
    emitLine("            if valueIndex <= #values then");
    emitLine("                result = result .. tostring(values[valueIndex])");
    emitLine("                valueIndex = valueIndex + 1");
    emitLine("            end");
    emitLine("            i = i + 1");
    emitLine("            ");
    emitLine("        elseif ch == '!' then");
    emitLine("            -- First character only");
    emitLine("            if valueIndex <= #values then");
    emitLine("                local s = tostring(values[valueIndex])");
    emitLine("                result = result .. s:sub(1, 1)");
    emitLine("                valueIndex = valueIndex + 1");
    emitLine("            end");
    emitLine("            i = i + 1");
    emitLine("            ");
    emitLine("        elseif ch == '\\\\' then");
    emitLine("            -- Fixed width string: \\  \\");
    emitLine("            local endSlash = format:find('\\\\', i + 1)");
    emitLine("            if endSlash then");
    emitLine("                local width = endSlash - i + 1");
    emitLine("                if valueIndex <= #values then");
    emitLine("                    local s = tostring(values[valueIndex])");
    emitLine("                    if #s > width then");
    emitLine("                        result = result .. s:sub(1, width)");
    emitLine("                    else");
    emitLine("                        result = result .. s .. string.rep(' ', width - #s)");
    emitLine("                    end");
    emitLine("                    valueIndex = valueIndex + 1");
    emitLine("                end");
    emitLine("                i = endSlash + 1");
    emitLine("            else");
    emitLine("                result = result .. ch");
    emitLine("                i = i + 1");
    emitLine("            end");
    emitLine("            ");
    emitLine("        elseif ch == '#' then");
    emitLine("            -- Numeric format: ###.##");
    emitLine("            local beforeDecimal = 0");
    emitLine("            local afterDecimal = 0");
    emitLine("            local hasDecimal = false");
    emitLine("            local hasComma = false");
    emitLine("            ");
    emitLine("            while i <= #format and (format:sub(i,i) == '#' or format:sub(i,i) == '.' or format:sub(i,i) == ',') do");
    emitLine("                if format:sub(i,i) == '.' then");
    emitLine("                    hasDecimal = true");
    emitLine("                elseif format:sub(i,i) == ',' then");
    emitLine("                    hasComma = true");
    emitLine("                else");
    emitLine("                    if hasDecimal then");
    emitLine("                        afterDecimal = afterDecimal + 1");
    emitLine("                    else");
    emitLine("                        beforeDecimal = beforeDecimal + 1");
    emitLine("                    end");
    emitLine("                end");
    emitLine("                i = i + 1");
    emitLine("            end");
    emitLine("            ");
    emitLine("            if valueIndex <= #values then");
    emitLine("                local num = tonumber(values[valueIndex]) or 0");
    emitLine("                local formatted");
    emitLine("                if hasDecimal then");
    emitLine("                    formatted = string.format('%.'..afterDecimal..'f', num)");
    emitLine("                else");
    emitLine("                    formatted = string.format('%d', math.floor(num))");
    emitLine("                end");
    emitLine("                ");
    emitLine("                -- Add thousands separators if requested");
    emitLine("                if hasComma and not hasDecimal then");
    emitLine("                    local k");
    emitLine("                    formatted, k = string.gsub(formatted, '^(-?%d+)(%d%d%d)', '%1,%2')");
    emitLine("                    while k > 0 do");
    emitLine("                        formatted, k = string.gsub(formatted, '^(-?%d+),(%d%d%d,)', '%1,%2')");
    emitLine("                    end");
    emitLine("                end");
    emitLine("                ");
    emitLine("                -- Right-align in field");
    emitLine("                local totalWidth = beforeDecimal + (hasDecimal and 1 + afterDecimal or 0)");
    emitLine("                if #formatted < totalWidth then");
    emitLine("                    result = result .. string.rep(' ', totalWidth - #formatted)");
    emitLine("                end");
    emitLine("                result = result .. formatted");
    emitLine("                valueIndex = valueIndex + 1");
    emitLine("            end");
    emitLine("            ");
    emitLine("        else");
    emitLine("            -- Literal character");
    emitLine("            result = result .. ch");
    emitLine("            i = i + 1");
    emitLine("        end");
    emitLine("    end");
    emitLine("    ");
    emitLine("    return result");
    emitLine("end");
    emitLine("");

    emitLine("-- INSTR function for string searching");
    emitLine("local function string_instr(haystack, needle, start)");
    emitLine("    start = start or 1");
    emitLine("    if start < 1 then start = 1 end");
    emitLine("    local pos = string.find(haystack, needle, start, true)");
    emitLine("    return pos or 0");
    emitLine("end");
    emitLine("");

    emitLine("-- JOIN$ function for joining string arrays");
    emitLine("local function string_join(array, separator)");
    emitLine("    if not array then return '' end");
    emitLine("    local result = ''");
    emitLine("    local first = true");
    emitLine("    -- Handle both 0-based and 1-based arrays");
    emitLine("    local hasZero = array[0] ~= nil");
    emitLine("    local hasOne = array[1] ~= nil");
    emitLine("    if hasZero and not hasOne then");
    emitLine("        -- 0-based array");
    emitLine("        local i = 0");
    emitLine("        while array[i] ~= nil do");
    emitLine("            if not first then result = result .. separator end");
    emitLine("            result = result .. tostring(array[i])");
    emitLine("            first = false");
    emitLine("            i = i + 1");
    emitLine("        end");
    emitLine("    else");
    emitLine("        -- 1-based array or mixed");
    emitLine("        local i = 1");
    emitLine("        while array[i] ~= nil do");
    emitLine("            if not first then result = result .. separator end");
    emitLine("            result = result .. tostring(array[i])");
    emitLine("            first = false");
    emitLine("            i = i + 1");
    emitLine("        end");
    emitLine("    end");
    emitLine("    return result");
    emitLine("end");
    emitLine("");

    emitLine("-- SPLIT$ function for splitting strings into arrays");
    emitLine("local function string_split(str, delimiter)");
    emitLine("    if not str or str == '' then return {} end");
    emitLine("    if not delimiter or delimiter == '' then");
    emitLine("        -- Split into individual characters");
    emitLine("        local result = {}");
    emitLine("        for i = 1, #str do");
    emitLine("            result[i] = str:sub(i, i)");
    emitLine("        end");
    emitLine("        return result");
    emitLine("    end");
    emitLine("    local result = {}");
    emitLine("    local start = 1");
    emitLine("    local index = 1");
    emitLine("    repeat");
    emitLine("        local pos = string.find(str, delimiter, start, true)");
    emitLine("        if pos then");
    emitLine("            result[index] = str:sub(start, pos - 1)");
    emitLine("            start = pos + #delimiter");
    emitLine("        else");
    emitLine("            result[index] = str:sub(start)");
    emitLine("        end");
    emitLine("        index = index + 1");
    emitLine("    until not pos");
    emitLine("    return result");
    emitLine("end");
    emitLine("");

    emitLine("-- Stack for expression evaluation");
    emitLine("local stack = {}");
    emitLine("local sp = 0");
    emitLine("");

    emitLine("local function push(v)");
    emitLine("    sp = sp + 1");
    emitLine("    stack[sp] = v");
    emitLine("end");
    emitLine("");

    emitLine("local function pop()");
    emitLine("    local v = stack[sp]");
    emitLine("    sp = sp - 1");
    emitLine("    return v");
    emitLine("end");
    emitLine("");
    
    emitLine("-- LBOUND/UBOUND functions for array bounds");
    emitLine("-- These look up the array from global scope since argument is evaluated as variable");
    emitLine("local function LBOUND(arr_or_var, dim)");
    emitLine("    dim = dim or 1");
    emitLine("    -- Always return OPTION BASE for LBOUND");
    emitLine("    return " + std::to_string(m_arrayBase));
    emitLine("end");
    emitLine("");
    
    emitLine("local function UBOUND(arr_or_var, dim)");
    emitLine("    dim = dim or 1");
    emitLine("    -- arr_or_var might be a variable value or an array");
    emitLine("    -- We need to look up arrays from global scope");
    emitLine("    local arr = arr_or_var");
    emitLine("    if type(arr) ~= 'table' then");
    emitLine("        -- Try to find the array by looking in global scope");
    emitLine("        -- This is a workaround for argument evaluation issues");
    emitLine("        return 0  -- Cannot determine bounds");
    emitLine("    end");
    emitLine("    local max_idx = 0");
    emitLine("    -- Handle FFI arrays");
    emitLine("    if arr.data and arr.size then");
    emitLine("        return arr.size - 1");
    emitLine("    end");
    emitLine("    -- Handle regular Lua tables");
    emitLine("    for k, v in pairs(arr) do");
    emitLine("        if type(k) == 'number' and k > max_idx then");
    emitLine("            max_idx = k");
    emitLine("        end");
    emitLine("    end");
    if (m_arrayBase == 0) {
        emitLine("    return max_idx - 1  -- Convert from 1-based Lua to 0-based BASIC");
    } else {
        emitLine("    return max_idx - 1  -- Adjust for array size");
    }
    emitLine("end");
    emitLine("");

    emitLine("-- Constants table");
    emitLine("local constants = {}");
    emitLine("");
    emitLine("-- Temp variables for operations (declared at function scope to avoid goto issues)");
    emitLine("local _on_temp = 0  -- For ON GOTO/GOSUB/CALL selector");
    emitLine("local a, b, done, dim, idx, val, ret_label");
    emitLine("");
    emitLine("-- Reusable variables for multi-dimensional arrays (reduces local count)");
    emitLine("local idx0, idx1, idx2, idx3, idx4, idx5, idx6, idx7, idx8, idx9");
    emitLine("local dim0, dim1, dim2, dim3, dim4, dim5, dim6, dim7, dim8, dim9");
    emitLine("");
    emitLine("-- Reusable FOR loop control variables (reduces local count)");
    emitLine("local for_start, for_end, for_step");
    emitLine("");
    emitLine("-- Cursor position for AT/LOCATE commands");
    emitLine("local _cursor_x, _cursor_y = 0, 0");
    emitLine("");

    // Error tracking variable (if enabled)
    if (m_errorTracking) {
        emitLine("-- BASIC line number tracker (for error reporting)");
        emitLine("-- Enable with: OPTION ERROR");
        emitLine("local _LINE = 0");
        emitLine("");
    }

    // Emit variable table if using hot/cold caching
    if (m_config.useVariableCache) {
        emitVariableTableDeclaration();
    }
    
    // Emit parameter pool for modular commands (reduces local variable usage)
    emitParameterPoolDeclaration();
}

void LuaCodeGenerator::emitFooter() {
    emitLine("");
    emitLine("-- Entry point: wrap main in coroutine and start event loop");
    emitLine("_main_coroutine = coroutine.create(main)");
    emitLine("");
    emitLine("-- Start main coroutine");
    emitLine("local success, yield_type, resume_condition = coroutine.resume(_main_coroutine)");
    emitLine("if not success then");
    emitLine("    -- Cleanup timer system on error");
    emitLine("    debug.sethook(nil)");
    emitLine("    basic_timer_shutdown()");
    emitLine("    ");
    if (m_errorTracking) {
        emitLine("    if _LINE > 0 then");
        emitLine("        io.stderr:write(\"Runtime error at BASIC line \" .. _LINE .. \": \" .. tostring(yield_type) .. \"\\n\")");
        emitLine("    else");
        emitLine("        io.stderr:write(\"Runtime error: \" .. tostring(yield_type) .. \"\\n\")");
        emitLine("    end");
        if (m_config.exitOnError) {
            emitLine("    os.exit(1)");
        }
    } else {
        emitLine("    error(yield_type)");
    }
    emitLine("elseif yield_type == 'wait_frames' then");
    emitLine("    -- Main script is waiting, set up the wait");
    emitLine("    _main_wait_until_frame = resume_condition");
    emitLine("end");
    emitLine("");
    emitLine("-- Main event loop: pump event checker while main script is running");
    emitLine("while coroutine.status(_main_coroutine) ~= 'dead' do");
    emitLine("    -- Wait for one frame");
    emitLine("    wait_frame()");
    emitLine("    ");
    emitLine("    -- Pump event checker to process timers and resume waiting coroutines");
    emitLine("    local ok, err = coroutine.resume(_event_checker)");
    emitLine("    if not ok and err then");
    emitLine("        io.stderr:write('Event checker error: ' .. tostring(err) .. '\\n')");
    emitLine("        break");
    emitLine("    end");
    emitLine("    ");
    emitLine("    -- Resume main coroutine if it yielded for loop check (not waiting)");
    emitLine("    if not _main_wait_until_frame and coroutine.status(_main_coroutine) == 'suspended' then");
    emitLine("        local ok, yield_type, resume_condition = coroutine.resume(_main_coroutine)");
    emitLine("        if not ok then");
    emitLine("            io.stderr:write('Main script error: ' .. tostring(yield_type) .. '\\n')");
    emitLine("            break");
    emitLine("        elseif yield_type == 'wait_frames' then");
    emitLine("            _main_wait_until_frame = resume_condition");
    emitLine("        end");
    emitLine("    end");
    emitLine("end");
    emitLine("");
    emitLine("-- Cleanup");
    emitLine("debug.sethook(nil)");
    emitLine("basic_timer_shutdown()");
}

void LuaCodeGenerator::emitVariableDeclarations() {
    if (m_variables.empty()) return;

    emitLine("-- Variable declarations");
    for (const auto& [name, idx] : m_variables) {
        emitLine("local " + getVarName(name) + " = 0");
    }
    emitLine("");

    m_stats.variablesUsed = m_variables.size();
}

void LuaCodeGenerator::emitArrayDeclarations() {
    if (m_arrays.empty()) return;

    emitLine("-- Array declarations");
    for (const auto& [name, idx] : m_arrays) {
        emitLine("local " + getArrayName(name));
    }
    emitLine("");

    m_stats.arraysUsed = m_arrays.size();
}

void LuaCodeGenerator::emitDataSection(const IRCode& irCode) {
    // DATA is now stored in C++ DataManager, not in Lua
    // The DataManager will be initialized by FBRunner3 before script execution
    // So we don't need to emit anything here
    (void)irCode;
}

void LuaCodeGenerator::emitTypeDefinitions(const IRCode& irCode) {
    // Emit all TYPE definitions at module level (before user functions)
    // This ensures type constructors are available when functions are defined
    emitLine("-- User-defined type constructors");
    emitLine("");
    
    for (const auto& instr : irCode.instructions) {
        if (instr.opcode == IROpcode::DEFINE_TYPE) {
            emitTypeDefinition(instr);
        }
    }
}

void LuaCodeGenerator::emitUserFunctions(const IRCode& irCode) {
    // Emit all FUNCTION and SUB definitions at module level
    emitLine("-- User-defined functions and subroutines");

    bool inFunctionDef = false;
    size_t funcStartIndex = 0;
    std::vector<std::string> definedHandlers;  // Track handler names for registration

    for (size_t i = 0; i < irCode.instructions.size(); i++) {
        const auto& instr = irCode.instructions[i];

        if (instr.opcode == IROpcode::DEFINE_FUNCTION || instr.opcode == IROpcode::DEFINE_SUB) {
            inFunctionDef = true;
            funcStartIndex = i;
            emitFunctionDefinition(instr);

            // Track the function/sub name for timer handler registration
            if (std::holds_alternative<std::string>(instr.operand1)) {
                definedHandlers.push_back(std::get<std::string>(instr.operand1));
            }

            // Skip param count and param names
            if (i + 1 < irCode.instructions.size() &&
                irCode.instructions[i + 1].opcode == IROpcode::PUSH_INT &&
                std::holds_alternative<int>(irCode.instructions[i + 1].operand1)) {
                int paramCount = std::get<int>(irCode.instructions[i + 1].operand1);
                i += 1 + paramCount; // Skip PUSH_INT and all PUSH_STRING param names
            }
        } else if (inFunctionDef && (instr.opcode == IROpcode::END_FUNCTION ||
                                      instr.opcode == IROpcode::END_SUB)) {
            emitFunctionDefinition(instr);
            inFunctionDef = false;
        } else if (inFunctionDef) {
            // Emit instructions inside the function body
            emitInstruction(instr, i);
        }
    }

    emitLine("");

    // Register all functions/subs as potential timer handlers
    if (!definedHandlers.empty()) {
        emitLine("-- Register timer event handlers");
        for (const auto& handlerName : definedHandlers) {
            emitLine("_handler_functions[\"" + handlerName + "\"] = func_" + handlerName);
            emitLine("_handler_coroutines[\"" + handlerName + "\"] = coroutine.create(func_" + handlerName + ")");
        }
        emitLine("");
    }
}

void LuaCodeGenerator::emitMainFunction(const IRCode& irCode) {
    emitLine("-- Main program");
    emitLine("local function main()");

    // First pass: collect all GOSUB target labels (subroutines to convert to functions)
    std::set<std::string> gosubTargets;
    std::set<size_t> subroutineInstructions; // Track which instructions are part of subroutines
    for (size_t i = 0; i < irCode.instructions.size(); i++) {
        const auto& instr = irCode.instructions[i];
        if (instr.opcode == IROpcode::CALL_GOSUB) {
            std::string labelStr;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                labelStr = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                labelStr = std::to_string(std::get<int>(instr.operand1));
            }
            if (!labelStr.empty()) {
                gosubTargets.insert(labelStr);
            }
        } else if (instr.opcode == IROpcode::ON_GOSUB) {
            // Parse comma-separated label IDs from operand
            std::string targets;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                targets = std::get<std::string>(instr.operand1);
            }
            if (!targets.empty()) {
                std::vector<std::string> labelIds;
                size_t start = 0;
                size_t pos = targets.find(',');
                while (pos != std::string::npos) {
                    labelIds.push_back(targets.substr(start, pos - start));
                    start = pos + 1;
                    pos = targets.find(',', start);
                }
                labelIds.push_back(targets.substr(start));

                for (const auto& label : labelIds) {
                    gosubTargets.insert(label);
                }
            }
        }
    }

    // Second pass: identify which instructions belong to subroutines
    for (const auto& targetLabel : gosubTargets) {
        bool inSubroutine = false;
        for (size_t i = 0; i < irCode.instructions.size(); i++) {
            const auto& instr = irCode.instructions[i];

            // Check if this is the target label
            if (instr.opcode == IROpcode::LABEL) {
                std::string labelStr;
                if (std::holds_alternative<std::string>(instr.operand1)) {
                    labelStr = std::get<std::string>(instr.operand1);
                } else if (std::holds_alternative<int>(instr.operand1)) {
                    labelStr = std::to_string(std::get<int>(instr.operand1));
                }
                if (labelStr == targetLabel) {
                    inSubroutine = true;
                    subroutineInstructions.insert(i); // Mark the label
                    continue;
                }
            }

            // If we're in the subroutine, mark instructions
            if (inSubroutine) {
                subroutineInstructions.insert(i);
                if (instr.opcode == IROpcode::RETURN_GOSUB) {
                    // End of subroutine
                    break;
                }
            }
        }
    }

    // Third pass: create table for GOSUB functions (avoids 200-local limit)
    emitLine("");
    emitLine("    -- GOSUB subroutines table (avoids local variable limit)");
    emitLine("    local _gosub = {}");
    emitLine("");

    // Fourth pass: emit subroutines as table entries
    for (const auto& targetLabel : gosubTargets) {
        emitLine("    _gosub." + getLabelName(targetLabel) + " = function()");

        // Find the label in the IR and emit code until RETURN
        bool inSubroutine = false;
        for (size_t i = 0; i < irCode.instructions.size(); i++) {
            const auto& instr = irCode.instructions[i];

            // Check if this is the target label
            if (instr.opcode == IROpcode::LABEL) {
                std::string labelStr;
                if (std::holds_alternative<std::string>(instr.operand1)) {
                    labelStr = std::get<std::string>(instr.operand1);
                } else if (std::holds_alternative<int>(instr.operand1)) {
                    labelStr = std::to_string(std::get<int>(instr.operand1));
                }
                if (labelStr == targetLabel) {
                    inSubroutine = true;
                    continue; // Don't emit the label itself
                }
            }

            // If we're in the subroutine, emit instructions
            if (inSubroutine) {
                if (instr.opcode == IROpcode::RETURN_GOSUB) {
                    // End of subroutine
                    emitLine("        return");
                    break;
                }

                // Emit the instruction with extra indentation for nested function
                m_indentOffset = 4; // Add 4 spaces for nested function
                emitInstruction(instr, i);
                m_indentOffset = 0; // Reset indentation
            }
        }

        emitLine("    end");
        emitLine("");
    }

    // Generate code for each instruction
    // Track the next label after FOR_INIT for loop back jumps
    std::string nextLabelAfterForInit;
    bool lastWasReturn = false;

    for (size_t i = 0; i < irCode.instructions.size(); i++) {
        const auto& instr = irCode.instructions[i];

        // Skip instructions that are part of subroutines (already emitted as functions)
        if (subroutineInstructions.count(i) > 0) {
            continue;
        }

        // Skip function/sub definitions - they were already emitted at module level
        if (instr.opcode == IROpcode::DEFINE_FUNCTION || instr.opcode == IROpcode::DEFINE_SUB) {
            // Find the corresponding END_FUNCTION/END_SUB and skip to it
            int depth = 1;
            size_t j = i + 1;

            // Skip param count and param names
            if (j < irCode.instructions.size() &&
                irCode.instructions[j].opcode == IROpcode::PUSH_INT &&
                std::holds_alternative<int>(irCode.instructions[j].operand1)) {
                int paramCount = std::get<int>(irCode.instructions[j].operand1);
                j += 1 + paramCount; // Skip PUSH_INT and all PUSH_STRING param names
            }

            while (j < irCode.instructions.size() && depth > 0) {
                if (irCode.instructions[j].opcode == IROpcode::DEFINE_FUNCTION ||
                    irCode.instructions[j].opcode == IROpcode::DEFINE_SUB) {
                    depth++;
                } else if (irCode.instructions[j].opcode == IROpcode::END_FUNCTION ||
                           irCode.instructions[j].opcode == IROpcode::END_SUB) {
                    depth--;
                }
                j++;
            }
            i = j - 1; // -1 because the loop will increment
            continue;
        }

        // Use semicolon before unreachable labels (Lua 5.2+ syntax)
        if (lastWasReturn && instr.opcode == IROpcode::LABEL) {
            // Don't emit anything, the semicolon will be added by emitLabel
            lastWasReturn = false;
        }

        // If this is a LABEL right after FOR_INIT, save it for FOR_NEXT
        if (instr.opcode == IROpcode::LABEL && !nextLabelAfterForInit.empty()) {
            if (std::holds_alternative<std::string>(instr.operand1)) {
                nextLabelAfterForInit = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                nextLabelAfterForInit = std::to_string(std::get<int>(instr.operand1));
            }

            // Store this label in the current loop context
            if (!m_forLoopStack.empty()) {
                m_forLoopStack.back().loopBackLabel = nextLabelAfterForInit;
            }
            nextLabelAfterForInit.clear();
        }

        // Mark that we need to capture the next label
        if (instr.opcode == IROpcode::FOR_INIT) {
            nextLabelAfterForInit = "pending";
        }

        emitInstruction(instr, i);

        // Track if we just emitted a return
        lastWasReturn = (instr.opcode == IROpcode::END || instr.opcode == IROpcode::HALT);
    }

    emitLine("    ::end_program::");
    emitLine("    -- Shutdown timer system");
    emitLine("    debug.sethook(nil)");
    emitLine("    basic_timer_shutdown()");

    emitLine("end");
    emitLine("");

    // DATA/READ/RESTORE support
    // Note: basic_read_data(), basic_read_data_string(), and basic_restore()
    // are provided by C++ bindings in FBTBindings.cpp
    // The DataManager is initialized with DATA values before script execution
}

// =============================================================================
// Label Resolution
// =============================================================================

void LuaCodeGenerator::resolveLabels(const IRCode& irCode) {
    for (size_t i = 0; i < irCode.instructions.size(); i++) {
        const auto& instr = irCode.instructions[i];
        if (instr.opcode == IROpcode::LABEL) {
            std::string label;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                label = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                label = std::to_string(std::get<int>(instr.operand1));
            }
            if (!label.empty()) {
                m_labelAddresses[label] = static_cast<int>(i);
                m_labels[label] = m_labels.size();
            }
        }
    }
}

void LuaCodeGenerator::collectFunctionDefinitions(const IRCode& irCode) {
    // Scan through IR to find DEFINE_FUNCTION and DEFINE_SUB instructions
    // and collect parameter information
    for (size_t i = 0; i < irCode.instructions.size(); i++) {
        const auto& instr = irCode.instructions[i];

        if (instr.opcode == IROpcode::DEFINE_FUNCTION || instr.opcode == IROpcode::DEFINE_SUB) {
            if (!std::holds_alternative<std::string>(instr.operand1)) continue;

            std::string funcName = std::get<std::string>(instr.operand1);
            FunctionInfo info;
            info.name = funcName;
            info.isFunction = (instr.opcode == IROpcode::DEFINE_FUNCTION);
            info.startIndex = i;

            // Next instruction should be param count (PUSH_INT)
            size_t currentPos = i + 1;
            if (currentPos < irCode.instructions.size()) {
                const auto& nextInstr = irCode.instructions[currentPos];
                if (nextInstr.opcode == IROpcode::PUSH_INT && std::holds_alternative<int>(nextInstr.operand1)) {
                    int paramCount = std::get<int>(nextInstr.operand1);
                    currentPos++;

                    // Following instructions should be param names (PUSH_STRING)
                    // optionally followed by PARAM_BYREF
                    for (int p = 0; p < paramCount && currentPos < irCode.instructions.size(); p++) {
                        const auto& paramInstr = irCode.instructions[currentPos];
                        if (paramInstr.opcode == IROpcode::PUSH_STRING &&
                            std::holds_alternative<std::string>(paramInstr.operand1)) {
                            std::string paramName = std::get<std::string>(paramInstr.operand1);
                            info.parameters.push_back(paramName);
                            currentPos++;
                            
                            // Check if next instruction is PARAM_BYREF for this parameter
                            bool isByRef = false;
                            if (currentPos < irCode.instructions.size()) {
                                const auto& nextInstr = irCode.instructions[currentPos];
                                if (nextInstr.opcode == IROpcode::PARAM_BYREF &&
                                    std::holds_alternative<std::string>(nextInstr.operand1) &&
                                    std::get<std::string>(nextInstr.operand1) == paramName) {
                                    isByRef = true;
                                    currentPos++;
                                }
                            }
                            info.parameterIsByRef.push_back(isByRef);
                        } else {
                            currentPos++;
                        }
                    }
                    
                    // Now scan for LOCAL and SHARED declarations throughout the function body
                    // Note: They may appear anywhere in the function, not just at the start
                    while (currentPos < irCode.instructions.size()) {
                        const auto& bodyInstr = irCode.instructions[currentPos];
                        
                        if (bodyInstr.opcode == IROpcode::DECLARE_LOCAL) {
                            if (std::holds_alternative<std::string>(bodyInstr.operand1)) {
                                info.localVariables.push_back(std::get<std::string>(bodyInstr.operand1));
                            }
                        } else if (bodyInstr.opcode == IROpcode::DECLARE_SHARED) {
                            if (std::holds_alternative<std::string>(bodyInstr.operand1)) {
                                info.sharedVariables.push_back(std::get<std::string>(bodyInstr.operand1));
                            }
                        } else if (bodyInstr.opcode == IROpcode::END_FUNCTION ||
                                   bodyInstr.opcode == IROpcode::END_SUB) {
                            // End of function body
                            break;
                        }
                        currentPos++;
                    }
                }
            }

            m_functionDefs[funcName] = info;
        }
    }
}

// =============================================================================
// Instruction Translation
// =============================================================================

void LuaCodeGenerator::emitInstruction(const IRInstruction& instr, size_t index) {
    // Emit line number tracking if enabled and line changed
    if (m_errorTracking && instr.sourceLineNumber > 0 && instr.sourceLineNumber != m_lastEmittedLine) {
        emitLine("    -- LINE " + std::to_string(instr.sourceLineNumber));
        emitLine("    _LINE = " + std::to_string(instr.sourceLineNumber));
        m_lastEmittedLine = instr.sourceLineNumber;
    }

    if (m_config.emitComments) {
        emitComment("IR[" + std::to_string(index) + "]");
    }

    // Save previous opcode before processing current instruction
    IROpcode previousOpcode = m_lastEmittedOpcode;

    switch (instr.opcode) {
        // Stack operations
        case IROpcode::PUSH_INT:
        case IROpcode::PUSH_DOUBLE:
        case IROpcode::PUSH_STRING:
            emitStackOp(instr);
            break;

        case IROpcode::POP:
            emitLine("    pop()");
            break;

        // Arithmetic
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::IDIV:
        case IROpcode::MOD:
        case IROpcode::POW:
        case IROpcode::NEG:
            emitArithmetic(instr);
            break;

        // String operations
        case IROpcode::STR_CONCAT:
        case IROpcode::UNICODE_CONCAT:
            emitStringConcat(instr);
            break;

        // Comparison
        case IROpcode::EQ:
        case IROpcode::NE:
        case IROpcode::LT:
        case IROpcode::LE:
        case IROpcode::GT:
        case IROpcode::GE:
            emitComparison(instr);
            break;

        // Logical
        case IROpcode::AND:
        case IROpcode::OR:
        case IROpcode::XOR:
        case IROpcode::EQV:
        case IROpcode::IMP:
        case IROpcode::NOT:
            emitLogical(instr);
            break;

        // Variables
        case IROpcode::LOAD_VAR:
        case IROpcode::STORE_VAR:
        case IROpcode::MID_ASSIGN:
            emitVariable(instr);
            break;

        // Constants
        case IROpcode::LOAD_CONST:
            m_usesConstants = true;  // Track that program uses constants
            emitConstant(instr);
            break;

        // Arrays
        case IROpcode::LOAD_ARRAY:
        case IROpcode::STORE_ARRAY:
        case IROpcode::DIM_ARRAY:
        case IROpcode::REDIM_ARRAY:
        case IROpcode::ERASE_ARRAY:
        case IROpcode::FILL_ARRAY:
        case IROpcode::ARRAY_ADD:
        case IROpcode::ARRAY_SUB:
        case IROpcode::ARRAY_MUL:
        case IROpcode::ARRAY_DIV:
        case IROpcode::ARRAY_ADD_SCALAR:
        case IROpcode::ARRAY_SUB_SCALAR:
        case IROpcode::ARRAY_MUL_SCALAR:
        case IROpcode::ARRAY_DIV_SCALAR:
            emitArray(instr);
            break;

        case IROpcode::SWAP_VAR:
            emitSwap(instr);
            break;

        case IROpcode::LBOUND_ARRAY:
        case IROpcode::UBOUND_ARRAY:
            emitArrayBounds(instr);
            break;

        // SIMD array operations
        case IROpcode::SIMD_PAIR_ARRAY_ADD:
        case IROpcode::SIMD_PAIR_ARRAY_SUB:
        case IROpcode::SIMD_PAIR_ARRAY_SCALE:
        case IROpcode::SIMD_PAIR_ARRAY_ADD_SCALAR:
        case IROpcode::SIMD_PAIR_ARRAY_SUB_SCALAR:
        case IROpcode::SIMD_QUAD_ARRAY_ADD:
        case IROpcode::SIMD_QUAD_ARRAY_SUB:
        case IROpcode::SIMD_QUAD_ARRAY_SCALE:
        case IROpcode::SIMD_QUAD_ARRAY_ADD_SCALAR:
        case IROpcode::SIMD_QUAD_ARRAY_SUB_SCALAR:
            emitSIMD(instr);
            break;

        // Control flow
        case IROpcode::LABEL:
        case IROpcode::JUMP:
        case IROpcode::JUMP_IF_FALSE:
        case IROpcode::JUMP_IF_TRUE:
        case IROpcode::CALL_GOSUB:
        case IROpcode::RETURN_GOSUB:
        case IROpcode::ON_GOTO:
        case IROpcode::ON_GOSUB:
        case IROpcode::ON_CALL:
        case IROpcode::IF_START:
        case IROpcode::ELSEIF_START:
        case IROpcode::ELSE_START:
        case IROpcode::IF_END:
            emitControlFlow(instr, index);
            break;

        // Loops
        case IROpcode::FOR_INIT:
        case IROpcode::FOR_CHECK:
        case IROpcode::FOR_NEXT:
        case IROpcode::FOR_IN_INIT:
        case IROpcode::FOR_IN_CHECK:
        case IROpcode::FOR_IN_NEXT:
        case IROpcode::WHILE_START:
        case IROpcode::WHILE_END:
        case IROpcode::REPEAT_START:
        case IROpcode::REPEAT_END:
        case IROpcode::DO_WHILE_START:
        case IROpcode::DO_UNTIL_START:
        case IROpcode::DO_START:
        case IROpcode::DO_LOOP_WHILE:
        case IROpcode::DO_LOOP_UNTIL:
        case IROpcode::DO_LOOP_END:
            emitLoop(instr);
            break;

        // I/O
        case IROpcode::PRINT:
        case IROpcode::PRINT_NEWLINE:
        case IROpcode::PRINT_USING:
        case IROpcode::PRINT_AT:
        case IROpcode::PRINT_AT_USING:
        case IROpcode::INPUT_AT:
        case IROpcode::INPUT:
        case IROpcode::INPUT_PROMPT:
        case IROpcode::READ_DATA:
        case IROpcode::RESTORE:
        case IROpcode::OPEN_FILE:
        case IROpcode::CLOSE_FILE:
        case IROpcode::CLOSE_FILE_ALL:
        case IROpcode::PRINT_FILE:
        case IROpcode::PRINT_FILE_NEWLINE:
        case IROpcode::INPUT_FILE:
        case IROpcode::LINE_INPUT_FILE:
        case IROpcode::WRITE_FILE:
            emitIO(instr);
            break;

        // Built-in functions
        case IROpcode::CALL_BUILTIN:
            emitBuiltinFunction(instr);
            break;

        // User-defined types
        case IROpcode::DEFINE_TYPE:
            // Types are now emitted earlier by emitTypeDefinitions()
            // Skip them during main program emission
            break;

        // User-defined functions and subs
        case IROpcode::DEFINE_FUNCTION:
        case IROpcode::DEFINE_SUB:
        case IROpcode::END_FUNCTION:
        case IROpcode::END_SUB:
            emitFunctionDefinition(instr);
            break;

        case IROpcode::CALL_FUNCTION:
        case IROpcode::CALL_SUB:
            emitFunctionCall(instr);
            break;
        
        case IROpcode::DECLARE_LOCAL:
        case IROpcode::DECLARE_SHARED:
            // These are handled during function collection and emitted at function start
            // No code emission needed here
            break;

        case IROpcode::RETURN_VALUE:
        case IROpcode::RETURN_VOID:
            emitReturn(instr);
            m_lastEmittedOpcode = instr.opcode;
            break;

        case IROpcode::EXIT_FOR:
        case IROpcode::EXIT_DO:
        case IROpcode::EXIT_WHILE:
        case IROpcode::EXIT_REPEAT:
        case IROpcode::EXIT_FUNCTION:
        case IROpcode::EXIT_SUB:
            emitExit(instr);
            break;

        case IROpcode::END:
        case IROpcode::HALT:
            emitLine("    goto end_program");
            break;

        // Timer operations
        case IROpcode::AFTER_TIMER:
        case IROpcode::EVERY_TIMER:
        case IROpcode::AFTER_FRAMES:
        case IROpcode::EVERY_FRAMES:
        case IROpcode::TIMER_STOP:
        case IROpcode::TIMER_INTERVAL:
            emitTimer(instr);
            break;



        case IROpcode::LOAD_MEMBER:
            emitLoadMember(instr);
            break;

        case IROpcode::STORE_MEMBER:
            emitStoreMember(instr);
            break;

        case IROpcode::LOAD_ARRAY_MEMBER:
            emitLoadArrayMember(instr);
            break;

        case IROpcode::STORE_ARRAY_MEMBER:
            emitStoreArrayMember(instr);
            break;

        default:
            if (m_config.emitComments) {
                emitComment("Unhandled opcode: " + std::to_string(static_cast<int>(instr.opcode)));
            }
            break;
    }

    // Track what opcode was just emitted for unreachable code detection
    m_lastEmittedOpcode = instr.opcode;
}

void LuaCodeGenerator::emitStackOp(const IRInstruction& instr) {
    // Use expression optimizer when possible
    if (canUseExpressionMode()) {
        switch (instr.opcode) {
            case IROpcode::PUSH_INT:
                if (std::holds_alternative<int>(instr.operand1)) {
                    m_exprOptimizer.pushLiteral(std::to_string(std::get<int>(instr.operand1)));
                } else {
                    m_exprOptimizer.pushLiteral("0");
                }
                return;

            case IROpcode::PUSH_DOUBLE:
                if (std::holds_alternative<double>(instr.operand1)) {
                    m_exprOptimizer.pushLiteral(std::to_string(std::get<double>(instr.operand1)));
                } else {
                    m_exprOptimizer.pushLiteral("0.0");
                }
                return;

            case IROpcode::PUSH_STRING:
                if (std::holds_alternative<std::string>(instr.operand1)) {
                    m_exprOptimizer.pushLiteral(escapeString(std::get<std::string>(instr.operand1)));
                } else {
                    m_exprOptimizer.pushLiteral("''");
                }
                return;

            default:
                break;
        }
    }

    // Fallback to stack-based emission
    switch (instr.opcode) {
        case IROpcode::PUSH_INT:
            if (std::holds_alternative<int>(instr.operand1)) {
                emitLine("    push(" + std::to_string(std::get<int>(instr.operand1)) + ")");
            } else {
                emitLine("    push(0)");
            }
            break;

        case IROpcode::PUSH_DOUBLE:
            if (std::holds_alternative<double>(instr.operand1)) {
                emitLine("    push(" + std::to_string(std::get<double>(instr.operand1)) + ")");
            } else {
                emitLine("    push(0.0)");
            }
            break;

        case IROpcode::PUSH_STRING:
            if (std::holds_alternative<std::string>(instr.operand1)) {
                emitLine("    push(" + escapeString(std::get<std::string>(instr.operand1)) + ")");
            } else {
                emitLine("    push('')");
            }
            break;

        default:
            break;
    }
}

void LuaCodeGenerator::emitArithmetic(const IRInstruction& instr) {
    // Use expression optimizer when possible
    if (canUseExpressionMode()) {
        switch (instr.opcode) {
            case IROpcode::ADD:
                m_exprOptimizer.applyBinaryOp(BinaryOp::ADD);
                return;
            case IROpcode::SUB:
                m_exprOptimizer.applyBinaryOp(BinaryOp::SUB);
                return;
            case IROpcode::MUL:
                m_exprOptimizer.applyBinaryOp(BinaryOp::MUL);
                return;
            case IROpcode::DIV:
                m_exprOptimizer.applyBinaryOp(BinaryOp::DIV);
                return;
            case IROpcode::IDIV:
                m_exprOptimizer.applyBinaryOp(BinaryOp::IDIV);
                return;
            case IROpcode::MOD:
                m_exprOptimizer.applyBinaryOp(BinaryOp::MOD);
                return;
            case IROpcode::POW:
                m_exprOptimizer.applyBinaryOp(BinaryOp::POW);
                return;
            case IROpcode::NEG:
                m_exprOptimizer.applyUnaryOp(UnaryOp::NEG);
                return;
            default:
                break;
        }
    }

    // Fallback to stack-based emission
    switch (instr.opcode) {
        case IROpcode::ADD:
            emitLine("    b = pop(); a = pop(); push(a + b)");
            break;
        case IROpcode::SUB:
            emitLine("    b = pop(); a = pop(); push(a - b)");
            break;
        case IROpcode::MUL:
            emitLine("    b = pop(); a = pop(); push(a * b)");
            break;
        case IROpcode::DIV:
            emitLine("    b = pop(); a = pop(); push(a / b)");
            break;
        case IROpcode::IDIV:
            emitLine("    b = pop(); a = pop(); push(math.floor(a / b))");
            break;
        case IROpcode::MOD:
            emitLine("    b = pop(); a = pop(); push(a % b)");
            break;
        case IROpcode::POW:
            emitLine("    b = pop(); a = pop(); push(a ^ b)");
            break;
        case IROpcode::NEG:
            emitLine("    push(-pop())");
            break;

        default:
            break;
    }
}

void LuaCodeGenerator::emitComparison(const IRInstruction& instr) {
    // Use expression optimizer when possible
    if (canUseExpressionMode()) {
        switch (instr.opcode) {
            case IROpcode::EQ:
                m_exprOptimizer.applyBinaryOp(BinaryOp::EQ);
                return;
            case IROpcode::NE:
                m_exprOptimizer.applyBinaryOp(BinaryOp::NE);
                return;
            case IROpcode::LT:
                m_exprOptimizer.applyBinaryOp(BinaryOp::LT);
                return;
            case IROpcode::LE:
                m_exprOptimizer.applyBinaryOp(BinaryOp::LE);
                return;
            case IROpcode::GT:
                m_exprOptimizer.applyBinaryOp(BinaryOp::GT);
                return;
            case IROpcode::GE:
                m_exprOptimizer.applyBinaryOp(BinaryOp::GE);
                return;
            default:
                break;
        }
    }

    // Fallback to stack-based emission
    // In Unicode mode, use unicode_string_equal and unicode_string_compare for proper comparison
    if (m_unicodeMode) {
        switch (instr.opcode) {
            case IROpcode::EQ:
                emitLine("    b = pop(); a = pop(); push((unicode_string_equal(a, b)) and -1 or 0)");
                break;
            case IROpcode::NE:
                emitLine("    b = pop(); a = pop(); push((not unicode_string_equal(a, b)) and -1 or 0)");
                break;
            case IROpcode::LT:
                emitLine("    b = pop(); a = pop(); push((unicode_string_compare(a, b) < 0) and -1 or 0)");
                break;
            case IROpcode::LE:
                emitLine("    b = pop(); a = pop(); push((unicode_string_compare(a, b) <= 0) and -1 or 0)");
                break;
            case IROpcode::GT:
                emitLine("    b = pop(); a = pop(); push((unicode_string_compare(a, b) > 0) and -1 or 0)");
                break;
            case IROpcode::GE:
                emitLine("    b = pop(); a = pop(); push((unicode_string_compare(a, b) >= 0) and -1 or 0)");
                break;
            default:
                break;
        }
    } else {
        switch (instr.opcode) {
            case IROpcode::EQ:
                emitLine("    b = pop(); a = pop(); push((a == b) and -1 or 0)");
                break;
            case IROpcode::NE:
                emitLine("    b = pop(); a = pop(); push((a ~= b) and -1 or 0)");
                break;
            case IROpcode::LT:
                emitLine("    b = pop(); a = pop(); push((a < b) and -1 or 0)");
                break;
            case IROpcode::LE:
                emitLine("    b = pop(); a = pop(); push((a <= b) and -1 or 0)");
                break;
            case IROpcode::GT:
                emitLine("    b = pop(); a = pop(); push((a > b) and -1 or 0)");
                break;
            case IROpcode::GE:
                emitLine("    b = pop(); a = pop(); push((a >= b) and -1 or 0)");
                break;
            default:
                break;
        }
    }
}

void LuaCodeGenerator::emitLogical(const IRInstruction& instr) {
    // Use expression optimizer when possible
    if (canUseExpressionMode()) {
        switch (instr.opcode) {
            case IROpcode::AND:
                m_exprOptimizer.applyBinaryOp(BinaryOp::AND);
                return;
            case IROpcode::OR:
                m_exprOptimizer.applyBinaryOp(BinaryOp::OR);
                return;
            case IROpcode::XOR:
                m_exprOptimizer.applyBinaryOp(BinaryOp::XOR);
                return;
            case IROpcode::EQV:
                m_exprOptimizer.applyBinaryOp(BinaryOp::EQV);
                return;
            case IROpcode::IMP:
                m_exprOptimizer.applyBinaryOp(BinaryOp::IMP);
                return;
            case IROpcode::NOT:
                m_exprOptimizer.applyUnaryOp(UnaryOp::NOT);
                return;
            default:
                break;
        }
    }

    // Fallback to stack-based emission
    // Use bitwise operations by default for BASIC compatibility
    switch (instr.opcode) {
        case IROpcode::AND:
            emitLine("    b = pop(); a = pop(); push(bitwise.band(a, b))");
            break;
        case IROpcode::OR:
            emitLine("    b = pop(); a = pop(); push(bitwise.bor(a, b))");
            break;
        case IROpcode::XOR:
            emitLine("    b = pop(); a = pop(); push(bitwise.bxor(a, b))");
            break;
        case IROpcode::EQV:
            emitLine("    b = pop(); a = pop(); push(bitwise.beqv(a, b))");
            break;
        case IROpcode::IMP:
            emitLine("    b = pop(); a = pop(); push(bitwise.bimp(a, b))");
            break;
        case IROpcode::NOT:
            emitLine("    push(bitwise.bnot(pop()))");
            break;
        default:
            break;
    }
}

void LuaCodeGenerator::emitVariable(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;

    std::string varName = std::get<std::string>(instr.operand1);
    std::string luaVarName = getVarName(varName);

    // Register variable if not seen before
    if (m_variables.find(varName) == m_variables.end()) {
        m_variables[varName] = m_variables.size();
    }

    switch (instr.opcode) {
        case IROpcode::LOAD_VAR: {
            // Use expression optimizer when possible
            // Get the variable (using hot/cold reference)
            std::string varRef = m_config.useVariableCache ?
                                 getVariableReference(varName) : luaVarName;

            if (canUseExpressionMode()) {
                m_exprOptimizer.pushVariable(varRef);
            } else {
                emitLine("    push(" + varRef + ")");
            }
            break;
        }

        case IROpcode::MID_ASSIGN: {
            // MID$(var$, pos, len) = replacement$
            // Stack has: pos, len, replacement (top)
            std::string varName = std::get<std::string>(instr.operand1);
            std::string varRef = m_config.useVariableCache ?
                getVariableReference(varName) : getVarName(varName);

            if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
                // Pop replacement, len, pos from expression optimizer
                auto replacement = m_exprOptimizer.pop();
                auto len = m_exprOptimizer.pop();
                auto pos = m_exprOptimizer.pop();

                if (replacement && len && pos) {
                    std::string replacementStr = m_exprOptimizer.toString(replacement);
                    std::string lenStr = m_exprOptimizer.toString(len);
                    std::string posStr = m_exprOptimizer.toString(pos);

                    emitLine("    " + varRef + " = basic_mid_assign(" + varRef + ", " +
                             posStr + ", " + lenStr + ", " + replacementStr + ")");
                } else {
                    flushExpressionToStack();
                    emitLine("    " + varRef + " = basic_mid_assign(" + varRef + ", pop(), pop(), pop())");
                }
            } else {
                flushExpressionToStack();
                emitLine("    " + varRef + " = basic_mid_assign(" + varRef + ", pop(), pop(), pop())");
            }
            break;
        }

        case IROpcode::STORE_VAR: {
            // Store the value from the stack (using hot/cold reference)
            std::string varRef = m_config.useVariableCache ?
                                 getVariableReference(varName) : luaVarName;

            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto expr = m_exprOptimizer.pop();
                if (expr) {
                    std::string exprCode = m_exprOptimizer.toString(expr);
                    emitLine("    " + varRef + " = " + exprCode);
                } else {
                    emitLine("    " + varRef + " = pop()");
                }
            } else {
                emitLine("    " + varRef + " = pop()");
            }
            break;
        }

        default:
            emitLine("    -- Unknown variable opcode: " + std::string(opcodeToString(instr.opcode)));
            break;
    }
}

void LuaCodeGenerator::emitConstant(const IRInstruction& instr) {
    int index = std::get<int>(instr.operand1);

    // Inline constant values if we have access to the constants manager
    if (m_constantsManager && m_config.inlineConstants) {
        ConstantValue value = m_constantsManager->getConstant(index);

        std::string literalValue;
        if (std::holds_alternative<int64_t>(value)) {
            literalValue = std::to_string(std::get<int64_t>(value));
        } else if (std::holds_alternative<double>(value)) {
            double dval = std::get<double>(value);
            literalValue = std::to_string(dval);
        } else if (std::holds_alternative<std::string>(value)) {
            literalValue = "\"" + escapeString(std::get<std::string>(value)) + "\"";
        }

        if (canUseExpressionMode()) {
            m_exprOptimizer.pushLiteral(literalValue);
        } else {
            emitLine("    push(" + literalValue + ")");
        }
    } else {
        // Fallback to runtime lookup if constants manager is not available
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushLiteral("constants_get(" + std::to_string(index) + ")");
        } else {
            emitLine("    push(constants_get(" + std::to_string(index) + "))");
        }
    }
}

void LuaCodeGenerator::emitArray(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;

    std::string arrayName = std::get<std::string>(instr.operand1);
    std::string luaArrayName = getArrayName(arrayName);
    std::string typeSuffix = instr.arrayElementTypeSuffix;

    // Register array if not seen before
    if (m_arrays.find(arrayName) == m_arrays.end()) {
        m_arrays[arrayName] = m_arrays.size();

        // Initialize array info with FFI detection
        ArrayInfo info;
        info.name = arrayName;
        info.typeSuffix = typeSuffix;
        info.luaVarName = luaArrayName;
        
        // Determine if this array should use FFI
        // FFI is beneficial for numeric arrays but not for string arrays
        info.usesFFI = (typeSuffix != "$"); // String arrays use Lua tables
        
        m_arrayInfo[arrayName] = info;
    }

    switch (instr.opcode) {
        case IROpcode::REDIM_ARRAY:
        case IROpcode::ERASE_ARRAY:
            // These are handled as statements, not expressions
            // They modify arrays but don't produce values
            flushExpressionToStack();
            break;

        case IROpcode::DIM_ARRAY: {
            // DIM is a side-effecting operation that modifies global state
            // We must flush expressions to maintain correct evaluation order
            flushExpressionToStack();

            // Pop dimension(s) and initialize array
            int dims = 1;
            if (std::holds_alternative<int>(instr.operand2)) {
                dims = std::get<int>(instr.operand2);
            }

            if (dims == 1) {
                emitLine("    dim = pop()");

                // Check if this is a user-defined type array
                if (!instr.userDefinedType.empty()) {
                    // Array of user-defined types - initialize each element with constructor
                    std::string constructorName = instr.userDefinedType + "_new";
                    emitLine("    " + luaArrayName + " = {}");
                    if (m_arrayBase == 0) {
                        emitLine("    for i = 0, dim do " + luaArrayName + "[i + 1] = " + constructorName + "() end");
                    } else {
                        emitLine("    for i = 1, dim + 1 do " + luaArrayName + "[i] = " + constructorName + "() end");
                    }
                } else {
                    // Standard array allocation
                    // Check if we should use FFI for this array
                    bool shouldUseFFI = m_arrayInfo[arrayName].usesFFI;
                
                    if (shouldUseFFI) {
                    // Try FFI allocation first, with Lua table fallback
                    emitLine("    -- Try FFI allocation for performance");
                    emitLine("    local ffi_array = create_ffi_array(dim + 1, detect_array_type('" + typeSuffix + "'))");
                    emitLine("    if ffi_array then");
                    emitLine("        " + luaArrayName + " = ffi_array");
                    emitLine("        -- Initialize FFI array to zero");
                    emitLine("        for i = 0, dim do");
                    emitLine("            " + luaArrayName + ".data[i] = 0");
                    emitLine("        end");
                    emitLine("    else");
                    emitLine("        -- Fallback to Lua table");
                    emitLine("        " + luaArrayName + " = {}");
                    std::string initValue = (typeSuffix == "$") ? "\"\"" : "0";
                    if (m_arrayBase == 0) {
                        emitLine("        for i = 0, dim do " + luaArrayName + "[i + 1] = " + initValue + " end");
                        } else {
                            emitLine("        for i = 1, dim + 1 do " + luaArrayName + "[i] = " + initValue + " end");
                        }
                        emitLine("    end");
                    } else {
                        // Use Lua table for string arrays or when FFI is disabled
                        emitLine("    " + luaArrayName + " = {}");
                        std::string initValue = (typeSuffix == "$") ? "\"\"" : "0";
                        if (m_arrayBase == 0) {
                            emitLine("    for i = 0, dim do " + luaArrayName + "[i + 1] = " + initValue + " end");
                        } else {
                            emitLine("    for i = 1, dim + 1 do " + luaArrayName + "[i] = " + initValue + " end");
                        }
                    }
                }
            } else {
                // Multi-dimensional arrays - pop dimensions in reverse order and initialize nested tables
                // Pop all dimensions from stack (they were pushed in order, so pop in reverse)
                for (int i = dims - 1; i >= 0; i--) {
                    emitLine("    dim" + std::to_string(i) + " = pop()");
                }

                // Initialize the multi-dimensional array
                emitLine("    " + luaArrayName + " = {}");

                // Generate nested initialization loops
                // Multi-dimensional arrays use direct BASIC indexing (0-based or 1-based) in Lua
                // No conversion needed - Lua tables can handle any integer index
                std::string indent = "    ";
                for (int d = 0; d < dims; d++) {
                    std::string loopVar = "i" + std::to_string(d);
                    int startIdx = m_arrayBase;
                    emitLine(indent + "for " + loopVar + " = " + std::to_string(startIdx) + ", " + std::to_string(startIdx) + " + dim" + std::to_string(d) + " do");
                    indent += "  ";
                    if (d < dims - 1) {
                        // Not the last dimension - create nested table
                        std::string tableAccess = luaArrayName;
                        for (int k = 0; k <= d; k++) {
                            tableAccess += "[i" + std::to_string(k) + "]";
                        }
                        emitLine(indent + "if not " + tableAccess + " then " + tableAccess + " = {} end");
                    } else {
                        // Last dimension - initialize based on type
                        std::string initValue;
                        if (!instr.userDefinedType.empty()) {
                            // User-defined type - call constructor
                            initValue = instr.userDefinedType + "_new()";
                        } else if (typeSuffix == "$") {
                            initValue = "\"\"";
                        } else {
                            initValue = "0";
                        }
                        std::string tableAccess = luaArrayName;
                        for (int k = 0; k <= d; k++) {
                            tableAccess += "[i" + std::to_string(k) + "]";
                        }
                        emitLine(indent + tableAccess + " = " + initValue);
                    }
                }
                // Close all loops
                for (int d = 0; d < dims; d++) {
                    indent = indent.substr(0, indent.length() - 2);
                    emitLine(indent + "end");
                }
            }
            break;
        }

        case IROpcode::LOAD_ARRAY: {
            // Get number of dimensions
            int dims = 1;
            if (std::holds_alternative<int>(instr.operand2)) {
                dims = std::get<int>(instr.operand2);
            }

            if (dims == 1) {
                // 1D array with FFI support
                bool mayUseFFI = m_arrayInfo.count(arrayName) && m_arrayInfo[arrayName].usesFFI;
                
                if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                    // Get the index expression
                    auto indexExpr = m_exprOptimizer.pop();
                    if (indexExpr) {
                        if (mayUseFFI) {
                            // Generate FFI-aware array access expression
                            std::string indexCode = m_exprOptimizer.toString(indexExpr);
                            std::string accessExpr = "(" + luaArrayName + ".data and " + 
                                                   luaArrayName + ".data[" + indexCode + "] or " +
                                                   luaArrayName + "[" + (m_arrayBase == 0 ? indexCode + " + 1" : indexCode) + "] or 0)";
                            m_exprOptimizer.pushVariable(accessExpr);
                        } else {
                            // Regular Lua table access
                            if (m_arrayBase == 0) {
                                auto oneLiteral = Expr::makeLiteral("1");
                                auto adjustedIndex = Expr::makeBinaryOp(BinaryOp::ADD, indexExpr, oneLiteral);
                                m_exprOptimizer.pushArrayAccess(luaArrayName, adjustedIndex);
                            } else {
                                m_exprOptimizer.pushArrayAccess(luaArrayName, indexExpr);
                            }
                        }
                    } else {
                        // Fallback to stack operations
                        emitLine("    idx = pop()");
                        if (mayUseFFI) {
                            emitLine("    if " + luaArrayName + ".data then");
                            emitLine("        push(" + luaArrayName + ".data[idx] or 0)");
                            emitLine("    else");
                            if (m_arrayBase == 0) {
                                emitLine("        push(" + luaArrayName + "[idx + 1] or 0)");
                            } else {
                                emitLine("        push(" + luaArrayName + "[idx] or 0)");
                            }
                            emitLine("    end");
                        } else {
                            if (m_arrayBase == 0) {
                                emitLine("    push(" + luaArrayName + "[idx + 1] or 0)");
                            } else {
                                emitLine("    push(" + luaArrayName + "[idx] or 0)");
                            }
                        }
                    }
                } else {
                    emitLine("    idx = pop()");
                    if (mayUseFFI) {
                        emitLine("    if " + luaArrayName + ".data then");
                        emitLine("        push(" + luaArrayName + ".data[idx] or 0)");
                        emitLine("    else");
                        if (m_arrayBase == 0) {
                            emitLine("        push(" + luaArrayName + "[idx + 1] or 0)");
                        } else {
                            emitLine("        push(" + luaArrayName + "[idx] or 0)");
                        }
                        emitLine("    end");
                    } else {
                        if (m_arrayBase == 0) {
                            emitLine("    push(" + luaArrayName + "[idx + 1] or 0)");
                        } else {
                            emitLine("    push(" + luaArrayName + "[idx] or 0)");
                        }
                    }
                }
            } else {
                // Multi-dimensional array - try to preserve expressions for indices
                if (canUseExpressionMode() && m_exprOptimizer.size() >= dims) {
                    // Pop indices in reverse order and convert to strings
                    std::vector<std::string> indexExprs;
                    for (int i = 0; i < dims; i++) {
                        auto indexExpr = m_exprOptimizer.pop();
                        if (indexExpr) {
                            indexExprs.insert(indexExprs.begin(), m_exprOptimizer.toString(indexExpr));
                        } else {
                            // Fallback if expression is invalid
                            flushExpressionToStack();
                            for (int j = dims - 1; j >= 0; j--) {
                                emitLine("    idx" + std::to_string(j) + " = pop()");
                            }
                            goto multidim_fallback;
                        }
                    }
                    
                    // Build direct access expression
                    std::string access = luaArrayName;
                    for (int i = 0; i < dims; i++) {
                        access += "[" + indexExprs[i] + "]";
                    }
                    
                    // Multi-dimensional arrays currently use Lua tables (no FFI support yet)
                    m_exprOptimizer.pushVariable("(" + access + " or 0)");
                } else {
                    multidim_fallback:
                    flushExpressionToStack();
                    
                    // Pop all indices in reverse order (they were pushed in order)
                    for (int i = dims - 1; i >= 0; i--) {
                        emitLine("    idx" + std::to_string(i) + " = pop()");
                    }

                    // Build nested table access
                    std::string access = luaArrayName;
                    for (int i = 0; i < dims; i++) {
                        access += "[idx" + std::to_string(i) + "]";
                    }

                    emitLine("    push(" + access + " or 0)");
                }
            }
            break;
        }

        case IROpcode::STORE_ARRAY: {
            // Get number of dimensions
            int dims = 1;
            if (std::holds_alternative<int>(instr.operand2)) {
                dims = std::get<int>(instr.operand2);
            }

            // Check if this array uses FFI (only for 1D arrays)
            bool mayUseFFI = (dims == 1) && m_arrayInfo.count(arrayName) && m_arrayInfo[arrayName].usesFFI;

            if (dims == 1) {
                // 1D array with enhanced FFI support
                // Stack has: [..., value, index] (index on top)
                // IR generator pushes value first, then index
                // Pop index first, then value
                if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                    // Pop in correct order: index (top), then value
                    auto indexExpr = m_exprOptimizer.pop();
                    auto valueExpr = m_exprOptimizer.pop();

                    if (valueExpr && indexExpr) {
                        std::string indexCode = m_exprOptimizer.toString(indexExpr);
                        std::string valueCode = m_exprOptimizer.toString(valueExpr);

                        if (mayUseFFI) {
                            // Enhanced FFI-aware assignment with type checking
                            emitLine("    if " + luaArrayName + ".data then");
                            emitLine("        " + luaArrayName + ".data[" + indexCode + "] = " + valueCode);
                            emitLine("    else");
                            if (m_arrayBase == 0) {
                                emitLine("        " + luaArrayName + "[" + indexCode + " + 1] = " + valueCode);
                            } else {
                                emitLine("        " + luaArrayName + "[" + indexCode + "] = " + valueCode);
                            }
                            emitLine("    end");
                        } else {
                            // Regular Lua table store
                            if (m_arrayBase == 0) {
                                auto oneLiteral = Expr::makeLiteral("1");
                                auto adjustedIndex = Expr::makeBinaryOp(BinaryOp::ADD, indexExpr, oneLiteral);
                                std::string adjustedIndexCode = m_exprOptimizer.toString(adjustedIndex);
                                emitLine("    " + luaArrayName + "[" + adjustedIndexCode + "] = " + valueCode);
                            } else {
                                emitLine("    " + luaArrayName + "[" + indexCode + "] = " + valueCode);
                            }
                        }
                    } else {
                        // Fallback to stack operations
                        emitLine("    idx = pop()");
                        emitLine("    val = pop()");
                        if (mayUseFFI) {
                            emitLine("    if " + luaArrayName + ".data then");
                            emitLine("        " + luaArrayName + ".data[idx] = val");
                            emitLine("    else");
                            if (m_arrayBase == 0) {
                                emitLine("        " + luaArrayName + "[idx + 1] = val");
                            } else {
                                emitLine("        " + luaArrayName + "[idx] = val");
                            }
                            emitLine("    end");
                        } else {
                            if (m_arrayBase == 0) {
                                emitLine("    " + luaArrayName + "[idx + 1] = val");
                            } else {
                                emitLine("    " + luaArrayName + "[idx] = val");
                            }
                        }
                    }
                } else {
                    emitLine("    idx = pop()");
                    emitLine("    val = pop()");
                    if (mayUseFFI) {
                        emitLine("    if " + luaArrayName + ".data then");
                        emitLine("        " + luaArrayName + ".data[idx] = val");
                        emitLine("    else");
                        if (m_arrayBase == 0) {
                            emitLine("        " + luaArrayName + "[idx + 1] = val");
                        } else {
                            emitLine("        " + luaArrayName + "[idx] = val");
                        }
                        emitLine("    end");
                    } else {
                        if (m_arrayBase == 0) {
                            emitLine("    " + luaArrayName + "[idx + 1] = val");
                        } else {
                            emitLine("    " + luaArrayName + "[idx] = val");
                        }
                    }
                }
            } else {
                // Multi-dimensional array assignment - try to preserve expressions
                if (canUseExpressionMode() && m_exprOptimizer.size() >= dims + 1) {
                    // Pop indices and value, keeping expressions
                    std::vector<std::string> indexExprs;
                    for (int i = 0; i < dims; i++) {
                        auto indexExpr = m_exprOptimizer.pop();
                        if (indexExpr) {
                            indexExprs.insert(indexExprs.begin(), m_exprOptimizer.toString(indexExpr));
                        } else {
                            // Fallback to stack operations
                            flushExpressionToStack();
                            goto multidim_assign_fallback;
                        }
                    }
                    
                    auto valueExpr = m_exprOptimizer.pop();
                    if (valueExpr) {
                        std::string valueCode = m_exprOptimizer.toString(valueExpr);
                        
                        // Build direct assignment
                        std::string access = luaArrayName;
                        for (int i = 0; i < dims; i++) {
                            access += "[" + indexExprs[i] + "]";
                        }
                        
                        emitLine("    " + access + " = " + valueCode);
                    } else {
                        flushExpressionToStack();
                        goto multidim_assign_fallback;
                    }
                } else {
                    multidim_assign_fallback:
                    flushExpressionToStack();

                    // Stack has: [..., value, idx0, idx1, ..., idxN-1]
                    // IR generator pushes: value first, then indices in order
                    // So pop indices in reverse order first, then value
                    for (int i = dims - 1; i >= 0; i--) {
                        emitLine("    idx" + std::to_string(i) + " = pop()");
                    }

                    // Pop value last
                    emitLine("    val = pop()");

                    // Build nested table access
                    std::string access = luaArrayName;
                    for (int i = 0; i < dims; i++) {
                        access += "[idx" + std::to_string(i) + "]";
                    }

                    emitLine("    " + access + " = val");
                }
            }
            break;
        }

        case IROpcode::FILL_ARRAY: {
            // Fill all elements of array with a scalar value
            // Stack has: value
            flushExpressionToStack();
            
            // Pop the value to fill with
            emitLine("    val = pop()");
            
            // Check if this is an FFI array or Lua table
            bool mayUseFFI = m_arrayInfo.count(arrayName) && m_arrayInfo[arrayName].usesFFI;
            
            if (mayUseFFI) {
                // Handle both FFI and Lua table cases
                emitLine("    if " + luaArrayName + ".data then");
                emitLine("        -- FFI array");
                emitLine("        for i = 0, " + luaArrayName + ".size - 1 do");
                emitLine("            " + luaArrayName + ".data[i] = val");
                emitLine("        end");
                emitLine("    else");
                emitLine("        -- Lua table (fill all entries regardless of OPTION BASE)");
                emitLine("        for i = 1, #" + luaArrayName + " do");
                emitLine("            " + luaArrayName + "[i] = val");
                emitLine("        end");
                emitLine("    end");
            } else {
                // Pure Lua table (fill all entries regardless of OPTION BASE)
                emitLine("    for i = 1, #" + luaArrayName + " do");
                emitLine("        " + luaArrayName + "[i] = val");
                emitLine("    end");
            }
            break;
        }

        case IROpcode::ARRAY_ADD:
        case IROpcode::ARRAY_SUB:
        case IROpcode::ARRAY_MUL:
        case IROpcode::ARRAY_DIV: {
            // Element-wise array operations: result() = a() op b()
            flushExpressionToStack();
            
            std::string resultArray = luaArrayName;
            std::string arrayA = getArrayName(std::get<std::string>(instr.operand2));
            std::string arrayB = getArrayName(std::get<std::string>(instr.operand3));
            
            // Determine operation
            std::string op;
            if (instr.opcode == IROpcode::ARRAY_ADD) op = "+";
            else if (instr.opcode == IROpcode::ARRAY_SUB) op = "-";
            else if (instr.opcode == IROpcode::ARRAY_MUL) op = "*";
            else if (instr.opcode == IROpcode::ARRAY_DIV) op = "/";
            
            // Check if arrays use FFI
            bool resultFFI = m_arrayInfo.count(arrayName) && m_arrayInfo[arrayName].usesFFI;
            bool aFFI = m_arrayInfo.count(std::get<std::string>(instr.operand2)) && 
                       m_arrayInfo[std::get<std::string>(instr.operand2)].usesFFI;
            bool bFFI = m_arrayInfo.count(std::get<std::string>(instr.operand3)) && 
                       m_arrayInfo[std::get<std::string>(instr.operand3)].usesFFI;
            
            // Generate element-wise loop
            if (resultFFI && aFFI && bFFI) {
                // All FFI arrays
                emitLine("    if " + resultArray + ".data and " + arrayA + ".data and " + arrayB + ".data then");
                emitLine("        local size = math.min(" + resultArray + ".size, " + arrayA + ".size, " + arrayB + ".size)");
                emitLine("        for i = 0, size - 1 do");
                emitLine("            " + resultArray + ".data[i] = " + arrayA + ".data[i] " + op + " " + arrayB + ".data[i]");
                emitLine("        end");
                emitLine("    else");
                emitLine("        -- Lua table fallback");
                emitLine("        local size = math.min(#" + resultArray + ", #" + arrayA + ", #" + arrayB + ")");
                emitLine("        for i = 1, size do");
                emitLine("            " + resultArray + "[i] = (" + arrayA + "[i] or 0) " + op + " (" + arrayB + "[i] or 0)");
                emitLine("        end");
                emitLine("    end");
            } else {
                // Lua tables
                emitLine("    local size = math.min(#" + resultArray + ", #" + arrayA + ", #" + arrayB + ")");
                emitLine("    for i = 1, size do");
                emitLine("        " + resultArray + "[i] = (" + arrayA + "[i] or 0) " + op + " (" + arrayB + "[i] or 0)");
                emitLine("    end");
            }
            break;
        }

        case IROpcode::ARRAY_ADD_SCALAR:
        case IROpcode::ARRAY_SUB_SCALAR:
        case IROpcode::ARRAY_MUL_SCALAR:
        case IROpcode::ARRAY_DIV_SCALAR: {
            // Element-wise array-scalar operations: result() = a() op scalar
            flushExpressionToStack();
            
            emitLine("    scalar = pop()");
            
            std::string resultArray = luaArrayName;
            std::string arrayA = getArrayName(std::get<std::string>(instr.operand2));
            
            // Determine operation
            std::string op;
            if (instr.opcode == IROpcode::ARRAY_ADD_SCALAR) op = "+";
            else if (instr.opcode == IROpcode::ARRAY_SUB_SCALAR) op = "-";
            else if (instr.opcode == IROpcode::ARRAY_MUL_SCALAR) op = "*";
            else if (instr.opcode == IROpcode::ARRAY_DIV_SCALAR) op = "/";
            
            // Check if arrays use FFI
            bool resultFFI = m_arrayInfo.count(arrayName) && m_arrayInfo[arrayName].usesFFI;
            bool aFFI = m_arrayInfo.count(std::get<std::string>(instr.operand2)) && 
                       m_arrayInfo[std::get<std::string>(instr.operand2)].usesFFI;
            
            // Generate element-wise loop
            if (resultFFI && aFFI) {
                // FFI arrays
                emitLine("    if " + resultArray + ".data and " + arrayA + ".data then");
                emitLine("        local size = math.min(" + resultArray + ".size, " + arrayA + ".size)");
                emitLine("        for i = 0, size - 1 do");
                emitLine("            " + resultArray + ".data[i] = " + arrayA + ".data[i] " + op + " scalar");
                emitLine("        end");
                emitLine("    else");
                emitLine("        -- Lua table fallback");
                emitLine("        local size = math.min(#" + resultArray + ", #" + arrayA + ")");
                emitLine("        for i = 1, size do");
                emitLine("            " + resultArray + "[i] = (" + arrayA + "[i] or 0) " + op + " scalar");
                emitLine("        end");
                emitLine("    end");
            } else {
                // Lua tables
                emitLine("    local size = math.min(#" + resultArray + ", #" + arrayA + ")");
                emitLine("    for i = 1, size do");
                emitLine("        " + resultArray + "[i] = (" + arrayA + "[i] or 0) " + op + " scalar");
                emitLine("    end");
            }
            break;
        }

        default:
            break;
    }
}

void LuaCodeGenerator::emitControlFlow(const IRInstruction& instr, size_t index) {
    std::string labelStr;

    // Smart flush for conditional jumps - only flush if we can't use expressions directly
    if (instr.opcode == IROpcode::JUMP_IF_FALSE ||
        instr.opcode == IROpcode::JUMP_IF_TRUE) {
        // If we have exactly one expression, we can use it directly for the condition
        if (m_exprOptimizer.size() != 1) {
            flushExpressionToStack();
        }
    }

    switch (instr.opcode) {
        case IROpcode::LABEL:
            if (std::holds_alternative<std::string>(instr.operand1)) {
                labelStr = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                labelStr = std::to_string(std::get<int>(instr.operand1));
            }

            // Check if this is the loop-back label for a native FOR loop
            if (!m_forLoopStack.empty()) {
                auto& loopInfo = m_forLoopStack.back();
                // Check if we already emitted the native loop (from FOR_INIT)
                if (loopInfo.canUseNativeLoop && loopInfo.nativeLoopEmitted) {
                    // Check if this is specifically the loop-back label (matches the FOR_NEXT label)
                    // Only skip emitting if it's the actual loop continuation point
                    // We need to emit GOTO target labels that happen to be inside the loop
                    if (labelStr == loopInfo.loopBackLabel || loopInfo.loopBackLabel.empty()) {
                        loopInfo.loopBackLabel = labelStr; // Save for later
                        // Don't emit the loop-back label - we're using native Lua for loop
                        break;
                    }
                    // This is a different label (GOTO target) inside the loop - emit it
                }
            }

            if (!labelStr.empty()) {
                emitLabel(labelStr);
            }
            break;

        case IROpcode::JUMP:
            // Skip unreachable JUMP after RETURN
            if (m_lastEmittedOpcode == IROpcode::RETURN_VALUE ||
                m_lastEmittedOpcode == IROpcode::RETURN_GOSUB) {
                // Unreachable code after return - skip it entirely
                if (m_config.emitComments) {
                    emitComment("Skipping unreachable JUMP after RETURN");
                }
                break;
            }

            if (std::holds_alternative<std::string>(instr.operand1)) {
                labelStr = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                labelStr = std::to_string(std::get<int>(instr.operand1));
            }
            if (!labelStr.empty()) {
                // If this is a loop back edge, yield to allow Control+C checks
                if (instr.isLoopJump) {
                    if (m_config.emitComments) {
                        emitComment("Yield for Control+C check (loop back edge)");
                    }
                    emitLine("    if coroutine.running() then coroutine.yield('loop_check') end");
                }
                emitLine("    goto " + getLabelName(labelStr));
            }
            break;

        case IROpcode::JUMP_IF_FALSE:
            if (std::holds_alternative<std::string>(instr.operand1)) {
                labelStr = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                labelStr = std::to_string(std::get<int>(instr.operand1));
            }
            if (!labelStr.empty()) {
                // Use expression optimizer for condition if available
                if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                    auto condExpr = m_exprOptimizer.pop();
                    if (condExpr) {
                        std::string condCode = m_exprOptimizer.toString(condExpr);
                        emitLine("    if not basicBoolToLua(" + condCode + ") then goto " + getLabelName(labelStr) + " end");
                    } else {
                        emitLine("    if not basicBoolToLua(pop()) then goto " + getLabelName(labelStr) + " end");
                    }
                } else {
                    emitLine("    if not basicBoolToLua(pop()) then goto " + getLabelName(labelStr) + " end");
                }
            }
            break;

        case IROpcode::JUMP_IF_TRUE:
            if (std::holds_alternative<std::string>(instr.operand1)) {
                labelStr = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                labelStr = std::to_string(std::get<int>(instr.operand1));
            }
            if (!labelStr.empty()) {
                // Use expression optimizer for condition if available
                if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                    auto condExpr = m_exprOptimizer.pop();
                    if (condExpr) {
                        std::string condCode = m_exprOptimizer.toString(condExpr);
                        emitLine("    if basicBoolToLua(" + condCode + ") then goto " + getLabelName(labelStr) + " end");
                    } else {
                        emitLine("    if basicBoolToLua(pop()) then goto " + getLabelName(labelStr) + " end");
                    }
                } else {
                    emitLine("    if basicBoolToLua(pop()) then goto " + getLabelName(labelStr) + " end");
                }
            }
            break;

        case IROpcode::CALL_GOSUB: {
            // Implement GOSUB as a function call
            if (std::holds_alternative<std::string>(instr.operand1)) {
                labelStr = std::get<std::string>(instr.operand1);
            } else if (std::holds_alternative<int>(instr.operand1)) {
                labelStr = std::to_string(std::get<int>(instr.operand1));
            }
            if (!labelStr.empty()) {
                emitLine("    _gosub." + getLabelName(labelStr) + "()");
            }
            break;
        }

        case IROpcode::RETURN_GOSUB: {
            // RETURN is handled inside the function itself - skip here
            // (already emitted as 'return' in the function body)
            break;
        }

        case IROpcode::ON_GOTO: {
            // Implement ON GOTO - computed goto based on selector value
            // Try to use expression for selector if we have exactly one
            if (canUseExpressionMode() && m_exprOptimizer.size() == 1) {
                auto selectorExpr = m_exprOptimizer.pop();
                if (selectorExpr) {
                    std::string selectorCode = m_exprOptimizer.toString(selectorExpr);
                    
                    // Parse comma-separated label IDs from operand
                    std::string targets;
                    if (std::holds_alternative<std::string>(instr.operand1)) {
                        targets = std::get<std::string>(instr.operand1);
                    }

                    if (!targets.empty()) {
                        // Split targets and emit computed goto using expression
                        std::vector<std::string> labels;
                        std::stringstream ss(targets);
                        std::string label;
                        while (std::getline(ss, label, ',')) {
                            labels.push_back(label);
                        }

                        if (!labels.empty()) {
                            emitLine("    local selector = " + selectorCode);
                            for (size_t i = 0; i < labels.size(); i++) {
                                std::string condition = (i == 0) ? "if" : "elseif";
                                emitLine("    " + condition + " selector == " + std::to_string(i + 1) + " then");
                                emitLine("        goto " + getLabelName(labels[i]));
                            }
                            if (!labels.empty()) {
                                emitLine("    end");
                            }
                            break;
                        }
                    }
                }
            }
            
            // Fallback to stack-based approach
            flushExpressionToStack();

            // Parse comma-separated label IDs from operand
            std::string targets;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                targets = std::get<std::string>(instr.operand1);
            }

            if (!targets.empty()) {
                // Split targets by comma
                std::vector<std::string> labelIds;
                size_t start = 0;
                size_t pos = 0;
                while ((pos = targets.find(',', start)) != std::string::npos) {
                    labelIds.push_back(targets.substr(start, pos - start));
                    start = pos + 1;
                }
                labelIds.push_back(targets.substr(start));

                // Store selector in a temporary variable without local declaration
                // to avoid Lua goto scope issues
                emitLine("    _on_temp = pop()");

                // Generate if-elseif chain
                for (size_t i = 0; i < labelIds.size(); i++) {
                    std::string labelName = getLabelName(labelIds[i]);
                    if (i == 0) {
                        emitLine("    if _on_temp == 1 then goto " + labelName);
                    } else {
                        emitLine("    elseif _on_temp == " + std::to_string(i + 1) + " then goto " + labelName);
                    }
                }
                emitLine("    end");
            }
            break;
        }

        case IROpcode::ON_GOSUB: {
            // Implement ON GOSUB - computed gosub based on selector value
            flushExpressionToStack();

            // Parse comma-separated label IDs from operand
            std::string targets;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                targets = std::get<std::string>(instr.operand1);
            }

            if (!targets.empty()) {
                std::vector<std::string> labelIds;
                size_t start = 0;
                size_t pos = targets.find(',');
                while (pos != std::string::npos) {
                    labelIds.push_back(targets.substr(start, pos - start));
                    start = pos + 1;
                    pos = targets.find(',', start);
                }
                labelIds.push_back(targets.substr(start));

                // Store selector in a temporary variable without local declaration
                emitLine("    _on_temp = pop()");

                // Generate if-elseif chain with function calls
                for (size_t i = 0; i < labelIds.size(); i++) {
                    std::string labelName = getLabelName(labelIds[i]);
                    if (i == 0) {
                        emitLine("    if _on_temp == 1 then");
                    } else {
                        emitLine("    elseif _on_temp == " + std::to_string(i + 1) + " then");
                    }
                    emitLine("        _gosub." + labelName + "()");
                }
                emitLine("    end");
            }
            break;
        }

        case IROpcode::ON_CALL: {
            // Implement ON CALL - computed function/sub call based on selector value
            // This is more efficient with table-based dispatch
            flushExpressionToStack();

            // Parse comma-separated function names from operand
            std::string targets;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                targets = std::get<std::string>(instr.operand1);
            }

            if (!targets.empty()) {
                // Split targets by comma
                std::vector<std::string> funcNames;
                size_t start = 0;
                size_t pos = 0;
                while ((pos = targets.find(',', start)) != std::string::npos) {
                    funcNames.push_back(targets.substr(start, pos - start));
                    start = pos + 1;
                }
                funcNames.push_back(targets.substr(start));

                // Store selector in temporary variable
                emitLine("    _on_temp = pop()");

                // Generate inline dispatch without local table (to avoid goto scope issues)
                for (size_t i = 0; i < funcNames.size(); i++) {
                    // Apply function name mangling (func_ prefix)
                    std::string funcName = "func_" + funcNames[i];
                    if (i == 0) {
                        emitLine("    if _on_temp == 1 then " + funcName + "()");
                    } else {
                        emitLine("    elseif _on_temp == " + std::to_string(i + 1) + " then " + funcName + "()");
                    }
                }
                if (!funcNames.empty()) {
                    emitLine("    end");
                }
            }
            break;
        }

        case IROpcode::IF_START: {
            // Begin structured IF block - condition is on stack
            // Use expression optimizer for condition if available
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string condCode = m_exprOptimizer.toString(condExpr);
                    emitLine("    if basicBoolToLua(" + condCode + ") then");
                } else {
                    emitLine("    if basicBoolToLua(pop()) then");
                }
            } else {
                emitLine("    if basicBoolToLua(pop()) then");
            }
            break;
        }

        case IROpcode::ELSEIF_START: {
            // Begin ELSEIF block - condition is on stack
            // Use expression optimizer for condition if available
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string condCode = m_exprOptimizer.toString(condExpr);
                    emitLine("    elseif basicBoolToLua(" + condCode + ") then");
                } else {
                    emitLine("    elseif basicBoolToLua(pop()) then");
                }
            } else {
                emitLine("    elseif basicBoolToLua(pop()) then");
            }
            break;
        }

        case IROpcode::ELSE_START: {
            // Begin ELSE block
            emitLine("    else");
            break;
        }

        case IROpcode::IF_END: {
            // End IF block
            emitLine("    end");
            break;
        }

        default:
            break;
    }
}

void LuaCodeGenerator::emitLoop(const IRInstruction& instr) {
    switch (instr.opcode) {
        case IROpcode::FOR_INIT: {
            // FOR loops need a variable name
            if (!std::holds_alternative<std::string>(instr.operand1)) {
                return;
            }

            std::string varName = std::get<std::string>(instr.operand1);
            std::string luaVarName = getVarName(varName);
            // Try to detect native loop opportunity
            bool canUseNative = false;
            std::string startExpr, endExpr, stepExpr;

            // Check if we have simple expressions in the expression optimizer
            if (m_exprOptimizer.size() == 3) {
                // We have three expressions: start, end, step
                // Pop them to check if they're simple
                auto stepE = m_exprOptimizer.pop();
                auto endE = m_exprOptimizer.pop();
                auto startE = m_exprOptimizer.pop();

                if (stepE && endE && startE) {
                    // Check if all are simple (literals or simple variables)
                    if (m_exprOptimizer.isSimple(stepE) &&
                        m_exprOptimizer.isSimple(endE) &&
                        m_exprOptimizer.isSimple(startE)) {
                        canUseNative = true;
                        startExpr = m_exprOptimizer.toString(startE);
                        endExpr = m_exprOptimizer.toString(endE);
                        stepExpr = m_exprOptimizer.toString(stepE);
                    } else {
                        // Not simple enough - put them back and flush
                        flushExpressionToStack();
                    }
                } else {
                    // Failed to pop - flush whatever we have
                    flushExpressionToStack();
                }
            } else {
                // Wrong number of expressions - flush to stack
                flushExpressionToStack();
            }

            // Save loop context
            ForLoopInfo info;
            info.varName = varName;
            info.canUseNativeLoop = canUseNative;
            info.startExpr = startExpr;
            info.endExpr = endExpr;
            info.stepExpr = stepExpr;
            info.nativeLoopEmitted = false;
            info.loopBodyStartIndex = -1;

            if (canUseNative) {
                // Emit native loop immediately (don't wait for LABEL - structured IFs have no labels!)
                std::string luaVarName = getVarName(varName);
                emitLine("    for " + luaVarName + " = " + startExpr + ", " +
                         endExpr + ", " + stepExpr + " do");
                info.nativeLoopEmitted = true;
                info.endValue = endExpr;      // Preserve for potential fallback
                info.stepValue = stepExpr;    // Preserve for potential fallback
                info.startAddress = -1;
            } else {
                // Stack-based loop - emit initialization using reusable variables
                emitLine("    for_step = pop()");
                emitLine("    for_end = pop()");
                emitLine("    " + luaVarName + " = pop()");

                info.endValue = "for_end";
                info.stepValue = "for_step";
                info.startAddress = -1;
            }

            m_forLoopStack.push_back(info);
            break;
        }

        case IROpcode::FOR_CHECK: {
            // Check if loop should continue
            if (m_forLoopStack.empty()) break;
            auto& loopInfo = m_forLoopStack.back();

            std::string luaVarName = getVarName(loopInfo.varName);

            std::string exitLabel;
            if (std::holds_alternative<std::string>(instr.operand2)) {
                exitLabel = std::get<std::string>(instr.operand2);
            } else if (std::holds_alternative<int>(instr.operand2)) {
                exitLabel = std::to_string(std::get<int>(instr.operand2));
            }

            emitLine("    if " + loopInfo.stepValue + " > 0 then");
            emitLine("        if " + luaVarName + " > " + loopInfo.endValue + " then");
            if (!exitLabel.empty()) {
                emitLine("            goto " + getLabelName(exitLabel));
            }
            emitLine("        end");
            emitLine("    else");
            emitLine("        if " + luaVarName + " < " + loopInfo.endValue + " then");
            if (!exitLabel.empty()) {
                emitLine("            goto " + getLabelName(exitLabel));
            }
            emitLine("        end");
            emitLine("    end");
            break;
        }

        case IROpcode::FOR_NEXT: {
            // Increment loop variable and check if done, then jump back
            if (m_forLoopStack.empty()) break;
            auto& loopInfo = m_forLoopStack.back();

            std::string luaVarName = getVarName(loopInfo.varName);

            if (loopInfo.canUseNativeLoop && loopInfo.nativeLoopEmitted) {
                // Close the native for loop
                emitLine("    end");
                m_forLoopStack.pop_back();
            } else {
                // Manual loop - emit increment and check
                emitLine("    " + luaVarName + " = " + luaVarName + " + " + loopInfo.stepValue);

                // Check if loop should continue (same logic as interpreter's isDone())
                emitLine("    done = false");
                emitLine("    if " + loopInfo.stepValue + " > 0 then");
                emitLine("        done = (" + luaVarName + " > " + loopInfo.endValue + ")");
                emitLine("    else");
                emitLine("        done = (" + luaVarName + " < " + loopInfo.endValue + ")");
                emitLine("    end");

                // Jump back to loop start if not done
                if (!loopInfo.loopBackLabel.empty()) {
                    emitLine("    if not done then goto " + getLabelName(loopInfo.loopBackLabel) + " end");
                }

                m_forLoopStack.pop_back();
            }
            break;
        }

        case IROpcode::FOR_IN_INIT: {
            // Initialize FOR...IN loop
            if (!std::holds_alternative<std::string>(instr.operand1)) {
                return;
            }

            std::string varName = std::get<std::string>(instr.operand1);
            std::string indexVarName = "";
            if (std::holds_alternative<std::string>(instr.operand2)) {
                indexVarName = std::get<std::string>(instr.operand2);
            }

            // Array should be on stack
            flushExpressionToStack();
            
            std::string luaVarName = getVarName(varName);
            std::string luaIndexVarName = indexVarName.empty() ? "" : getVarName(indexVarName);
            
            // Generate optimized FOR...IN loop leveraging our FFI arrays
            emitLine("    local for_in_array = pop()");
            emitLine("    local for_in_size = 0");
            emitLine("    local for_in_index = 0");
            emitLine("    ");
            emitLine("    -- Determine array size and access method");
            emitLine("    if for_in_array.data then");
            emitLine("        -- FFI array");
            emitLine("        for_in_size = for_in_array.size");
            emitLine("    elseif type(for_in_array) == 'table' then");
            emitLine("        -- Lua table array");
            emitLine("        for_in_size = #for_in_array");
            emitLine("    else");
            emitLine("        for_in_size = 0");
            emitLine("    end");
            emitLine("    ");
            
            // Save loop info
            ForInLoopInfo info;
            info.varName = varName;
            info.indexVarName = indexVarName;
            info.arrayName = "for_in_array";
            info.canUseNativeLoop = true;
            info.nativeLoopEmitted = false;
            m_forInLoopStack.push_back(info);
            break;
        }

        case IROpcode::FOR_IN_CHECK: {
            // Check if FOR...IN loop should continue
            if (m_forInLoopStack.empty()) break;
            auto& loopInfo = m_forInLoopStack.back();
            
            std::string exitLabel;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                exitLabel = std::get<std::string>(instr.operand1);
            }
            
            emitLine("    if for_in_index >= for_in_size then");
            if (!exitLabel.empty()) {
                emitLine("        goto " + getLabelName(exitLabel));
            }
            emitLine("    end");
            
            // Set loop variables
            std::string luaVarName = getVarName(loopInfo.varName);
            emitLine("    -- Get current element");
            emitLine("    if for_in_array.data then");
            emitLine("        " + luaVarName + " = for_in_array.data[for_in_index]");
            emitLine("    else");
            if (m_arrayBase == 0) {
                emitLine("        " + luaVarName + " = for_in_array[for_in_index + 1] or 0");
            } else {
                emitLine("        " + luaVarName + " = for_in_array[for_in_index] or 0");
            }
            emitLine("    end");
            
            // Set index variable if provided
            if (!loopInfo.indexVarName.empty()) {
                std::string luaIndexVarName = getVarName(loopInfo.indexVarName);
                emitLine("    " + luaIndexVarName + " = for_in_index");
            }
            
            break;
        }

        case IROpcode::FOR_IN_NEXT: {
            // Advance FOR...IN loop to next element
            if (m_forInLoopStack.empty()) break;
            auto& loopInfo = m_forInLoopStack.back();
            
            std::string loopLabel;
            if (std::holds_alternative<std::string>(instr.operand1)) {
                loopLabel = std::get<std::string>(instr.operand1);
            }
            
            // Increment index and jump back
            emitLine("    for_in_index = for_in_index + 1");
            if (!loopLabel.empty()) {
                emitLine("    goto " + getLabelName(loopLabel));
            }
            
            m_forInLoopStack.pop_back();
            break;
        }

        case IROpcode::WHILE_START: {
            // Begin WHILE loop with condition on stack or in optimizer
            // CRITICAL: We must re-evaluate the condition each iteration!
            
            // Check if operand1 contains a serialized expression string (for deferred evaluation)
            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string serializedExpr = std::get<std::string>(instr.operand1);
                if (!serializedExpr.empty()) {
                    // Use native Lua while loop with the serialized expression
                    // Lua will re-evaluate this expression each iteration automatically
                    // Use basicBoolToLua to convert BASIC boolean (0/-1) to Lua boolean
                    emitLine("    while basicBoolToLua(" + serializedExpr + ") do");
                    m_whileLoopStack.push_back({WhileLoopType::WITH_CONDITION});
                    break;
                }
            }
            
            // Fall back to stack-based evaluation with goto pattern
            // Get the loop start label from operand1 (added by IR generator)
            int loopLabel = -1;
            if (std::holds_alternative<int>(instr.operand1)) {
                loopLabel = std::get<int>(instr.operand1);
            }
            
            if (!m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    // Condition expression available - use native Lua while loop
                    // Lua will re-evaluate this expression each iteration automatically
                    // Use basicBoolToLua to convert BASIC boolean (0/-1) to Lua boolean
                    std::string cond = m_exprOptimizer.toString(condExpr);
                    emitLine("    while basicBoolToLua(" + cond + ") do");
                    m_whileLoopStack.push_back({WhileLoopType::WITH_CONDITION});
                } else {
                    // Optimizer returned null - must use goto pattern
                    // Check for BASIC false: 0, false, or nil (use helper to handle both int 0 and boolean false)
                    emitLine("    local __cond = pop()");
                    emitLine("    if __cond == 0 or __cond == false or __cond == nil then goto " + getLabelName(std::to_string(loopLabel)) + "_end end");
                    m_whileLoopStack.push_back({WhileLoopType::FROM_STACK});
                }
            } else {
                // Condition was pushed to stack - can't use native while loop
                // The IR emitted a LABEL before the condition, so we use goto pattern
                // to jump back and re-evaluate the condition code
                // Check for BASIC false: 0, false, or nil
                emitLine("    local __cond = pop()");
                emitLine("    if __cond == 0 or __cond == false or __cond == nil then goto " + getLabelName(std::to_string(loopLabel)) + "_end end");
                m_whileLoopStack.push_back({WhileLoopType::FROM_STACK});
            }
            break;
        }

        case IROpcode::WHILE_END: {
            // End WHILE loop
            if (m_whileLoopStack.empty()) {
                throw std::runtime_error("WHILE_END without matching WHILE_START");
            }
            
            WhileLoopInfo loopInfo = m_whileLoopStack.back();
            m_whileLoopStack.pop_back();
            
            if (loopInfo.type == WhileLoopType::WITH_CONDITION) {
                // Used native while loop - just close it
                emitLine("    end");
            } else {
                // Used goto pattern - need to jump back to label to re-evaluate condition
                int loopLabel = -1;
                if (std::holds_alternative<int>(instr.operand1)) {
                    loopLabel = std::get<int>(instr.operand1);
                }
                emitLine("    goto " + getLabelName(std::to_string(loopLabel)));
                emitLine("    ::" + getLabelName(std::to_string(loopLabel)) + "_end::");
            }
            break;
        }

        case IROpcode::REPEAT_START: {
            // Begin REPEAT loop
            emitLine("    repeat");
            break;
        }

        case IROpcode::REPEAT_END: {
            // End REPEAT loop with UNTIL condition on stack
            if (!m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string cond = m_exprOptimizer.toString(condExpr);
                    emitLine("    until basicBoolToLua(" + cond + ")");
                } else {
                    emitLine("    until basicBoolToLua(pop())");
                }
            } else {
                emitLine("    until basicBoolToLua(pop())");
            }
            break;
        }

        case IROpcode::DO_WHILE_START: {
            // DO WHILE (pre-test) - same as WHILE
            if (!m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string cond = m_exprOptimizer.toString(condExpr);
                    emitLine("    while basicBoolToLua(" + cond + ") do");
                } else {
                    emitLine("    while basicBoolToLua(pop()) do");
                }
            } else {
                emitLine("    while pop() ~= 0 do");
            }
            // Track that we're in a pre-test WHILE loop
            DoLoopInfo info;
            info.type = DoLoopType::PRE_TEST_WHILE;
            m_doLoopStack.push_back(info);
            break;
        }

        case IROpcode::DO_UNTIL_START: {
            // DO UNTIL (pre-test) - while NOT condition
            if (!m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string cond = m_exprOptimizer.toString(condExpr);
                    emitLine("    while not basicBoolToLua(" + cond + ") do");
                } else {
                    emitLine("    while not basicBoolToLua(pop()) do");
                }
            } else {
                emitLine("    while pop() == 0 do");
            }
            // Track that we're in a pre-test UNTIL loop
            DoLoopInfo info;
            info.type = DoLoopType::PRE_TEST_UNTIL;
            m_doLoopStack.push_back(info);
            break;
        }

        case IROpcode::DO_START: {
            // Plain DO - always emit 'repeat' since all post-test loops use it
            // For infinite loops, DO_LOOP_END will emit 'until false'
            emitLine("    repeat");
            // Track that we're in a post-test or infinite loop
            // We'll determine which when we see the LOOP opcode
            DoLoopInfo info;
            info.type = DoLoopType::INFINITE;  // Default to infinite, may be changed by LOOP_WHILE/UNTIL
            m_doLoopStack.push_back(info);
            break;
        }

        case IROpcode::DO_LOOP_WHILE: {
            // LOOP WHILE (post-test) - use repeat...until NOT condition
            if (!m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string cond = m_exprOptimizer.toString(condExpr);
                    emitLine("    until not basicBoolToLua(" + cond + ")");
                } else {
                    emitLine("    until not basicBoolToLua(pop())");
                }
            } else {
                emitLine("    until not basicBoolToLua(pop())");
            }
            // Mark the current loop as post-test
            if (!m_doLoopStack.empty()) {
                m_doLoopStack.back().type = DoLoopType::POST_TEST;
                m_doLoopStack.pop_back();
            }
            break;
        }

        case IROpcode::DO_LOOP_UNTIL: {
            // LOOP UNTIL (post-test)
            if (!m_exprOptimizer.isEmpty()) {
                auto condExpr = m_exprOptimizer.pop();
                if (condExpr) {
                    std::string cond = m_exprOptimizer.toString(condExpr);
                    emitLine("    until basicBoolToLua(" + cond + ")");
                } else {
                    emitLine("    until basicBoolToLua(pop())");
                }
            } else {
                emitLine("    until basicBoolToLua(pop())");
            }
            // Mark the current loop as post-test
            if (!m_doLoopStack.empty()) {
                m_doLoopStack.back().type = DoLoopType::POST_TEST;
                m_doLoopStack.pop_back();
            }
            break;
        }

        case IROpcode::DO_LOOP_END: {
            // Plain LOOP end - behavior depends on loop type
            if (!m_doLoopStack.empty()) {
                DoLoopInfo info = m_doLoopStack.back();
                m_doLoopStack.pop_back();

                if (info.type == DoLoopType::PRE_TEST_WHILE || info.type == DoLoopType::PRE_TEST_UNTIL) {
                    // Pre-test loop - close with 'end'
                    emitLine("    end");
                } else {
                    // Infinite loop (DO ... LOOP with no condition) - close with 'until false'
                    emitLine("    until false");
                }
            } else {
                // No loop info - default to 'until false' for safety
                emitLine("    until false");
            }
            break;
        }

        default:
            break;
    }
}

void LuaCodeGenerator::emitIO(const IRInstruction& instr) {
    switch (instr.opcode) {
        case IROpcode::PRINT:
            // Use expression optimizer for print if available
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto expr = m_exprOptimizer.pop();
                if (expr) {
                    std::string code = m_exprOptimizer.toString(expr);
                    emitLine("    basic_print(" + code + ")");
                } else {
                    emitLine("    basic_print(pop())");
                }
            } else {
                emitLine("    basic_print(pop())");
            }
            break;

        case IROpcode::PRINT_NEWLINE:
            emitLine("    basic_print_newline()");
            break;

        case IROpcode::PRINT_USING: {
            // PRINT USING: format string on stack, then N values
            int argCount = std::holds_alternative<int>(instr.operand1) ? std::get<int>(instr.operand1) : 0;

            if (canUseExpressionMode() && m_exprOptimizer.size() >= argCount + 1) {
                // Pop values in reverse order (they're on the stack)
                std::vector<std::string> values;
                for (int i = 0; i < argCount; i++) {
                    auto expr = m_exprOptimizer.pop();
                    if (expr) {
                        values.insert(values.begin(), m_exprOptimizer.toString(expr));
                    } else {
                        values.insert(values.begin(), "pop()");
                    }
                }

                // Pop format string
                auto formatExpr = m_exprOptimizer.pop();
                std::string formatStr;
                if (formatExpr) {
                    formatStr = m_exprOptimizer.toString(formatExpr);
                } else {
                    formatStr = "pop()";
                }

                // Emit call to basic_print_using
                std::string args = formatStr;
                for (const auto& val : values) {
                    args += ", " + val;
                }
                emitLine("    basic_print(basic_print_using(" + args + "))");
                emitLine("    basic_print_newline()");
            } else {
                // Fallback to stack-based
                std::string popVals;
                for (int i = 0; i < argCount; i++) {
                    if (i > 0) popVals = ", " + popVals;
                    popVals = "pop()" + popVals;
                }
                if (argCount > 0) {
                    emitLine("    basic_print(basic_print_using(pop(), " + popVals + "))");
                } else {
                    emitLine("    basic_print(basic_print_using(pop()))");
                }
                emitLine("    basic_print_newline()");
            }
            break;
        }

        case IROpcode::INPUT_PROMPT:
            // Print the prompt without newline
            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string prompt = std::get<std::string>(instr.operand1);
                emitLine("    io.write(" + escapeString(prompt) + ")");
            }
            break;

        case IROpcode::PRINT_AT: {
            // PRINT_AT: x, y, N text items, fg, bg on stack
            // Pop in reverse: bg, fg, then N text items, then y, x
            int itemCount = std::holds_alternative<int>(instr.operand1) ? std::get<int>(instr.operand1) : 0;

            flushExpressionToStack();

            // Pop colors
            emitLine("    local _bg = pop()");
            emitLine("    local _fg = pop()");

            // Pop and concatenate text items
            if (itemCount == 0) {
                emitLine("    local _text = \"\"");
            } else if (itemCount == 1) {
                emitLine("    local _text = tostring(pop())");
            } else {
                // Pop items into local variables (they come off in reverse order)
                for (int i = 0; i < itemCount; i++) {
                    emitLine("    local _item" + std::to_string(i) + " = pop()");
                }
                // Now concatenate in reverse order to get original order
                emitLine("    local _text = \"\"");
                for (int i = itemCount - 1; i >= 0; i--) {
                    emitLine("    _text = _text .. tostring(_item" + std::to_string(i) + ")");
                }
            }

            // Pop y and x coordinates
            emitLine("    local _y = pop()");
            emitLine("    local _x = pop()");

            // Call text_put
            emitLine("    text_put(_x, _y, _text, _fg, _bg)");
            break;
        }

        case IROpcode::PRINT_AT_USING: {
            // PRINT_AT USING: x, y, format, N values, fg, bg on stack
            int valueCount = std::holds_alternative<int>(instr.operand1) ? std::get<int>(instr.operand1) : 0;

            flushExpressionToStack();

            // Pop colors
            emitLine("    local _bg = pop()");
            emitLine("    local _fg = pop()");

            // Pop values for formatting (in reverse order)
            std::vector<std::string> values;
            for (int i = 0; i < valueCount; i++) {
                std::string varName = "_val" + std::to_string(valueCount - i - 1);
                emitLine("    local " + varName + " = pop()");
                values.insert(values.begin(), varName);
            }

            // Pop format string
            emitLine("    local _format = pop()");

            // Pop y and x coordinates
            emitLine("    local _y = pop()");
            emitLine("    local _x = pop()");

            // Format the text using basic_print_using
            emitLine("    local _text = basic_print_using(_format" +
                    (values.empty() ? "" : ", " + std::accumulate(
                        std::next(values.begin()), values.end(), values[0],
                        [](const std::string& a, const std::string& b) { return a + ", " + b; })) + ")");

            // Call text_put
            emitLine("    text_put(_x, _y, _text, _fg, _bg)");
            break;
        }

        case IROpcode::INPUT:
            // Flush expression optimizer before INPUT (side-effecting)
            flushExpressionToStack();

            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string varName = std::get<std::string>(instr.operand1);
                // Use getVariableReference to respect hot/cold variable system
                std::string varRef = m_config.useVariableCache ?
                                     getVariableReference(varName) : getVarName(varName);
                // Check if it's a string variable
                if (varName.find("_STRING") != std::string::npos) {
                    emitLine("    " + varRef + " = basic_input_string()");
                } else {
                    emitLine("    " + varRef + " = basic_input()");
                }
            }
            break;

        case IROpcode::INPUT_AT: {
            // INPUT_AT: x, y on stack, prompt and variable in operands
            // Pop coordinates from stack
            flushExpressionToStack();

            emitLine("    local _y = pop()");
            emitLine("    local _x = pop()");

            // Get prompt and variable name from operands
            std::string prompt = std::holds_alternative<std::string>(instr.operand1) ?
                                std::get<std::string>(instr.operand1) : "";
            std::string varName = std::holds_alternative<std::string>(instr.operand2) ?
                                 std::get<std::string>(instr.operand2) : "";

            if (!varName.empty()) {
                // Use mangled name to match hot variable declarations (name$ -> var_name_STRING)
                std::string mangledName = mangleName(varName);
                std::string varRef = "var_" + mangledName;

                // Generate the input_at call
                if (!prompt.empty()) {
                    emitLine("    " + varRef + " = basic_input_at(_x, _y, \"" + prompt + "\")");
                } else {
                    emitLine("    " + varRef + " = basic_input_at(_x, _y, \"\")");
                }
            }
            break;
        }

        case IROpcode::READ_DATA:
            // Flush expression optimizer before READ (side-effecting)
            flushExpressionToStack();

            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string varName = std::get<std::string>(instr.operand1);
                // Use getVariableReference to respect hot/cold variable system
                std::string varRef = m_config.useVariableCache ?
                                     getVariableReference(varName) : getVarName(varName);
                // Check if it's a string variable
                if (varName.find("_STRING") != std::string::npos) {
                    emitLine("    " + varRef + " = basic_read_data_string()");
                } else {
                    emitLine("    " + varRef + " = basic_read_data()");
                }
            }
            break;

        case IROpcode::RESTORE:
            // Flush expression optimizer before RESTORE (side-effecting)
            flushExpressionToStack();

            if (std::holds_alternative<int>(instr.operand1)) {
                // RESTORE to line number
                int lineNumber = std::get<int>(instr.operand1);
                emitLine("    basic_restore(" + std::to_string(lineNumber) + ")");
            } else if (std::holds_alternative<std::string>(instr.operand1)) {
                // RESTORE to label name
                std::string labelName = std::get<std::string>(instr.operand1);
                emitLine("    basic_restore(" + escapeString(labelName) + ")");
            } else {
                // RESTORE with no argument - restore to beginning
                emitLine("    basic_restore()");
            }
            break;

        case IROpcode::OPEN_FILE:
            // OPEN file (operands: filename, mode, filenum)
            flushExpressionToStack();
            {
                std::string filename = std::get<std::string>(instr.operand1);
                std::string mode = std::get<std::string>(instr.operand2);
                std::string filenum = std::get<std::string>(instr.operand3);
                emitLine("    basic_open(\"" + filename + "\", \"" + mode + "\", " + filenum + ")");
            }
            break;

        case IROpcode::CLOSE_FILE:
            // CLOSE #n
            flushExpressionToStack();
            {
                std::string filenum = std::get<std::string>(instr.operand1);
                emitLine("    basic_close(" + filenum + ")");
            }
            break;

        case IROpcode::CLOSE_FILE_ALL:
            // CLOSE (all files)
            flushExpressionToStack();
            emitLine("    basic_close()");
            break;

        case IROpcode::PRINT_FILE:
            // PRINT# filenum, value
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto expr = m_exprOptimizer.pop();
                if (expr) {
                    std::string code = m_exprOptimizer.toString(expr);
                    std::string filenum = std::get<std::string>(instr.operand1);
                    std::string separator = std::get<std::string>(instr.operand2);
                    emitLine("    basic_print_file(" + filenum + ", " + code + ", " + escapeString(separator) + ")");
                }
            } else {
                flushExpressionToStack();
                std::string filenum = std::get<std::string>(instr.operand1);
                std::string separator = std::get<std::string>(instr.operand2);
                emitLine("    basic_print_file(" + filenum + ", pop(), " + escapeString(separator) + ")");
            }
            break;

        case IROpcode::PRINT_FILE_NEWLINE:
            // Print newline to file
            flushExpressionToStack();
            {
                std::string filenum = std::get<std::string>(instr.operand1);
                emitLine("    basic_print_file(" + filenum + ", \"\", \"\\\\n\")");
            }
            break;

        case IROpcode::INPUT_FILE:
            // INPUT# filenum, var
            flushExpressionToStack();
            {
                std::string filenum = std::get<std::string>(instr.operand1);
                std::string varname = std::get<std::string>(instr.operand2);
                emitLine("    " + getVariableReference(varname) + " = basic_input_file(" + filenum + ")");
            }
            break;

        case IROpcode::LINE_INPUT_FILE:
            // LINE INPUT# filenum, var
            flushExpressionToStack();
            {
                std::string filenum = std::get<std::string>(instr.operand1);
                std::string varname = std::get<std::string>(instr.operand2);
                emitLine("    " + getVariableReference(varname) + " = basic_line_input_file(" + filenum + ")");
            }
            break;

        case IROpcode::WRITE_FILE:
            // WRITE# filenum, value (quoted output)
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto expr = m_exprOptimizer.pop();
                if (expr) {
                    std::string code = m_exprOptimizer.toString(expr);
                    std::string filenum = std::get<std::string>(instr.operand1);
                    bool isLast = std::get<int>(instr.operand2) != 0;
                    emitLine("    basic_write_file(" + filenum + ", " + code + ", " + (isLast ? "true" : "false") + ")");
                }
            } else {
                flushExpressionToStack();
                std::string filenum = std::get<std::string>(instr.operand1);
                bool isLast = std::get<int>(instr.operand2) != 0;
                emitLine("    basic_write_file(" + filenum + ", pop(), " + (isLast ? "true" : "false") + ")");
            }
            break;

        default:
            break;
    }
}

void LuaCodeGenerator::emitBuiltinFunction(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;

    std::string funcName = std::get<std::string>(instr.operand1);
    int argCount = std::holds_alternative<int>(instr.operand2) ? std::get<int>(instr.operand2) : 0;
    
    // LBOUND/UBOUND are now handled as Lua helper functions (defined in header)
    // They take the array as an argument and return the bounds

    // Special handling for IIF - emit as proper conditional expression
    if (funcName == "__IIF" && argCount == 3) {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
            // Pop in reverse order: falseValue, trueValue, condition
            auto falseExpr = m_exprOptimizer.pop();
            auto trueExpr = m_exprOptimizer.pop();
            auto condExpr = m_exprOptimizer.pop();
            
            if (condExpr && trueExpr && falseExpr) {
                // Emit proper ternary using basicBoolToLua for correct BASIC boolean handling
                // This handles BASIC booleans (0/-1) and Lua booleans correctly
                std::string iifExpr = "(function() if basicBoolToLua(" + m_exprOptimizer.toString(condExpr) + 
                                      ") then return (" + m_exprOptimizer.toString(trueExpr) + 
                                      ") else return (" + m_exprOptimizer.toString(falseExpr) + 
                                      ") end end)()";
                m_exprOptimizer.pushVariable(iifExpr);
            } else {
                // Fallback to stack-based
                emitLine("    do");
                emitLine("        local __iif_false = pop()");
                emitLine("        local __iif_true = pop()");
                emitLine("        local __iif_cond = pop()");
                emitLine("        if basicBoolToLua(__iif_cond) then push(__iif_true) else push(__iif_false) end");
                emitLine("    end");
            }
        } else {
            // Stack-based fallback
            emitLine("    do");
            emitLine("        local __iif_false = pop()");
            emitLine("        local __iif_true = pop()");
            emitLine("        local __iif_cond = pop()");
            emitLine("        if basicBoolToLua(__iif_cond) then push(__iif_true) else push(__iif_false) end");
            emitLine("    end");
        }
        return;
    }

    // OPTIMIZATION 1: Handle native Lua math functions FIRST (before modular commands)
    // This ensures SIN, COS, etc. use expression optimizer instead of falling back to stack
    std::string luaFunc;  // Keep this for later use in the file
    
    // Math functions (1 argument) - process immediately
    if (funcName == "SIN") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.sin(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.sin(pop()))");
            }
        } else {
            emitLine("    push(math.sin(pop()))");
        }
        return;
    }
    else if (funcName == "COS") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.cos(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.cos(pop()))");
            }
        } else {
            emitLine("    push(math.cos(pop()))");
        }
        return;
    }
    else if (funcName == "TAN") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.tan(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.tan(pop()))");
            }
        } else {
            emitLine("    push(math.tan(pop()))");
        }
        return;
    }
    else if (funcName == "ATN") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.atan(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.atan(pop()))");
            }
        } else {
            emitLine("    push(math.atan(pop()))");
        }
        return;
    }
    else if (funcName == "SQR") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.sqrt(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.sqrt(pop()))");
            }
        } else {
            emitLine("    push(math.sqrt(pop()))");
        }
        return;
    }
    else if (funcName == "ACS") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.acos(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.acos(pop()))");
            }
        } else {
            emitLine("    push(math.acos(pop()))");
        }
        return;
    }
    else if (funcName == "ASN") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.asin(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.asin(pop()))");
            }
        } else {
            emitLine("    push(math.asin(pop()))");
        }
        return;
    }
    else if (funcName == "DEG") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.deg(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.deg(pop()))");
            }
        } else {
            emitLine("    push(math.deg(pop()))");
        }
        return;
    }
    else if (funcName == "RAD") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.rad(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.rad(pop()))");
            }
        } else {
            emitLine("    push(math.rad(pop()))");
        }
        return;
    }
    else if (funcName == "SGN") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("basic_sgn(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(basic_sgn(pop()))");
            }
        } else {
            emitLine("    push(basic_sgn(pop()))");
        }
        return;
    }
    else if (funcName == "FIX") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("basic_fix(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(basic_fix(pop()))");
            }
        } else {
            emitLine("    push(basic_fix(pop()))");
        }
        return;
    }
    else if (funcName == "LN") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.log(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.log(pop()))");
            }
        } else {
            emitLine("    push(math.log(pop()))");
        }
        return;
    }
    else if (funcName == "PI") {
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("math.pi");
        } else {
            emitLine("    push(math.pi)");
        }
        return;
    }
    else if (funcName == "MOD") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto arg2Expr = m_exprOptimizer.pop();
            auto arg1Expr = m_exprOptimizer.pop();
            if (arg1Expr && arg2Expr) {
                std::string arg1Str = m_exprOptimizer.toString(arg1Expr);
                std::string arg2Str = m_exprOptimizer.toString(arg2Expr);
                m_exprOptimizer.pushVariable("basic_mod(" + arg1Str + ", " + arg2Str + ")");
            } else {
                emitLine("    push(basic_mod(pop(), pop()))");
            }
        } else {
            emitLine("    push(basic_mod(pop(), pop()))");
        }
        return;
    }
    else if (funcName == "INT") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.floor(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.floor(pop()))");
            }
        } else {
            emitLine("    push(math.floor(pop()))");
        }
        return;
    }
    else if (funcName == "ABS") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.abs(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.abs(pop()))");
            }
        } else {
            emitLine("    push(math.abs(pop()))");
        }
        return;
    }
    else if (funcName == "LOG") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.log(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.log(pop()))");
            }
        } else {
            emitLine("    push(math.log(pop()))");
        }
        return;
    }
    else if (funcName == "EXP") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("math.exp(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(math.exp(pop()))");
            }
        } else {
            emitLine("    push(math.exp(pop()))");
        }
        return;
    }
    else if (funcName == "SGN") {
        // SGN needs special handling: SGN(x) = x>0 ? 1 : x<0 ? -1 : 0
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                std::string argStr = m_exprOptimizer.toString(argExpr);
                std::string sgnExpr = "((" + argStr + ") > 0 and 1 or (" + argStr + ") < 0 and -1 or 0)";
                m_exprOptimizer.pushVariable(sgnExpr);
            } else {
                emitLine("    a = pop(); push(a > 0 and 1 or a < 0 and -1 or 0)");
            }
        } else {
            emitLine("    a = pop(); push(a > 0 and 1 or a < 0 and -1 or 0)");
        }
        return;
    }
    
    // OPTIMIZATION 2: Handle RND, TIMER and key string functions BEFORE modular registry
    else if (funcName == "RND") {
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("basic_rnd()");
        } else {
            emitLine("    push(basic_rnd())");
        }
        return;
    }
    else if (funcName == "GETTICKS") {
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("system_getticks()");
        } else {
            emitLine("    push(system_getticks())");
        }
        return;
    }
    else if (funcName == "STR_STRING" || funcName == "STR$" || funcName == "STR") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("tostring(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(tostring(pop()))");
            }
        } else {
            emitLine("    push(tostring(pop()))");
        }
        return;
    }
    else if (funcName == "VAL") {
        // VAL(s) converts string to number
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("tonumber(" + m_exprOptimizer.toString(argExpr) + ") or 0");
            } else {
                emitLine("    push(tonumber(pop()) or 0)");
            }
        } else {
            emitLine("    push(tonumber(pop()) or 0)");
        }
        return;
    }
    else if (funcName == "ASC") {
        // ASC(s) returns ASCII/Unicode code of first character
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    m_exprOptimizer.pushVariable("unicode.unicode_asc(" + m_exprOptimizer.toString(argExpr) + ")");
                } else {
                    emitLine("    push(unicode.unicode_asc(pop()))");
                }
            } else {
                emitLine("    push(unicode.unicode_asc(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    std::string argStr = m_exprOptimizer.toString(argExpr);
                    m_exprOptimizer.pushVariable("string.byte(" + argStr + ", 1)");
                } else {
                    emitLine("    push(string.byte(pop(), 1))");
                }
            } else {
                emitLine("    push(string.byte(pop(), 1))");
            }
        }
        return;
    }
    // CHR$ - removed, handled later with Unicode awareness
    else if (funcName == "HEX_STRING" || funcName == "HEX$" || funcName == "HEX") {
        // HEX$(n, digits) returns hexadecimal string
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto digitsExpr = m_exprOptimizer.pop();
            auto numExpr = m_exprOptimizer.pop();
            if (numExpr && digitsExpr) {
                m_exprOptimizer.pushVariable("HEX_STRING(" + m_exprOptimizer.toString(numExpr) + ", " + m_exprOptimizer.toString(digitsExpr) + ")");
            } else {
                emitLine("    local digits = pop(); local num = pop(); push(HEX_STRING(num, digits))");
            }
        } else if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto numExpr = m_exprOptimizer.pop();
            if (numExpr) {
                m_exprOptimizer.pushVariable("HEX_STRING(" + m_exprOptimizer.toString(numExpr) + ", 0)");
            } else {
                emitLine("    push(HEX_STRING(pop(), 0))");
            }
        } else {
            emitLine("    push(HEX_STRING(pop(), 0))");
        }
        return;
    }
    else if (funcName == "BIN_STRING" || funcName == "BIN$" || funcName == "BIN") {
        // BIN$(n, digits) returns binary string
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto digitsExpr = m_exprOptimizer.pop();
            auto numExpr = m_exprOptimizer.pop();
            if (numExpr && digitsExpr) {
                m_exprOptimizer.pushVariable("BIN_STRING(" + m_exprOptimizer.toString(numExpr) + ", " + m_exprOptimizer.toString(digitsExpr) + ")");
            } else {
                emitLine("    local digits = pop(); local num = pop(); push(BIN_STRING(num, digits))");
            }
        } else if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto numExpr = m_exprOptimizer.pop();
            if (numExpr) {
                m_exprOptimizer.pushVariable("BIN_STRING(" + m_exprOptimizer.toString(numExpr) + ", 0)");
            } else {
                emitLine("    push(BIN_STRING(pop(), 0))");
            }
        } else {
            emitLine("    push(BIN_STRING(pop(), 0))");
        }
        return;
    }
    else if (funcName == "OCT_STRING" || funcName == "OCT$" || funcName == "OCT") {
        // OCT$(n, digits) returns octal string
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto digitsExpr = m_exprOptimizer.pop();
            auto numExpr = m_exprOptimizer.pop();
            if (numExpr && digitsExpr) {
                m_exprOptimizer.pushVariable("OCT_STRING(" + m_exprOptimizer.toString(numExpr) + ", " + m_exprOptimizer.toString(digitsExpr) + ")");
            } else {
                emitLine("    local digits = pop(); local num = pop(); push(OCT_STRING(num, digits))");
            }
        } else if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto numExpr = m_exprOptimizer.pop();
            if (numExpr) {
                m_exprOptimizer.pushVariable("OCT_STRING(" + m_exprOptimizer.toString(numExpr) + ", 0)");
            } else {
                emitLine("    push(OCT_STRING(pop(), 0))");
            }
        } else {
            emitLine("    push(OCT_STRING(pop(), 0))");
        }
        return;
    }
    // UCASE$ - removed, handled later with Unicode awareness
    // LCASE$ - removed, handled later with Unicode awareness
    // String functions (LEN is handled earlier to avoid duplication)
    // LEFT$ - removed, handled later with Unicode awareness
    // RIGHT$ - removed, handled later with Unicode awareness
    // MID$ - removed, handled later with Unicode awareness
    
    // JOIN$ - Join string array elements with separator
    else if (funcName == "JOIN_STRING" || funcName == "JOIN$" || funcName == "JOIN") {
        // JOIN$(array$, separator$) - joins array elements with separator
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto sepExpr = m_exprOptimizer.pop();
            auto arrayExpr = m_exprOptimizer.pop(); 
            if (sepExpr && arrayExpr) {
                std::string result = "string_join(" + m_exprOptimizer.toString(arrayExpr) + ", " +
                                    m_exprOptimizer.toString(sepExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    b = pop(); a = pop(); push(string_join(a, b))");
            }
        } else {
            emitLine("    b = pop(); a = pop(); push(string_join(a, b))");
        }
        return;
    }
    
    // SPLIT$ - Split string into array (this is more complex as it needs to handle array assignment)
    else if (funcName == "SPLIT_STRING" || funcName == "SPLIT$" || funcName == "SPLIT") {
        // This function is special - it's handled differently as it returns an array
        // For now, we'll implement it as a regular function that returns a table
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto delimExpr = m_exprOptimizer.pop();
            auto strExpr = m_exprOptimizer.pop();
            if (delimExpr && strExpr) {
                std::string result = "string_split(" + m_exprOptimizer.toString(strExpr) + ", " +
                                    m_exprOptimizer.toString(delimExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    b = pop(); a = pop(); push(string_split(a, b))");
            }
        } else {
            emitLine("    b = pop(); a = pop(); push(string_split(a, b))");
        }
        return;
    }
    
    // BUFFER$ - Create a string buffer for efficient MID$ operations
    else if (funcName == "BUFFER_STRING" || funcName == "BUFFER$" || funcName == "BUFFER") {
        // BUFFER$(string$) - creates a mutable string buffer
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto strExpr = m_exprOptimizer.pop();
            if (strExpr) {
                std::string result = "create_string_buffer(" + m_exprOptimizer.toString(strExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(create_string_buffer(pop()))");
            }
        } else {
            emitLine("    push(create_string_buffer(pop()))");
        }
        return;
    }
    
    // TOSTR$ - Convert buffer to string (specialized function for buffers)
    else if (funcName == "TOSTR_STRING" || funcName == "TOSTR$" || funcName == "TOSTR") {
        // TOSTR$ specifically for converting string buffers to strings
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto valueExpr = m_exprOptimizer.pop();
            if (valueExpr) {
                std::string result = "buffer_to_string(" + m_exprOptimizer.toString(valueExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(buffer_to_string(pop()))");
            }
        } else {
            emitLine("    push(buffer_to_string(pop()))");
        }
        return;
    }
    
    // INPUT$ - Read fixed number of characters from file
    else if (funcName == "INPUT_STRING" || funcName == "INPUT$" || funcName == "INPUT") {
        // INPUT$(count, fileNumber) - reads count characters from file
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto fileExpr = m_exprOptimizer.pop();
            auto countExpr = m_exprOptimizer.pop();
            if (fileExpr && countExpr) {
                std::string countStr = m_exprOptimizer.toString(countExpr);
                std::string fileStr = m_exprOptimizer.toString(fileExpr);
                std::string result = "basic_input_string_file(" + countStr + ", " + fileStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_input_string_file(pop(), pop()))");
            }
        } else {
            emitLine("    push(basic_input_string_file(pop(), pop()))");
        }
        return;
    }
    
    // BBC BASIC file I/O functions - handle directly for optimal performance
    else if (funcName == "OPENIN") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto filenameExpr = m_exprOptimizer.pop();
            if (filenameExpr) {
                std::string filenameStr = m_exprOptimizer.toString(filenameExpr);
                std::string result = "basic_openin(" + filenameStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_openin(pop()))");
            }
        } else {
            emitLine("    push(basic_openin(pop()))");
        }
        return;
    }
    else if (funcName == "OPENOUT") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto filenameExpr = m_exprOptimizer.pop();
            if (filenameExpr) {
                std::string filenameStr = m_exprOptimizer.toString(filenameExpr);
                std::string result = "basic_openout(" + filenameStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_openout(pop()))");
            }
        } else {
            emitLine("    push(basic_openout(pop()))");
        }
        return;
    }
    else if (funcName == "OPENUP") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto filenameExpr = m_exprOptimizer.pop();
            if (filenameExpr) {
                std::string filenameStr = m_exprOptimizer.toString(filenameExpr);
                std::string result = "basic_openup(" + filenameStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_openup(pop()))");
            }
        } else {
            emitLine("    push(basic_openup(pop()))");
        }
        return;
    }
    else if (funcName == "BGET") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto fileExpr = m_exprOptimizer.pop();
            if (fileExpr) {
                std::string fileStr = m_exprOptimizer.toString(fileExpr);
                std::string result = "basic_bget(" + fileStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_bget(pop()))");
            }
        } else {
            emitLine("    push(basic_bget(pop()))");
        }
        return;
    }
    else if (funcName == "EOF") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto fileExpr = m_exprOptimizer.pop();
            if (fileExpr) {
                std::string fileStr = m_exprOptimizer.toString(fileExpr);
                std::string result = "basic_eof_hash(" + fileStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_eof_hash(pop()))");
            }
        } else {
            emitLine("    push(basic_eof_hash(pop()))");
        }
        return;
    }
    else if (funcName == "EXT") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto fileExpr = m_exprOptimizer.pop();
            if (fileExpr) {
                std::string fileStr = m_exprOptimizer.toString(fileExpr);
                std::string result = "basic_ext_hash(" + fileStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_ext_hash(pop()))");
            }
        } else {
            emitLine("    push(basic_ext_hash(pop()))");
        }
        return;
    }
    else if (funcName == "PTR") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto fileExpr = m_exprOptimizer.pop();
            if (fileExpr) {
                std::string fileStr = m_exprOptimizer.toString(fileExpr);
                std::string result = "basic_ptr_hash(" + fileStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_ptr_hash(pop()))");
            }
        } else {
            emitLine("    push(basic_ptr_hash(pop()))");
        }
        return;
    }
    else if (funcName == "GETS") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto fileExpr = m_exprOptimizer.pop();
            if (fileExpr) {
                std::string fileStr = m_exprOptimizer.toString(fileExpr);
                std::string result = "basic_get_string_line(" + fileStr + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    push(basic_get_string_line(pop()))");
            }
        } else {
            emitLine("    push(basic_get_string_line(pop()))");
        }
        return;
    }
    
    // Check if this is a modular command/function
    // Ensure the global registry is initialized
    FasterBASIC::ModularCommands::initializeGlobalRegistry();
    auto& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();

    // Check both commands and functions
    const auto* commandDef = registry.getCommand(funcName);
    const auto* functionDef = registry.getFunction(funcName);
    const auto* def = commandDef ? commandDef : functionDef;

    if (def) {

        
        // Enhanced parameter handling for modular commands
        std::vector<std::string> paramNames;
        int paramCount = def->parameters.size();
        bool usedExpressionMode = false;
        
        if (paramCount > 0) {
            // Try direct expression optimization first (no locals needed)
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty() && 
                m_exprOptimizer.size() >= paramCount) {
                
                usedExpressionMode = true;
                // Generate direct parameter expressions (no local variables)
                for (int i = paramCount - 1; i >= 0; i--) {
                    auto expr = m_exprOptimizer.pop();
                    if (expr) {
                        paramNames.insert(paramNames.begin(), m_exprOptimizer.toString(expr));
                    } else {
                        paramNames.insert(paramNames.begin(), "nil");
                    }
                }
            } else {
                // Fallback to parameter pool (reuses param0-param19)
                flushExpressionToStack();
                std::string popSequence;
                for (int i = paramCount - 1; i >= 0; i--) {
                    popSequence += "param" + std::to_string(i) + " = pop(); ";
                    paramNames.insert(paramNames.begin(), "param" + std::to_string(i));
                }
                emitLine("    " + popSequence);
            }
        }

        // Check if we have custom code generation
        if (def->hasCustomCodeGen) {
            // Use custom code template - simple substitution for now
            std::string customCode = def->customCodeTemplate;
            // Replace parameter placeholders with actual parameter names
            for (size_t i = 0; i < paramNames.size(); i++) {
                std::string placeholder = "{" + std::to_string(i) + "}";
                size_t pos = 0;
                while ((pos = customCode.find(placeholder, pos)) != std::string::npos) {
                    customCode.replace(pos, placeholder.length(), paramNames[i]);
                    pos += paramNames[i].length();
                }
            }

            // Handle return value for functions
            if (def->isFunction) {
                if (usedExpressionMode && canUseExpressionMode()) {
                    // Push result to expression optimizer so subsequent operators can use it
                    m_exprOptimizer.pushVariable(customCode);
                } else {
                    emitLine("    push(" + customCode + ")");
                }
            } else {
                emitLine("    " + customCode);
            }
        } else {
            // Standard function call generation with TYPENAME parameter handling
            std::string callParams;
            
            // Build parameters, expanding TYPENAME parameters to include schema
            size_t paramIdx = 0;
            for (size_t i = 0; i < def->parameters.size(); i++) {
                const auto& paramDef = def->parameters[i];
                
                if (i > 0) callParams += ", ";
                
                if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::TYPENAME) {
                    // This is a TYPENAME parameter - extract the type name
                    std::string typeNameParam = paramNames[paramIdx++];
                    
                    // Strip quotes if it's a string literal
                    std::string typeName = typeNameParam;
                    if (typeName.length() >= 2 && typeName.front() == '"' && typeName.back() == '"') {
                        typeName = typeName.substr(1, typeName.length() - 2);
                    }
                    
                    // Add the typename string parameter
                    callParams += "\"" + typeName + "\"";
                    
                    // Generate and add schema table as additional parameter
                    callParams += ", ";
                    callParams += generateTypeSchemaTable(typeName);
                } else {
                    // Normal parameter
                    callParams += paramNames[paramIdx++];
                }
            }

            // Handle return value for functions
            if (def->isFunction) {
                std::string functionCall = def->luaFunction + "(" + callParams + ")";
                if (usedExpressionMode && canUseExpressionMode()) {
                    // Push result to expression optimizer so subsequent operators can use it
                    m_exprOptimizer.pushVariable(functionCall);
                } else {
                    emitLine("    push(" + functionCall + ")");
                }
            } else {
                emitLine("    " + def->luaFunction + "(" + callParams + ")");
            }
        }
        return;
    }

    // Special handling for PRINT_AT with PRINT-style syntax
    // Debug: Check what funcName we're getting
    if (funcName == "PRINT_AT") {
        emitLine("    -- DEBUG: PRINT_AT handler called with " + std::to_string(argCount) + " args");
        flushExpressionToStack();

        // PRINT_AT has variable arguments depending on mode:
        // Basic mode: x, y, text1, [text2, ...], [fg], [bg]
        // USING mode: x, y, format, -1 (marker), value1, [value2, ...], fg, bg

        // Pop all arguments in reverse order
        std::vector<std::string> args;
        for (int i = 0; i < argCount; i++) {
            args.insert(args.begin(), "pop()");
        }

        if (argCount < 2) {
            // Invalid - need at least x and y
            emitLine("    -- ERROR: PRINT_AT requires at least x, y coordinates");
            return;
        }

        std::string xCoord = args[0];
        std::string yCoord = args[1];

        // Check for USING mode (marker is -1 at position 3)
        bool hasUsing = false;
        if (argCount >= 4) {
            // We need to check if arg[3] is -1, but it's already popped
            // For now, we'll use a simpler approach: if argCount >= 4 and we detect pattern
            // We'll generate code that checks at runtime
            emitLine("    local _x = " + xCoord);
            emitLine("    local _y = " + yCoord);

            if (argCount == 3) {
                // Simple case: x, y, text
                emitLine("    local _text = tostring(" + args[2] + ")");
                emitLine("    text_put(_x, _y, _text, 0xFFFFFFFF, 0x000000FF)");
            } else if (argCount == 4) {
                // Could be: x, y, text, fg  OR  x, y, text1, text2
                emitLine("    local _text = tostring(" + args[2] + ") .. tostring(" + args[3] + ")");
                emitLine("    text_put(_x, _y, _text, 0xFFFFFFFF, 0x000000FF)");
            } else if (argCount == 5) {
                // x, y, text, fg, bg  OR  x, y, text1, text2, text3
                // Assume last two are colors if they look numeric
                emitLine("    local _arg3 = " + args[2]);
                emitLine("    local _arg4 = " + args[3]);
                emitLine("    local _arg5 = " + args[4]);
                emitLine("    local _text = tostring(_arg3)");
                emitLine("    local _fg = _arg4");
                emitLine("    local _bg = _arg5");
                emitLine("    text_put(_x, _y, _text, _fg, _bg)");
            } else {
                // Multiple text expressions: concatenate all but last 2 (which are colors)
                emitLine("    local _text = \"\"");
                for (size_t i = 2; i < args.size() - 2; i++) {
                    emitLine("    _text = _text .. tostring(" + args[i] + ")");
                }
                emitLine("    local _fg = " + args[args.size() - 2]);
                emitLine("    local _bg = " + args[args.size() - 1]);
                emitLine("    text_put(_x, _y, _text, _fg, _bg)");
            }
        } else {
            // Only x, y provided - use empty text
            emitLine("    text_put(" + xCoord + ", " + yCoord + ", \"\", 0xFFFFFFFF, 0x000000FF)");
        }

        return;
    }

    // Map BASIC builtin to native Lua function
    // (luaFunc already declared earlier)

    // Math functions (1 argument)
    if (funcName == "SIN") luaFunc = "math.sin";
    else if (funcName == "COS") luaFunc = "math.cos";
    else if (funcName == "TAN") luaFunc = "math.tan";
    else if (funcName == "ATN") luaFunc = "math.atan";
    else if (funcName == "SQR") luaFunc = "math.sqrt";
    else if (funcName == "INT") luaFunc = "math.floor";
    else if (funcName == "ABS") luaFunc = "math.abs";
    else if (funcName == "LOG") luaFunc = "math.log";
    else if (funcName == "LN") luaFunc = "math.log";
    else if (funcName == "EXP") luaFunc = "math.exp";
    else if (funcName == "ACS") luaFunc = "math.acos";
    else if (funcName == "ASN") luaFunc = "math.asin";
    else if (funcName == "DEG") luaFunc = "math.deg";
    else if (funcName == "RAD") luaFunc = "math.rad";
    else if (funcName == "SGN") luaFunc = "basic_sgn";
    else if (funcName == "FIX") luaFunc = "basic_fix";
    else if (funcName == "PI") {
        // PI constant - no arguments
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("math.pi");
        } else {
            emitLine("    push(math.pi)");
        }
        return;
    }
    else if (funcName == "MOD") {
        // MOD function with special array magnitude support
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 1) {
            auto arg1Expr = m_exprOptimizer.pop();
            if (m_exprOptimizer.size() >= 1) {
                auto arg2Expr = m_exprOptimizer.pop();
                if (arg1Expr && arg2Expr) {
                    std::string arg1Str = m_exprOptimizer.toString(arg1Expr);
                    std::string arg2Str = m_exprOptimizer.toString(arg2Expr);
                    m_exprOptimizer.pushVariable("basic_mod(" + arg2Str + ", " + arg1Str + ")");
                } else {
                    emitLine("    push(basic_mod(pop(), pop()))");
                }
            } else if (arg1Expr) {
                // Single argument - array magnitude
                std::string argStr = m_exprOptimizer.toString(arg1Expr);
                m_exprOptimizer.pushVariable("basic_mod(" + argStr + ")");
            }
        } else {
            emitLine("    push(basic_mod(pop(), pop()))");
        }
        return;
    }

    
    // OPTIMIZATION 2: Handle native functions BEFORE modular registry check
    // This ensures LEFT$, RIGHT$, MID$, RND, etc. use expression optimizer instead of stack operations
    
    // RND is special - no arguments
    else if (funcName == "RND") {
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("basic_rnd()");
        } else {
            emitLine("    push(basic_rnd())");
        }
        return;
    }
    // String functions
    else if (funcName == "LEN") {
        if (m_unicodeMode) {
            // In Unicode mode, use unicode.len (which is just # operator on table)
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    m_exprOptimizer.pushVariable("unicode.unicode_len(" + m_exprOptimizer.toString(argExpr) + ")");
                } else {
                    emitLine("    push(unicode.unicode_len(pop()))");
                }
            } else {
                emitLine("    push(unicode.unicode_len(pop()))");
            }
            return;
        } else {
            luaFunc = "string.len";
        }
    }
    else if (funcName == "ASC") {
        // ASC(s) returns ASCII/Unicode code of first character
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    m_exprOptimizer.pushVariable("unicode.unicode_asc(" + m_exprOptimizer.toString(argExpr) + ")");
                } else {
                    emitLine("    push(unicode.unicode_asc(pop()))");
                }
            } else {
                emitLine("    push(unicode.unicode_asc(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    std::string argStr = m_exprOptimizer.toString(argExpr);
                    m_exprOptimizer.pushVariable("string.byte(" + argStr + ", 1)");
                } else {
                    emitLine("    push(string.byte(pop(), 1))");
                }
            } else {
                emitLine("    push(string.byte(pop(), 1))");
            }
        }
        return;
    }
    else if (funcName == "CHR_STRING" || funcName == "CHR$" || funcName == "CHR") {
        // CHR$(n) returns character with ASCII/Unicode code n
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    m_exprOptimizer.pushVariable("unicode.unicode_chr(" + m_exprOptimizer.toString(argExpr) + ")");
                } else {
                    emitLine("    push(unicode.chr(pop()))");
                }
            } else {
                emitLine("    push(unicode.chr(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    std::string argStr = m_exprOptimizer.toString(argExpr);
                    m_exprOptimizer.pushVariable("string.char(" + argStr + ")");
                } else {
                    emitLine("    push(string.char(pop()))");
                }
            } else {
                emitLine("    push(string.char(pop()))");
            }
        }
        return;
    }
    else if (funcName == "STR_STRING" || funcName == "STR$" || funcName == "STR") {
        // STR$(n) converts number to string
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("tostring(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(tostring(pop()))");
            }
        } else {
            emitLine("    push(tostring(pop()))");
        }
        return;
    }
    else if (funcName == "VAL") {
        // VAL(s) converts string to number
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("tonumber(" + m_exprOptimizer.toString(argExpr) + ") or 0");
            } else {
                emitLine("    push(tonumber(pop()) or 0)");
            }
        } else {
            emitLine("    push(tonumber(pop()) or 0)");
        }
        return;
    }
    else if (funcName == "LEFT_STRING" || funcName == "LEFT$" || funcName == "LEFT") {
        // LEFT$(s, n) returns leftmost n characters
        if (m_unicodeMode) {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                auto lenExpr = m_exprOptimizer.pop();
                auto strExpr = m_exprOptimizer.pop();
                if (lenExpr && strExpr) {
                    std::string result = "unicode.unicode_left(" + m_exprOptimizer.toString(strExpr) + ", " +
                                        m_exprOptimizer.toString(lenExpr) + ")";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    b = pop(); a = pop(); push(unicode.left(a, b))");
                }
            } else {
                emitLine("    b = pop(); a = pop(); push(unicode.left(a, b))");
            }
        } else {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                auto lenExpr = m_exprOptimizer.pop();
                auto strExpr = m_exprOptimizer.pop();
                if (lenExpr && strExpr) {
                    std::string result = "string.sub(" + m_exprOptimizer.toString(strExpr) + ", 1, " +
                                        m_exprOptimizer.toString(lenExpr) + ")";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    b = pop(); a = pop(); push(string.sub(a, 1, b))");
                }
            } else {
                emitLine("    b = pop(); a = pop(); push(string.sub(a, 1, b))");
            }
        }
        return;
    }
    else if (funcName == "RIGHT_STRING" || funcName == "RIGHT$" || funcName == "RIGHT") {
        // RIGHT$(s, n) returns rightmost n characters
        if (m_unicodeMode) {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                auto lenExpr = m_exprOptimizer.pop();
                auto strExpr = m_exprOptimizer.pop();
                if (lenExpr && strExpr) {
                    std::string result = "unicode.unicode_right(" + m_exprOptimizer.toString(strExpr) + ", " +
                                        m_exprOptimizer.toString(lenExpr) + ")";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    b = pop(); a = pop(); push(unicode.right(a, b))");
                }
            } else {
                emitLine("    b = pop(); a = pop(); push(unicode.right(a, b))");
            }
        } else {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                auto lenExpr = m_exprOptimizer.pop();
                auto strExpr = m_exprOptimizer.pop();
                if (lenExpr && strExpr) {
                    std::string result = "string.sub(" + m_exprOptimizer.toString(strExpr) + ", -" +
                                        m_exprOptimizer.toString(lenExpr) + ")";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    b = pop(); a = pop(); push(string.sub(a, -b))");
                }
            } else {
                emitLine("    b = pop(); a = pop(); push(string.sub(a, -b))");
            }
        }
        return;
    }
    else if (funcName == "MID_STRING" || funcName == "MID$" || funcName == "MID") {
        // MID$(s, start, len) returns substring (BASIC uses 1-based indexing)
        if (m_unicodeMode) {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
                auto lenExpr = m_exprOptimizer.pop();
                auto startExpr = m_exprOptimizer.pop();
                auto strExpr = m_exprOptimizer.pop();
                if (lenExpr && startExpr && strExpr) {
                    std::string result = "unicode.unicode_mid(" + m_exprOptimizer.toString(strExpr) + ", " +
                                        m_exprOptimizer.toString(startExpr) + ", " +
                                        m_exprOptimizer.toString(lenExpr) + ")";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    len = pop(); start = pop(); s = pop(); push(unicode.mid(s, start, len))");
                }
            } else {
                emitLine("    len = pop(); start = pop(); s = pop(); push(unicode.mid(s, start, len))");
            }
        } else {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
                auto lenExpr = m_exprOptimizer.pop();
                auto startExpr = m_exprOptimizer.pop();
                auto strExpr = m_exprOptimizer.pop();
                if (lenExpr && startExpr && strExpr) {
                    std::string result = "string.sub(" + m_exprOptimizer.toString(strExpr) + ", " +
                                        m_exprOptimizer.toString(startExpr) + ", " +
                                        m_exprOptimizer.toString(startExpr) + " + " +
                                        m_exprOptimizer.toString(lenExpr) + " - 1)";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    len = pop(); start = pop(); s = pop(); push(string.sub(s, start, start + len - 1))");
                }
            } else {
                emitLine("    len = pop(); start = pop(); s = pop(); push(string.sub(s, start, start + len - 1))");
            }
        }
        return;
    }
    else if (funcName == "INSTR") {
        // INSTR can have 2 or 3 arguments:
        // 2 args: INSTR(haystack$, needle$) - search from beginning
        // 3 args: INSTR(start, haystack$, needle$) - search from position start
        if (argCount == 3) {
            // 3-argument version: INSTR(start, haystack$, needle$)
            if (m_unicodeMode) {
                if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
                    auto needleExpr = m_exprOptimizer.pop();
                    auto haystackExpr = m_exprOptimizer.pop();
                    auto startExpr = m_exprOptimizer.pop();
                    if (needleExpr && haystackExpr && startExpr) {
                        std::string result = "unicode.unicode_instr(" + m_exprOptimizer.toString(startExpr) + ", " +
                                            m_exprOptimizer.toString(haystackExpr) + ", " +
                                            m_exprOptimizer.toString(needleExpr) + ")";
                        m_exprOptimizer.pushVariable(result);
                    } else {
                        emitLine("    c = pop(); b = pop(); a = pop(); push(unicode.instr_start(a, b, c))");
                    }
                } else {
                    emitLine("    c = pop(); b = pop(); a = pop(); push(unicode.instr_start(a, b, c))");
                }
            } else {
                if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
                    auto needleExpr = m_exprOptimizer.pop();
                    auto haystackExpr = m_exprOptimizer.pop();
                    auto startExpr = m_exprOptimizer.pop();
                    if (needleExpr && haystackExpr && startExpr) {
                        std::string result = "(string.find(" + m_exprOptimizer.toString(haystackExpr) + ", " +
                                            m_exprOptimizer.toString(needleExpr) + ", " +
                                            m_exprOptimizer.toString(startExpr) + ", true) or 0)";
                        m_exprOptimizer.pushVariable(result);
                    } else {
                        emitLine("    c = pop(); b = pop(); a = pop(); push(string.find(b, c, a, true) or 0)");
                    }
                } else {
                    emitLine("    c = pop(); b = pop(); a = pop(); push(string.find(b, c, a, true) or 0)");
                }
            }
        } else {
            // 2-argument version: INSTR(haystack$, needle$)
            if (m_unicodeMode) {
                if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                    auto needleExpr = m_exprOptimizer.pop();
                    auto haystackExpr = m_exprOptimizer.pop();
                    if (needleExpr && haystackExpr) {
                        std::string result = "unicode.unicode_instr(" + m_exprOptimizer.toString(haystackExpr) + ", " +
                                            m_exprOptimizer.toString(needleExpr) + ")";
                        m_exprOptimizer.pushVariable(result);
                    } else {
                        emitLine("    b = pop(); a = pop(); push(unicode.instr(a, b))");
                    }
                } else {
                    emitLine("    b = pop(); a = pop(); push(unicode.instr(a, b))");
                }
            } else {
                if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                    auto needleExpr = m_exprOptimizer.pop();
                    auto haystackExpr = m_exprOptimizer.pop();
                    if (needleExpr && haystackExpr) {
                        std::string result = "(string.find(" + m_exprOptimizer.toString(haystackExpr) + ", " +
                                            m_exprOptimizer.toString(needleExpr) + ", 1, true) or 0)";
                        m_exprOptimizer.pushVariable(result);
                    } else {
                        emitLine("    b = pop(); a = pop(); push(string.find(a, b, 1, true) or 0)");
                    }
                } else {
                    emitLine("    b = pop(); a = pop(); push(string.find(a, b, 1, true) or 0)");
                }
            }
        }
        return;
    }
    else if (funcName == "STRING_STRING" || funcName == "STRING$" || funcName == "STRING") {
        // STRING$(count, char$) or STRING$(count, ascii) returns repeated character
        if (m_unicodeMode) {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                auto charExpr = m_exprOptimizer.pop();
                auto countExpr = m_exprOptimizer.pop();
                if (charExpr && countExpr) {
                    std::string charStr = m_exprOptimizer.toString(charExpr);
                    std::string countStr = m_exprOptimizer.toString(countExpr);
                    // Handle both table (codepoint array) and number: if number, use directly
                    std::string result = "unicode.unicode_string(" + countStr + ", (type(" + charStr + ") == 'number' and " + charStr + " or unicode.unicode_asc(" + charStr + ")))";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    b = pop(); a = pop(); push(unicode.unicode_string(a, type(b) == 'number' and b or unicode.unicode_asc(b)))");
                }
            } else {
                emitLine("    b = pop(); a = pop(); push(unicode.unicode_string(a, type(b) == 'number' and b or unicode.unicode_asc(b)))");
            }
        } else {
            if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
                auto charExpr = m_exprOptimizer.pop();
                auto countExpr = m_exprOptimizer.pop();
                if (charExpr && countExpr) {
                    std::string charStr = m_exprOptimizer.toString(charExpr);
                    std::string countStr = m_exprOptimizer.toString(countExpr);
                    // Handle both string and number: if number, convert to char
                    std::string result = "string.rep((type(" + charStr + ") == 'number' and string.char(" + charStr + ") or string.sub(" + charStr + ", 1, 1)), " + countStr + ")";
                    m_exprOptimizer.pushVariable(result);
                } else {
                    emitLine("    b = pop(); a = pop(); push(string.rep(type(b) == 'number' and string.char(b) or string.sub(b, 1, 1), a))");
                }
            } else {
                emitLine("    b = pop(); a = pop(); push(string.rep(type(b) == 'number' and string.char(b) or string.sub(b, 1, 1), a))");
            }
        }
        return;
    }
    else if (funcName == "SPACE_STRING" || funcName == "SPACE$" || funcName == "SPACE") {
        // SPACE$(n) returns n spaces
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto countExpr = m_exprOptimizer.pop();
                if (countExpr) {
                    m_exprOptimizer.pushVariable("unicode_space(" + m_exprOptimizer.toString(countExpr) + ")");
                } else {
                    emitLine("    push(unicode.space(pop()))");
                }
            } else {
                emitLine("    push(unicode.space(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto countExpr = m_exprOptimizer.pop();
                if (countExpr) {
                    m_exprOptimizer.pushVariable("string.rep(' ', " + m_exprOptimizer.toString(countExpr) + ")");
                } else {
                    emitLine("    push(string.rep(' ', pop()))");
                }
            } else {
                emitLine("    push(string.rep(' ', pop()))");
            }
        }
        return;
    }
    else if (funcName == "LCASE_STRING" || funcName == "LCASE$" || funcName == "LCASE") {
        // LCASE$(s) returns lowercase string
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("unicode.unicode_lower(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(unicode.lower(pop()))");
                }
            } else {
                emitLine("    push(unicode.lower(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("string.lower(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(string.lower(pop()))");
                }
            } else {
                emitLine("    push(string.lower(pop()))");
            }
        }
        return;
    }
    else if (funcName == "UCASE_STRING" || funcName == "UCASE$" || funcName == "UCASE") {
        // UCASE$(s) returns uppercase string
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("unicode.unicode_upper(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(unicode.upper(pop()))");
                }
            } else {
                emitLine("    push(unicode.upper(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("string.upper(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(string.upper(pop()))");
                }
            } else {
                emitLine("    push(string.upper(pop()))");
            }
        }
        return;
    }
    else if (funcName == "LTRIM_STRING" || funcName == "LTRIM$" || funcName == "LTRIM") {
        // LTRIM$(s) removes leading spaces
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("unicode_ltrim(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(unicode.ltrim(pop()))");
                }
            } else {
                emitLine("    push(unicode.ltrim(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("string.match(" + m_exprOptimizer.toString(strExpr) + ", '^%s*(.*)$')");
                } else {
                    emitLine("    push(string.match(pop(), '^%s*(.*)$'))");
                }
            } else {
                emitLine("    push(string.match(pop(), '^%s*(.*)$'))");
            }
        }
        return;
    }
    else if (funcName == "RTRIM_STRING" || funcName == "RTRIM$" || funcName == "RTRIM") {
        // RTRIM$(s) removes trailing spaces
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("unicode_rtrim(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(unicode.rtrim(pop()))");
                }
            } else {
                emitLine("    push(unicode.rtrim(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("string.match(" + m_exprOptimizer.toString(strExpr) + ", '^(.-)%s*$')");
                } else {
                    emitLine("    push(string.match(pop(), '^(.-)%s*$'))");
                }
            } else {
                emitLine("    push(string.match(pop(), '^(.-)%s*$'))");
            }
        }
        return;
    }
    else if (funcName == "TRIM_STRING" || funcName == "TRIM$" || funcName == "TRIM") {
        // TRIM$(s) removes leading and trailing spaces
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("unicode_trim(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(unicode.trim(pop()))");
                }
            } else {
                emitLine("    push(unicode.trim(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("string.match(" + m_exprOptimizer.toString(strExpr) + ", '^%s*(.-)%s*$')");
                } else {
                    emitLine("    push(string.match(pop(), '^%s*(.-)%s*$'))");
                }
            } else {
                emitLine("    push(string.match(pop(), '^%s*(.-)%s*$'))");
            }
        }
        return;
    }
    else if (funcName == "REVERSE_STRING" || funcName == "REVERSE$" || funcName == "REVERSE") {
        // REVERSE$(s) reverses a string
        if (m_unicodeMode) {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("unicode_reverse(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(unicode.reverse(pop()))");
                }
            } else {
                emitLine("    push(unicode.reverse(pop()))");
            }
        } else {
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto strExpr = m_exprOptimizer.pop();
                if (strExpr) {
                    m_exprOptimizer.pushVariable("string.reverse(" + m_exprOptimizer.toString(strExpr) + ")");
                } else {
                    emitLine("    push(string.reverse(pop()))");
                }
            } else {
                emitLine("    push(string.reverse(pop()))");
            }
        }
        return;
    }

    // RND is special - no arguments
    else if (funcName == "RND") {
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("basic_rnd()");
        } else {
            emitLine("    push(basic_rnd())");
        }
        return;
    }
    
    // OPTIMIZATION 2: Handle RND and key string functions BEFORE modular registry
    else if (funcName == "RND") {
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("basic_rnd()");
        } else {
            emitLine("    push(basic_rnd())");
        }
        return;
    }
    else if (funcName == "STR_STRING" || funcName == "STR$" || funcName == "STR") {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable("tostring(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(tostring(pop()))");
            }
        } else {
            emitLine("    push(tostring(pop()))");
        }
        return;
    }
    else if (funcName == "LEFT_STRING" || funcName == "LEFT$" || funcName == "LEFT") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto lenExpr = m_exprOptimizer.pop();
            auto strExpr = m_exprOptimizer.pop();
            if (lenExpr && strExpr) {
                std::string result = "string.sub(" + m_exprOptimizer.toString(strExpr) + ", 1, " +
                                    m_exprOptimizer.toString(lenExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    b = pop(); a = pop(); push(string.sub(a, 1, b))");
            }
        } else {
            emitLine("    b = pop(); a = pop(); push(string.sub(a, 1, b))");
        }
        return;
    }
    else if (funcName == "RIGHT_STRING" || funcName == "RIGHT$" || funcName == "RIGHT") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto lenExpr = m_exprOptimizer.pop();
            auto strExpr = m_exprOptimizer.pop();
            if (lenExpr && strExpr) {
                std::string result = "string.sub(" + m_exprOptimizer.toString(strExpr) + ", -" +
                                    m_exprOptimizer.toString(lenExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    b = pop(); a = pop(); push(string.sub(a, -b))");
            }
        } else {
            emitLine("    b = pop(); a = pop(); push(string.sub(a, -b))");
        }
        return;
    }
    else if (funcName == "MID_STRING" || funcName == "MID$" || funcName == "MID") {
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 3) {
            auto lenExpr = m_exprOptimizer.pop();
            auto startExpr = m_exprOptimizer.pop();
            auto strExpr = m_exprOptimizer.pop();
            if (lenExpr && startExpr && strExpr) {
                std::string startStr = m_exprOptimizer.toString(startExpr);
                std::string lenStr = m_exprOptimizer.toString(lenExpr);
                std::string result = "string.sub(" + m_exprOptimizer.toString(strExpr) + ", " +
                                    startStr + ", " + startStr + " + " + lenStr + " - 1)";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    c = pop(); b = pop(); a = pop(); push(string.sub(a, b, b + c - 1))");
            }
        } else {
            emitLine("    c = pop(); b = pop(); a = pop(); push(string.sub(a, b, b + c - 1))");
        }
        return;
    }

    // If we have a native Lua mapping for single-argument math function
    if (!luaFunc.empty()) {
        if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                m_exprOptimizer.pushVariable(luaFunc + "(" + m_exprOptimizer.toString(argExpr) + ")");
            } else {
                emitLine("    push(" + luaFunc + "(pop()))");
            }
        } else {
            emitLine("    push(" + luaFunc + "(pop()))");
        }
        return;
    }

    // SuperTerminal API Commands (statements - no return value)
    // High-res Graphics Layer commands - now handled by registry

    // Screen control commands - now handled by registry
    // Chunky Graphics commands - now handled by registry
    // Text layer commands - now handled by registry

    // Sprite commands - now handled by registry

    // Audio commands - now handled by registry
    if (funcName == "COLOR") {
        flushExpressionToStack();
        if (argCount == 2) {
            emitLine("    local bg = pop(); local fg = pop()");
            emitLine("    basic_color(fg, bg)");
        } else if (argCount == 1) {
            emitLine("    basic_color(pop(), -1)");
        }
        return;
    } else if (funcName == "WIDTH") {
        // WIDTH function returns terminal width
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("screen_width()");
        } else {
            emitLine("    push(screen_width())");
        }
        return;
    } else if (funcName == "HEIGHT") {
        // HEIGHT function returns terminal height
        if (canUseExpressionMode()) {
            m_exprOptimizer.pushVariable("screen_height()");
        } else {
            emitLine("    push(screen_height())");
        }
        return;
    }
    // Music/Audio commands - now handled by registry
    // Timing commands - now handled by registry
    // Particle commands - now handled by registry

    // === File I/O Functions ===
    else if (funcName == "EOF") {
        flushExpressionToStack();
        emitLine("    local filenum = pop()");
        emitLine("    push(basic_eof(filenum) and 1 or 0)");
        return;
    } else if (funcName == "LOC") {
        flushExpressionToStack();
        emitLine("    push(basic_loc(pop()))");
        return;
    } else if (funcName == "LOF") {
        flushExpressionToStack();
        emitLine("    push(basic_lof(pop()))");
        return;
    }

    // Unknown builtin - emit warning
    if (m_config.emitComments) {
        emitComment("Unknown builtin: " + funcName);
    }
    
    // DEBUG: Show when we hit the fallback path

    
    // Fallback: try to call it anyway (might be graphics/text function)
    // Name is already mangled in parser
    if (argCount > 0) {
        // CRITICAL FIX: Collect arguments from expression optimizer or stack
        // This properly handles complex expressions (like GetX() + 50)
        std::vector<std::string> argExprs;
        
        if (canUseExpressionMode() && m_exprOptimizer.size() >= argCount) {
            // Collect arguments from expression optimizer (in reverse order)
            for (int i = 0; i < argCount; i++) {
                auto argExpr = m_exprOptimizer.pop();
                if (argExpr) {
                    argExprs.insert(argExprs.begin(), m_exprOptimizer.toString(argExpr));
                } else {
                    argExprs.insert(argExprs.begin(), "nil");
                }
            }
        } else {
            // Fallback to stack-based collection
            flushExpressionToStack();
            for (int i = 0; i < argCount; i++) {
                argExprs.insert(argExprs.begin(), "pop()");
            }
        }
        
        // Build argument list
        std::string args;
        for (size_t i = 0; i < argExprs.size(); i++) {
            if (i > 0) args += ", ";
            args += argExprs[i];
        }
        
        emitLine("    " + funcName + "(" + args + ")");
    } else {
        emitLine("    " + funcName + "()");
    }
}

// =============================================================================
// User-Defined Functions and Subroutines
// =============================================================================

void LuaCodeGenerator::emitFunctionDefinition(const IRInstruction& instr) {
    std::string name;

    // DEFINE_FUNCTION/DEFINE_SUB have operand1, but END_FUNCTION/END_SUB don't
    if (instr.opcode == IROpcode::DEFINE_FUNCTION || instr.opcode == IROpcode::DEFINE_SUB) {
        if (!std::holds_alternative<std::string>(instr.operand1)) return;
        name = std::get<std::string>(instr.operand1);
    }

    switch (instr.opcode) {
        case IROpcode::DEFINE_FUNCTION:
        case IROpcode::DEFINE_SUB: {
            // Flush any pending expressions before defining function
            flushExpressionToStack();

            bool isFunction = (instr.opcode == IROpcode::DEFINE_FUNCTION);

            // Look up function definition info
            auto it = m_functionDefs.find(name);
            if (it != m_functionDefs.end()) {
                m_currentFunction = &it->second;
            }

            // Begin function definition
            emitLine("");
            if (m_config.emitComments) {
                emitComment((isFunction ? "FUNCTION " : "SUB ") + name);
            }

            // Build parameter list
            std::string paramList = "";
            if (m_currentFunction) {
                for (size_t i = 0; i < m_currentFunction->parameters.size(); i++) {
                    if (i > 0) paramList += ", ";
                    paramList += getVarName(m_currentFunction->parameters[i]);
                }
            }

            // Name is already mangled in parser
            emitLine("local function func_" + name + "(" + paramList + ")");
            
            // Emit local variable declarations
            if (m_currentFunction && !m_currentFunction->localVariables.empty()) {
                std::string localDecl = "    local ";
                for (size_t i = 0; i < m_currentFunction->localVariables.size(); i++) {
                    if (i > 0) localDecl += ", ";
                    localDecl += getVarName(m_currentFunction->localVariables[i]);
                }
                emitLine(localDecl);
            }

            break;
        }

        case IROpcode::END_FUNCTION:
        case IROpcode::END_SUB: {
            // Add implicit return for BYREF parameters at function end
            // BUT: Don't add if last statement was already a RETURN
            bool needImplicitReturn = (m_lastEmittedOpcode != IROpcode::RETURN_VALUE && 
                                       m_lastEmittedOpcode != IROpcode::RETURN_VOID);
            
            if (m_currentFunction && needImplicitReturn) {
                // Build list of BYREF parameters to return
                std::string byrefReturns;
                for (size_t i = 0; i < m_currentFunction->parameters.size(); i++) {
                    if (i < m_currentFunction->parameterIsByRef.size() && m_currentFunction->parameterIsByRef[i]) {
                        if (!byrefReturns.empty()) byrefReturns += ", ";
                        byrefReturns += "var_" + m_currentFunction->parameters[i];
                    }
                }
                
                // For FUNCTION, return function value + BYREF params
                if (m_currentFunction->isFunction && !byrefReturns.empty()) {
                    std::string funcResultVar = "var_" + m_currentFunction->name;
                    emitLine("    return " + funcResultVar + ", " + byrefReturns);
                } else if (!m_currentFunction->isFunction && !byrefReturns.empty()) {
                    // For SUB, just return BYREF params
                    emitLine("    return " + byrefReturns);
                } else if (m_currentFunction->isFunction) {
                    // Function with no BYREF - return function value
                    std::string funcResultVar = "var_" + m_currentFunction->name;
                    emitLine("    return " + funcResultVar);
                }
                // SUB with no BYREF - no return statement needed
            }
            
            emitLine("end");
            emitLine("");

            m_currentFunction = nullptr;
            m_lastEmittedOpcode = instr.opcode;
            break;
        }

        default:
            break;
    }
}

void LuaCodeGenerator::emitFunctionCall(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;

    std::string funcName = std::get<std::string>(instr.operand1);
    int argCount = 0;

    if (std::holds_alternative<int>(instr.operand2)) {
        argCount = std::get<int>(instr.operand2);
    }

    bool isFunction = (instr.opcode == IROpcode::CALL_FUNCTION);
    
    // Extract argument variable names from operand3 (for BYREF support)
    std::vector<std::string> argVarNames;
    if (std::holds_alternative<std::string>(instr.operand3)) {
        std::string argVarList = std::get<std::string>(instr.operand3);
        if (!argVarList.empty()) {
            size_t pos = 0;
            size_t nextComma = 0;
            while ((nextComma = argVarList.find(',', pos)) != std::string::npos) {
                argVarNames.push_back(argVarList.substr(pos, nextComma - pos));
                pos = nextComma + 1;
            }
            argVarNames.push_back(argVarList.substr(pos));
        }
    }
    
    // Check if this function has BYREF parameters
    std::vector<bool> byrefFlags;
    bool hasByRef = false;
    if (m_functionDefs.find(funcName) != m_functionDefs.end()) {
        const auto& funcInfo = m_functionDefs[funcName];
        byrefFlags = funcInfo.parameterIsByRef;
        for (bool flag : byrefFlags) {
            if (flag) {
                hasByRef = true;
                break;
            }
        }
    }

    if (canUseExpressionMode() && (argCount == 0 || !m_exprOptimizer.isEmpty())) {
        // Build arguments from expression optimizer (or no args for zero-argument functions)
        std::vector<std::string> args;
        for (int i = 0; i < argCount; i++) {
            auto argExpr = m_exprOptimizer.pop();
            if (argExpr) {
                args.insert(args.begin(), m_exprOptimizer.toString(argExpr));
            } else {
                args.insert(args.begin(), "nil");
            }
        }

        // Check if this is an external API function (SuperTerminal APIs)
        // External functions are all uppercase with underscores
        // Also treat graphics commands as built-in (RECT, CIRCLE, etc.)
        bool isExternalFunc = false;
        if (funcName == "RECT" || funcName == "CIRCLEF" ||
            funcName == "CIRCLE" || funcName == "LINE" || funcName == "PSET" ||
            funcName == "CLS" || funcName == "WAIT_FRAME" || funcName == "WAIT_FRAMES" || funcName == "WAIT_MS" ||
            funcName == "SLEEP") {
            isExternalFunc = true;
        } else if (!funcName.empty() && funcName.find('_') != std::string::npos) {
            // Check if all chars are uppercase, digits, or underscores
            isExternalFunc = true;
            for (char c : funcName) {
                if (!(std::isupper(c) || std::isdigit(c) || c == '_')) {
                    isExternalFunc = false;
                    break;
                }
            }
        }

        // Build function call expression (name already mangled in parser)
        // Check if this is a type constructor (ends with _new)
        bool isTypeConstructor = (funcName.length() > 4 && funcName.substr(funcName.length() - 4) == "_new");
        std::string prefix = (isExternalFunc || isTypeConstructor) ? "" : "func_";
        std::string actualFuncName = funcName;
        // Convert external functions to lowercase (they're registered that way in Lua)
        // Graphics commands map to gfx_* functions
        if (isExternalFunc) {
            if (funcName == "RECT") {
                actualFuncName = "gfx_rect_outline";
            } else if (funcName == "CIRCLEF") {
                actualFuncName = "gfx_circle";
            } else if (funcName == "CIRCLE") {
                actualFuncName = "gfx_circle_outline";
            } else if (funcName == "LINE") {
                actualFuncName = "gfx_line";
            } else if (funcName == "PSET") {
                actualFuncName = "gfx_point";
            } else if (funcName == "CLS") {
                actualFuncName = "text_clear";
            } else if (funcName == "WAIT_FRAME") {
                actualFuncName = "basic_wait_frame";
            } else if (funcName == "WAIT_FRAMES") {
                actualFuncName = "basic_wait_frames";
            } else if (funcName == "WAIT_MS") {
                actualFuncName = "basic_wait_ms";
            } else if (funcName == "SLEEP") {
                actualFuncName = "basic_sleep";
            } else {
                std::transform(actualFuncName.begin(), actualFuncName.end(), actualFuncName.begin(), ::tolower);
            }
        }
        std::string callExpr = prefix + actualFuncName + "(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) callExpr += ", ";
            callExpr += args[i];
        }
        callExpr += ")";

        if (isFunction) {
            // Function call - push result (or handle BYREF unpacking)
            if (hasByRef) {
                // Need to unpack BYREF returns
                flushExpressionToStack();
                
                // Build list of variables to receive BYREF returns
                std::string lvalues;
                bool hasValidByRef = false;
                for (size_t i = 0; i < byrefFlags.size() && i < argVarNames.size(); i++) {
                    if (byrefFlags[i] && !argVarNames[i].empty()) {
                        if (!lvalues.empty()) lvalues += ", ";
                        lvalues += getVarName(argVarNames[i]);
                        hasValidByRef = true;
                    }
                }
                
                if (hasValidByRef) {
                    // Emit tuple unpacking: retval, byref1, byref2, ... = func(...)
                    emitLine("    local _retval");
                    emitLine("    _retval, " + lvalues + " = " + callExpr);
                    emitLine("    push(_retval)");
                } else {
                    // No valid BYREF variables, just call normally
                    emitLine("    push(" + callExpr + ")");
                }
            } else {
                // No BYREF - normal function call
                m_exprOptimizer.pushVariable(callExpr);
            }
        } else {
            // SUB call - just execute (or handle BYREF unpacking)
            flushExpressionToStack();
            
            if (hasByRef) {
                // Build list of variables to receive BYREF returns
                std::string lvalues;
                bool hasValidByRef = false;
                for (size_t i = 0; i < byrefFlags.size() && i < argVarNames.size(); i++) {
                    if (byrefFlags[i] && !argVarNames[i].empty()) {
                        if (!lvalues.empty()) lvalues += ", ";
                        lvalues += getVarName(argVarNames[i]);
                        hasValidByRef = true;
                    }
                }
                
                if (hasValidByRef) {
                    // Emit tuple unpacking for SUB: byref1, byref2, ... = func(...)
                    emitLine("    " + lvalues + " = " + callExpr);
                } else {
                    // No valid BYREF variables, just call normally
                    emitLine("    " + callExpr);
                }
            } else {
                // No BYREF - normal SUB call
                emitLine("    " + callExpr);
            }
        }
    } else {
        // Stack-based mode
        flushExpressionToStack();

        // Pop arguments in reverse order
        std::vector<std::string> args;
        for (int i = 0; i < argCount; i++) {
            args.insert(args.begin(), popExpr());
        }

        // Check if this is an external API function (SuperTerminal APIs)
        // External functions are all uppercase with underscores
        // Also treat graphics commands as built-in (RECT, CIRCLE, etc.)
        bool isExternalFunc = false;
        if (funcName == "RECT" || funcName == "CIRCLEF" ||
            funcName == "CIRCLE" || funcName == "LINE" || funcName == "PSET" ||
            funcName == "CLS" || funcName == "WAIT_FRAME" || funcName == "WAIT_FRAMES" || funcName == "WAIT_MS" ||
            funcName == "SLEEP") {
            isExternalFunc = true;
        } else if (!funcName.empty() && funcName.find('_') != std::string::npos) {
            // Check if all chars are uppercase, digits, or underscores
            isExternalFunc = true;
            for (char c : funcName) {
                if (!(std::isupper(c) || std::isdigit(c) || c == '_')) {
                    isExternalFunc = false;
                    break;
                }
            }
        }

        // Build function call (name already mangled in parser)
        std::string prefix = isExternalFunc ? "" : "func_";
        std::string actualFuncName = funcName;
        // Convert external functions to lowercase (they're registered that way in Lua)
        // Graphics commands map to gfx_* functions
        if (isExternalFunc) {
            if (funcName == "RECT") {
                actualFuncName = "gfx_rect_outline";
            } else if (funcName == "CIRCLE") {
                actualFuncName = "gfx_circle_outline";
            } else if (funcName == "CIRCLEF") {
                actualFuncName = "gfx_circle";
            } else if (funcName == "LINE") {
                actualFuncName = "gfx_line";
            } else if (funcName == "PSET") {
                actualFuncName = "gfx_point";
            } else if (funcName == "CLS") {
                actualFuncName = "text_clear";
            } else if (funcName == "WAIT_FRAME") {
                actualFuncName = "basic_wait_frame";
            } else if (funcName == "WAIT_FRAMES") {
                actualFuncName = "basic_wait_frames";
            } else if (funcName == "WAIT_MS") {
                actualFuncName = "basic_wait_ms";
            } else if (funcName == "SLEEP") {
                actualFuncName = "basic_sleep";
            } else {
                std::transform(actualFuncName.begin(), actualFuncName.end(), actualFuncName.begin(), ::tolower);
            }
        }
        std::string callExpr = prefix + actualFuncName + "(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) callExpr += ", ";
            callExpr += args[i];
        }
        callExpr += ")";

        if (isFunction) {
            // Function call - push result (or handle BYREF unpacking)
            if (hasByRef) {
                // Build list of variables to receive BYREF returns
                std::string lvalues;
                bool hasValidByRef = false;
                for (size_t i = 0; i < byrefFlags.size() && i < argVarNames.size(); i++) {
                    if (byrefFlags[i] && !argVarNames[i].empty()) {
                        if (!lvalues.empty()) lvalues += ", ";
                        lvalues += getVarName(argVarNames[i]);
                        hasValidByRef = true;
                    }
                }
                
                if (hasValidByRef) {
                    // Emit tuple unpacking: retval, byref1, byref2, ... = func(...)
                    emitLine("    local _retval");
                    emitLine("    _retval, " + lvalues + " = " + callExpr);
                    emitLine("    push(_retval)");
                } else {
                    // No valid BYREF variables, just call normally
                    emitLine("    push(" + callExpr + ")");
                }
            } else {
                // No BYREF - normal function call
                emitLine("    push(" + callExpr + ")");
            }
        } else {
            // SUB call - just execute (or handle BYREF unpacking)
            if (hasByRef) {
                // Build list of variables to receive BYREF returns
                std::string lvalues;
                bool hasValidByRef = false;
                for (size_t i = 0; i < byrefFlags.size() && i < argVarNames.size(); i++) {
                    if (byrefFlags[i] && !argVarNames[i].empty()) {
                        if (!lvalues.empty()) lvalues += ", ";
                        lvalues += getVarName(argVarNames[i]);
                        hasValidByRef = true;
                    }
                }
                
                if (hasValidByRef) {
                    // Emit tuple unpacking for SUB: byref1, byref2, ... = func(...)
                    emitLine("    " + lvalues + " = " + callExpr);
                } else {
                    // No valid BYREF variables, just call normally
                    emitLine("    " + callExpr);
                }
            } else {
                // No BYREF - normal SUB call
                emitLine("    " + callExpr);
            }
        }
    }
}

void LuaCodeGenerator::emitExit(const IRInstruction& instr) {
    flushExpressionToStack();

    switch (instr.opcode) {
        case IROpcode::EXIT_FOR:
        case IROpcode::EXIT_DO:
        case IROpcode::EXIT_WHILE:
        case IROpcode::EXIT_REPEAT: {
            // Exit from loop - emit break statement
            emitLine("    break");
            break;
        }

        case IROpcode::EXIT_FUNCTION: {
            // Exit from FUNCTION - return with current function result variable + BYREF params
            if (m_currentFunction) {
                if (m_config.useLuaJITHints) {
                    emitLine("    if samm then samm.exit_scope() end");
                }
                
                // Build return statement: function result + BYREF parameters
                std::string funcResultVar = "var_" + m_currentFunction->name;
                std::string returnValues = funcResultVar;  // Function return value
                
                // Append BYREF parameters
                for (size_t i = 0; i < m_currentFunction->parameters.size(); i++) {
                    if (i < m_currentFunction->parameterIsByRef.size() && 
                        m_currentFunction->parameterIsByRef[i]) {
                        returnValues += ", var_" + m_currentFunction->parameters[i];
                    }
                }
                
                emitLine("    return " + returnValues);
            } else {
                emitLine("    return");
            }
            break;
        }

        case IROpcode::EXIT_SUB: {
            // Exit from SUB - return BYREF parameters only
            if (m_currentFunction) {
                // Build return with BYREF parameters
                std::string returnValues;
                for (size_t i = 0; i < m_currentFunction->parameters.size(); i++) {
                    if (i < m_currentFunction->parameterIsByRef.size() && 
                        m_currentFunction->parameterIsByRef[i]) {
                        if (!returnValues.empty()) returnValues += ", ";
                        returnValues += "var_" + m_currentFunction->parameters[i];
                    }
                }
                
                if (!returnValues.empty()) {
                    emitLine("    return " + returnValues);
                } else {
                    emitLine("    return");
                }
            } else {
                emitLine("    return");
            }
            break;
        }

        default:
            break;
    }
}

void LuaCodeGenerator::emitTimer(const IRInstruction& instr) {
    flushExpressionToStack();

    switch (instr.opcode) {
        case IROpcode::AFTER_TIMER: {
            // AFTER timer: pop duration, call basic_timer_after
            if (!std::holds_alternative<std::string>(instr.operand1)) {
                return;
            }
            std::string handlerName = std::get<std::string>(instr.operand1);
            
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto durationExpr = m_exprOptimizer.pop();
                if (durationExpr) {
                    std::string duration = m_exprOptimizer.toString(durationExpr);
                    emitLine("    basic_timer_after(" + duration + ", \"" + handlerName + "\")");
                } else {
                    emitLine("    basic_timer_after(pop(), \"" + handlerName + "\")");
                }
            } else {
                emitLine("    basic_timer_after(pop(), \"" + handlerName + "\")");
            }
            break;
        }

        case IROpcode::EVERY_TIMER: {
            // EVERY timer: pop duration, call basic_timer_every
            if (!std::holds_alternative<std::string>(instr.operand1)) {
                return;
            }
            std::string handlerName = std::get<std::string>(instr.operand1);
            
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto durationExpr = m_exprOptimizer.pop();
                if (durationExpr) {
                    std::string duration = m_exprOptimizer.toString(durationExpr);
                    emitLine("    basic_timer_every(" + duration + ", \"" + handlerName + "\")");
                } else {
                    emitLine("    basic_timer_every(pop(), \"" + handlerName + "\")");
                }
            } else {
                emitLine("    basic_timer_every(pop(), \"" + handlerName + "\")");
            }
            break;
        }

        case IROpcode::AFTER_FRAMES: {
            // AFTERFRAMES: pop frame count, call basic_timer_after_frames
            if (!std::holds_alternative<std::string>(instr.operand1)) {
                return;
            }
            std::string handlerName = std::get<std::string>(instr.operand1);
            
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto countExpr = m_exprOptimizer.pop();
                if (countExpr) {
                    std::string count = m_exprOptimizer.toString(countExpr);
                    emitLine("    basic_timer_after_frames(" + count + ", \"" + handlerName + "\")");
                } else {
                    emitLine("    basic_timer_after_frames(pop(), \"" + handlerName + "\")");
                }
            } else {
                emitLine("    basic_timer_after_frames(pop(), \"" + handlerName + "\")");
            }
            break;
        }

        case IROpcode::EVERY_FRAMES: {
            // EVERYFRAME: pop frame count, call basic_timer_every_frame
            if (!std::holds_alternative<std::string>(instr.operand1)) {
                return;
            }
            std::string handlerName = std::get<std::string>(instr.operand1);
            
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto countExpr = m_exprOptimizer.pop();
                if (countExpr) {
                    std::string count = m_exprOptimizer.toString(countExpr);
                    emitLine("    basic_timer_every_frame(" + count + ", \"" + handlerName + "\")");
                } else {
                    emitLine("    basic_timer_every_frame(pop(), \"" + handlerName + "\")");
                }
            } else {
                emitLine("    basic_timer_every_frame(pop(), \"" + handlerName + "\")");
            }
            break;
        }

        case IROpcode::TIMER_STOP: {
            // TIMER STOP: can be by ID (from stack), handler name, or "ALL"
            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string target = std::get<std::string>(instr.operand1);
                emitLine("    basic_timer_stop(\"" + target + "\")");
            } else {
                // Timer ID is on the stack
                if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                    auto idExpr = m_exprOptimizer.pop();
                    if (idExpr) {
                        std::string timerId = m_exprOptimizer.toString(idExpr);
                        emitLine("    basic_timer_stop(" + timerId + ")");
                    } else {
                        emitLine("    basic_timer_stop(pop())");
                    }
                } else {
                    emitLine("    basic_timer_stop(pop())");
                }
            }
            break;
        }

        case IROpcode::TIMER_INTERVAL: {
            // TIMER INTERVAL: set debug hook interval
            if (std::holds_alternative<int>(instr.operand1)) {
                int interval = std::get<int>(instr.operand1);
                emitLine("    _set_timer_interval(" + std::to_string(interval) + ")");
            } else if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto intervalExpr = m_exprOptimizer.pop();
                if (intervalExpr) {
                    std::string interval = m_exprOptimizer.toString(intervalExpr);
                    emitLine("    _set_timer_interval(" + interval + ")");
                } else {
                    emitLine("    _set_timer_interval(pop())");
                }
            } else {
                emitLine("    _set_timer_interval(pop())");
            }
            break;
        }

        default:
            break;
    }
}

void LuaCodeGenerator::emitReturn(const IRInstruction& instr) {
    switch (instr.opcode) {
        case IROpcode::RETURN_VALUE: {
            // Return with value (for FUNCTION)
            // Must promote return value to parent scope, then exit current scope
            std::string returnValues;
            
            if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
                auto returnExpr = m_exprOptimizer.pop();
                if (returnExpr) {
                    returnValues = m_exprOptimizer.toString(returnExpr);
                } else {
                    flushExpressionToStack();
                    returnValues = popExpr();
                }
            } else {
                flushExpressionToStack();
                returnValues = popExpr();
            }
            
            // Append BYREF parameters to return
            if (m_currentFunction) {
                for (size_t i = 0; i < m_currentFunction->parameters.size(); i++) {
                    if (i < m_currentFunction->parameterIsByRef.size() && 
                        m_currentFunction->parameterIsByRef[i]) {
                        returnValues += ", " + getVarName(m_currentFunction->parameters[i]);
                    }
                }
            }
            
            emitLine("    return " + returnValues);
            break;
        }

        case IROpcode::RETURN_VOID: {
            // Return without value (for SUB)
            flushExpressionToStack();
            
            // Build return with BYREF parameters
            std::string returnValues;
            if (m_currentFunction) {
                bool first = true;
                for (size_t i = 0; i < m_currentFunction->parameters.size(); i++) {
                    if (i < m_currentFunction->parameterIsByRef.size() && 
                        m_currentFunction->parameterIsByRef[i]) {
                        if (!first) returnValues += ", ";
                        returnValues += getVarName(m_currentFunction->parameters[i]);
                        first = false;
                    }
                }
            }
            
            if (!returnValues.empty()) {
                emitLine("    return " + returnValues);
            } else {
                emitLine("    return");
            }
            break;
        }

        default:
            break;
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

void LuaCodeGenerator::emit(const std::string& code) {
    m_output << code;
}

void LuaCodeGenerator::emitLine(const std::string& code) {
    // Apply indentation offset for nested contexts (e.g., subroutines)
    if (m_indentOffset > 0 && !code.empty()) {
        m_output << std::string(m_indentOffset, ' ') << code << "\n";
    } else {
        m_output << code << "\n";
    }
    m_stats.linesGenerated++;
}

void LuaCodeGenerator::emitComment(const std::string& comment) {
    if (m_config.emitComments) {
        emitLine("    -- " + comment);
    }
}

void LuaCodeGenerator::emitLabel(const std::string& label) {
    emitLine("    ::" + getLabelName(label) + "::");
}

std::string LuaCodeGenerator::getVarName(const std::string& name) {
    // Convert BASIC variable name to valid Lua identifier
    std::string luaName = "var_" + name;
    // Replace invalid characters (like $ % # !) with underscore
    for (char& c : luaName) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    return luaName;
}

std::string LuaCodeGenerator::getArrayName(const std::string& name) {
    std::string luaName = "arr_" + name;
    for (char& c : luaName) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    return luaName;
}

std::string LuaCodeGenerator::getLabelName(const std::string& label) {
    std::string luaName = "label_" + label;
    for (char& c : luaName) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    return luaName;
}

std::string LuaCodeGenerator::escapeString(const std::string& str) {
    std::ostringstream oss;

    // In OPTION UNICODE mode, wrap string literals with unicode.from_utf8()
    if (m_unicodeMode) {
        oss << "unicode.unicode_from_utf8(\"";
    } else {
        oss << "\"";
    }

    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }

    if (m_unicodeMode) {
        oss << "\")";
    } else {
        oss << "\"";
    }

    return oss.str();
}

// =============================================================================
// TYPE Schema Generation for TYPENAME Parameters
// =============================================================================

std::string LuaCodeGenerator::generateTypeSchemaTable(const std::string& typeName) {
    // Look up TYPE in symbol table
    auto it = m_code->types.find(typeName);
    if (it == m_code->types.end()) {
        // TYPE not found - emit nil with error comment
        return "nil  -- ERROR: TYPE '" + typeName + "' not found";
    }
    
    const TypeSymbol& typeSymbol = it->second;
    
    // Generate inline schema table
    std::ostringstream schema;
    schema << "{fields={";
    
    for (size_t i = 0; i < typeSymbol.fields.size(); i++) {
        const auto& field = typeSymbol.fields[i];
        
        if (i > 0) schema << ",";
        
        schema << "{name=\"" << field.name << "\","
               << "sqltype=\"" << mapToSQLType(field.builtInType) << "\"}";
    }
    
    schema << "}}";
    return schema.str();
}

std::string LuaCodeGenerator::mapToSQLType(VariableType type) {
    switch (type) {
        case VariableType::INT: 
            return "INTEGER";
        case VariableType::FLOAT: 
        case VariableType::DOUBLE: 
            return "REAL";
        case VariableType::STRING:
        case VariableType::UNICODE: 
            return "TEXT";
        default: 
            return "TEXT";
    }
}

void LuaCodeGenerator::emitStringConcat(const IRInstruction& instr) {
    // String concatenation: pop 2 strings, push concatenation
    // Check IR opcode to determine which type of concat
    bool isUnicode = (instr.opcode == IROpcode::UNICODE_CONCAT);

    if (isUnicode) {
        // Unicode mode: use unicode.concat()
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto rightExpr = m_exprOptimizer.pop();
            auto leftExpr = m_exprOptimizer.pop();
            if (rightExpr && leftExpr) {
                std::string result = "unicode.unicode_concat(" + m_exprOptimizer.toString(leftExpr) + ", " +
                                    m_exprOptimizer.toString(rightExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    b = pop(); a = pop(); push(unicode.unicode_concat(a, b))");
            }
        } else {
            emitLine("    b = pop(); a = pop(); push(unicode.unicode_concat(a, b))");
        }
    } else {
        // Standard mode: use Lua's .. operator
        if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
            auto rightExpr = m_exprOptimizer.pop();
            auto leftExpr = m_exprOptimizer.pop();
            if (rightExpr && leftExpr) {
                std::string result = "(" + m_exprOptimizer.toString(leftExpr) + " .. " +
                                    m_exprOptimizer.toString(rightExpr) + ")";
                m_exprOptimizer.pushVariable(result);
            } else {
                emitLine("    b = pop(); a = pop(); push(a .. b)");
            }
        } else {
            emitLine("    b = pop(); a = pop(); push(a .. b)");
        }
    }
}

std::string LuaCodeGenerator::popExpr() {
    if (m_exprStack.empty()) {
        return "pop()";
    }
    auto entry = m_exprStack.back();
    m_exprStack.pop_back();
    return entry.expr;
}

void LuaCodeGenerator::pushExpr(const std::string& expr, bool isTemp) {
    m_exprStack.push_back({expr, isTemp});
}

std::string LuaCodeGenerator::allocTemp() {
    return "temp_" + std::to_string(m_tempVarCounter++);
}

void LuaCodeGenerator::freeTemp(const std::string& temp) {
    // Could track temp variable pool here
}

// =============================================================================
// Expression Optimizer Helpers
// =============================================================================

bool LuaCodeGenerator::canUseExpressionMode() const {
    // We can use expression mode if:
    // 1. Expression mode is enabled
    // 2. We're not in a complex control flow situation
    // 3. The expression optimizer doesn't have side effects
    return m_useExpressionMode;
}

// Enhanced side-effect analysis - determines if we can safely preserve expressions
bool LuaCodeGenerator::canPreserveExpressions(const IRInstruction& nextInstr) const {
    if (!m_useExpressionMode || m_exprOptimizer.isEmpty()) {
        return false;
    }

    // Check if the next instruction can work with expressions directly
    switch (nextInstr.opcode) {
        // Instructions that can consume expressions directly
        case IROpcode::STORE_VAR:
        case IROpcode::STORE_ARRAY:
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::MOD:
        case IROpcode::POW:
        case IROpcode::EQ:
        case IROpcode::NE:
        case IROpcode::LT:
        case IROpcode::LE:
        case IROpcode::GT:
        case IROpcode::GE:
        case IROpcode::AND:
        case IROpcode::OR:
        case IROpcode::XOR:
        case IROpcode::CALL_BUILTIN:
        case IROpcode::STR_CONCAT:
            return true;
            
        // Conditional jumps can use expressions for conditions
        case IROpcode::JUMP_IF_FALSE:
        case IROpcode::JUMP_IF_TRUE:
            return m_exprOptimizer.size() == 1; // Only if single expression for condition
            
        // These require flushing
        case IROpcode::REDIM_ARRAY:
        case IROpcode::ERASE_ARRAY:
            return false;  // Must flush - these are statements, not expressions

        case IROpcode::DIM_ARRAY:
        case IROpcode::ON_GOTO:
        case IROpcode::ON_GOSUB:
        case IROpcode::CALL_GOSUB:
        case IROpcode::RETURN_GOSUB:
            return false;
            
        default:
            // Conservative: assume we need to flush for unknown instructions
            return false;
    }
}

// Smart flush that only flushes when necessary
void LuaCodeGenerator::smartFlushExpressions(const IRInstruction& nextInstr) {
    if (!canPreserveExpressions(nextInstr)) {
        flushExpressionToStack();
    }
}

void LuaCodeGenerator::flushExpressionToStack() {
    // If we have expressions in the optimizer, we need to emit them as stack operations
    // Important: Collect all expressions first, then emit in reverse order to preserve stack order
    std::vector<std::shared_ptr<Expr>> exprs;
    while (!m_exprOptimizer.isEmpty()) {
        auto expr = m_exprOptimizer.pop();
        if (expr) {
            exprs.push_back(expr);
        }
    }

    // Emit in reverse order (bottom to top of original stack)
    for (auto it = exprs.rbegin(); it != exprs.rend(); ++it) {
        std::string code = m_exprOptimizer.toString(*it);
        emitLine("    push(" + code + ")");
    }
}

// =============================================================================
// Variable Access Analysis and Hot/Cold Caching
// =============================================================================

void LuaCodeGenerator::analyzeVariableAccess(const IRCode& irCode) {
    // First pass: count variable accesses and identify loop counters
    std::unordered_set<std::string> loopCounters;

    for (size_t i = 0; i < irCode.instructions.size(); i++) {
        const auto& instr = irCode.instructions[i];

        // Track FOR loop counters as always hot
        if (instr.opcode == IROpcode::FOR_INIT) {
            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string varName = std::get<std::string>(instr.operand1);
                loopCounters.insert(varName);
                m_variableAccess[varName].isLoopCounter = true;
            }
        }

        // Count LOAD_VAR and STORE_VAR accesses
        if (instr.opcode == IROpcode::LOAD_VAR || instr.opcode == IROpcode::STORE_VAR) {
            if (std::holds_alternative<std::string>(instr.operand1)) {
                std::string varName = std::get<std::string>(instr.operand1);
                m_variableAccess[varName].name = varName;
                m_variableAccess[varName].accessCount++;
            }
        }

        // Track variables used in INPUT operations (operand2 is the variable name)
        if (instr.opcode == IROpcode::INPUT || instr.opcode == IROpcode::INPUT_FILE ||
            instr.opcode == IROpcode::LINE_INPUT_FILE || instr.opcode == IROpcode::INPUT_AT) {
            if (std::holds_alternative<std::string>(instr.operand2)) {
                std::string varName = std::get<std::string>(instr.operand2);
                m_variableAccess[varName].name = varName;
                m_variableAccess[varName].accessCount++;
            } else if (std::holds_alternative<std::string>(instr.operand1)) {
                // For INPUT (non-file), the variable is in operand1
                if (instr.opcode == IROpcode::INPUT) {
                    std::string varName = std::get<std::string>(instr.operand1);
                    m_variableAccess[varName].name = varName;
                    m_variableAccess[varName].accessCount++;
                }
            }
        }
    }
}

void LuaCodeGenerator::selectHotVariables() {
    // Build list of candidates sorted by access count
    std::vector<std::pair<std::string, int>> candidates;

    for (const auto& pair : m_variableAccess) {
        const auto& info = pair.second;
        // Loop counters and frequently accessed vars are candidates
        if (info.isLoopCounter || info.accessCount > 1) {
            candidates.push_back({info.name, info.accessCount});
        }
    }

    // Sort by access count (descending)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Select top N as hot variables (respecting maxLocalVariables limit)
    int availableSlots = m_config.maxLocalVariables - m_usedLocalSlots;
    int hotCount = std::min(availableSlots, (int)candidates.size());

    for (int i = 0; i < hotCount; i++) {
        const std::string& varName = candidates[i].first;
        m_hotVariables.push_back(varName);
        m_variableAccess[varName].isHot = true;
    }

    m_usedLocalSlots += hotCount;

    // Assign integer IDs to cold variables for fast array access
    m_coldVariableIDs.clear();
    int nextColdID = 0;
    for (const auto& pair : m_variableAccess) {
        const std::string& varName = pair.first;
        if (!pair.second.isHot) {
            m_coldVariableIDs[varName] = nextColdID++;
        }
    }
}

bool LuaCodeGenerator::isHotVariable(const std::string& varName) {
    if (!m_config.useVariableCache) return true; // All locals if not caching

    // Function parameters are always hot (they're already function-local)
    if (m_currentFunction != nullptr) {
        for (const auto& param : m_currentFunction->parameters) {
            if (param == varName) {
                return true; // Parameters are always hot
            }
        }
    }

    auto it = m_variableAccess.find(varName);
    if (it != m_variableAccess.end()) {
        return it->second.isHot;
    }
    return false; // Unknown variables go to cold storage
}

std::string LuaCodeGenerator::getVariableReference(const std::string& varName) {
    if (!m_config.useVariableCache) {
        return getVarName(varName); // Original behavior
    }

    if (isHotVariable(varName)) {
        return getVarName(varName); // Local variable (fast)
    } else {
        // Use integer-indexed array (much faster than hash table)
        auto it = m_coldVariableIDs.find(varName);
        if (it != m_coldVariableIDs.end()) {
            return "vars[" + std::to_string(it->second) + "]"; // Array access: ~2-3 cycles
        } else {
            // Fallback for unknown variables (shouldn't happen)
            return "vars." + getVarName(varName); // Hash table: ~20 cycles
        }
    }
}

void LuaCodeGenerator::emitVariableTableDeclaration() {
    if (m_hotVariables.empty() && m_variableAccess.empty()) {
        return; // No variables to manage
    }

    emitLine("-- Hot variables (frequently accessed) cached as locals");
    if (!m_hotVariables.empty()) {
        // Declare all hot variables
        std::string hotDecl = "local ";
        for (size_t i = 0; i < m_hotVariables.size(); i++) {
            if (i > 0) hotDecl += ", ";
            hotDecl += getVarName(m_hotVariables[i]);
        }
        emitLine(hotDecl);

        // Initialize all hot variables to 0
        for (const auto& varName : m_hotVariables) {
            emitLine(getVarName(varName) + " = 0");
        }
    }

    // Emit cold variable storage (array-based for performance)
    if (m_variableAccess.size() > m_hotVariables.size()) {
        emitLine("-- Cold variables stored in array (unlimited capacity)");
        emitLine("local vars = {}");
    }
}

void LuaCodeGenerator::emitParameterPoolDeclaration() {
    emitLine("-- Parameter pool for modular commands (reduces local variable usage)");
    emitLine("-- Pool supports up to 20 parameters per command");
    emitLine("local param0, param1, param2, param3, param4, param5, param6, param7, param8, param9");
    emitLine("local param10, param11, param12, param13, param14, param15, param16, param17, param18, param19");
    emitLine("");
}

// =============================================================================
// User-Defined Type (Record/Structure) Operations
// =============================================================================

void LuaCodeGenerator::emitTypeDefinition(const IRInstruction& instr) {
    // TYPE declaration generates a constructor function
    // The constructor returns a table with all fields initialized to default values
    
    if (!std::holds_alternative<std::string>(instr.operand1)) {
        return;
    }
    
    std::string typeName = std::get<std::string>(instr.operand1);
    
    // Look up type definition in IRCode metadata
    auto it = m_code->types.find(typeName);
    if (it == m_code->types.end()) {
        emitComment("Warning: Type " + typeName + " not found in symbol table");
        return;
    }
    
    const TypeSymbol& typeSymbol = it->second;
    
    emitLine("");
    emitComment("Constructor for TYPE " + typeName);
    emitLine("local function " + typeName + "_new()");
    emitLine("    return {");
    
    // Initialize each field to its default value
    for (const auto& field : typeSymbol.fields) {
        std::string defaultValue;
        
        if (field.isBuiltIn) {
            switch (field.builtInType) {
                case VariableType::INT:
                    defaultValue = "0";
                    break;
                case VariableType::FLOAT:
                case VariableType::DOUBLE:
                    defaultValue = "0.0";
                    break;
                case VariableType::STRING:
                case VariableType::UNICODE:
                    defaultValue = m_unicodeMode ? "{}" : "\"\"";
                    break;
                default:
                    defaultValue = "nil";
                    break;
            }
        } else {
            // User-defined type - call its constructor
            defaultValue = field.typeName + "_new()";
        }
        
        emitLine("        " + field.name + " = " + defaultValue + ",");
    }
    
    emitLine("    }");
    emitLine("end");
    emitLine("");
}

void LuaCodeGenerator::emitLoadMember(const IRInstruction& instr) {
    // LOAD_MEMBER: pop record from stack, push member value
    // Stack-based: pop record, push record.member
    
    if (!std::holds_alternative<std::string>(instr.operand1)) {
        return;
    }
    
    std::string memberName = std::get<std::string>(instr.operand1);
    
    // Use expression optimizer if available
    if (canUseExpressionMode() && !m_exprOptimizer.isEmpty()) {
        // Pop record expression from optimizer
        auto recordExpr = m_exprOptimizer.pop();
        if (recordExpr) {
            std::string recordStr = m_exprOptimizer.toString(recordExpr);
            // Push member access expression back to optimizer
            m_exprOptimizer.pushVariable(recordStr + "." + memberName);
        } else {
            // Fallback to stack-based
            m_exprOptimizer.pushVariable("pop()." + memberName);
        }
    } else {
        // Fallback to stack-based approach
        std::string recordExpr = popExpr();
        std::string memberExpr = recordExpr + "." + memberName;
        pushExpr(memberExpr);
    }
}

void LuaCodeGenerator::emitStoreMember(const IRInstruction& instr) {
    // STORE_MEMBER: pop value, pop record, store value to record.member
    // In uplifted Lua: record.member = value
    
    if (!std::holds_alternative<std::string>(instr.operand1)) {
        return;
    }
    
    std::string memberName = std::get<std::string>(instr.operand1);
    
    std::string recordExpr;
    std::string valueExpr;
    
    // Use expression optimizer if available
    if (canUseExpressionMode() && m_exprOptimizer.size() >= 2) {
        // Pop record expression (top of stack)
        auto recordExprNode = m_exprOptimizer.pop();
        // Pop value expression (next on stack)
        auto valueExprNode = m_exprOptimizer.pop();
        
        if (recordExprNode && valueExprNode) {
            recordExpr = m_exprOptimizer.toString(recordExprNode);
            valueExpr = m_exprOptimizer.toString(valueExprNode);
        } else {
            // Fallback
            flushExpressionToStack();
            recordExpr = popExpr();
            valueExpr = popExpr();
        }
    } else {
        // Flush any pending expressions
        flushExpressionToStack();
        
        // Pop member expression (the record with any intermediate member accesses)
        recordExpr = popExpr();
        
        // Pop value expression
        valueExpr = popExpr();
    }
    
    // Generate assignment: record.member = value
    emitLine("    " + recordExpr + "." + memberName + " = " + valueExpr);
}

void LuaCodeGenerator::emitLoadArrayMember(const IRInstruction& instr) {
    // LOAD_ARRAY_MEMBER: pop indices, load array element member, push value
    // operand1 = array name
    // operand2 = member path (e.g., "Name" or "Position.X")
    // operand3 = dimension count (optional)
    
    if (!std::holds_alternative<std::string>(instr.operand1) ||
        !std::holds_alternative<std::string>(instr.operand2)) {
        return;
    }
    
    std::string arrayName = std::get<std::string>(instr.operand1);
    std::string memberPath = std::get<std::string>(instr.operand2);
    
    // Get dimension count (default to 1 for backward compatibility)
    int dims = 1;
    if (std::holds_alternative<int>(instr.operand3)) {
        dims = std::get<int>(instr.operand3);
    }
    
    // Flush any pending expressions to ensure indices are on stack
    flushExpressionToStack();
    
    // Pop indices into temporary variables
    std::vector<std::string> indexVars;
    for (int i = dims - 1; i >= 0; i--) {
        std::string idxVar = "idx" + std::to_string(i);
        emitLine("    " + idxVar + " = pop()");
        indexVars.insert(indexVars.begin(), idxVar);
    }
    
    // Build array access and load the member value
    if (dims == 1) {
        // For 1D arrays, handle both FFI and regular arrays
        emitLine("    if arr_" + arrayName + ".data then");
        emitLine("        push(arr_" + arrayName + ".data[" + indexVars[0] + "]." + memberPath + ")");
        emitLine("    else");
        emitLine("        push(arr_" + arrayName + "[" + indexVars[0] + "]." + memberPath + ")");
        emitLine("    end");
    } else {
        // For multi-dimensional arrays
        std::string arrayAccess = "arr_" + arrayName;
        for (const auto& idx : indexVars) {
            arrayAccess += "[" + idx + "]";
        }
        emitLine("    push(" + arrayAccess + "." + memberPath + ")");
    }
}

void LuaCodeGenerator::emitStoreArrayMember(const IRInstruction& instr) {
    // STORE_ARRAY_MEMBER: pop value, pop indices, store to array[index].member
    // operand1 = array name
    // operand2 = member path (e.g., "Name" or "Position.X")
    // operand3 = dimension count (optional)
    
    if (!std::holds_alternative<std::string>(instr.operand1) ||
        !std::holds_alternative<std::string>(instr.operand2)) {
        return;
    }
    
    std::string arrayName = std::get<std::string>(instr.operand1);
    std::string memberPath = std::get<std::string>(instr.operand2);
    
    // Get dimension count (default to 1 for backward compatibility)
    int dims = 1;
    if (std::holds_alternative<int>(instr.operand3)) {
        dims = std::get<int>(instr.operand3);
    }
    
    // Flush any pending expressions
    flushExpressionToStack();
    
    // Pop indices into local variables to avoid re-evaluation
    std::vector<std::string> indexVars;
    for (int i = 0; i < dims; i++) {
        std::string indexExpr = popExpr();
        std::string tempVar = "_tmp_idx_" + std::to_string(m_tempVarCounter++);
        emitLine("    local " + tempVar + " = " + indexExpr);
        indexVars.insert(indexVars.begin(), tempVar);
    }
    
    // Pop value expression
    std::string valueExpr = popExpr();
    
    // Build array access expression with all dimensions
    std::string arrayAccess;
    if (dims == 1) {
        // For 1D arrays
        arrayAccess = "arr_" + arrayName;
        // Check if it's an FFI array or regular table
        emitLine("    if arr_" + arrayName + ".data then");
        emitLine("        arr_" + arrayName + ".data[" + indexVars[0] + "]." + memberPath + " = " + valueExpr);
        emitLine("    else");
        emitLine("        arr_" + arrayName + "[" + indexVars[0] + "]." + memberPath + " = " + valueExpr);
        emitLine("    end");
        return;
    } else {
        // For multi-dimensional arrays
        arrayAccess = "arr_" + arrayName;
        for (const auto& idx : indexVars) {
            arrayAccess += "[" + idx + "]";
        }
        // Generate assignment: array[index1][index2]...member = value
        emitLine("    " + arrayAccess + "." + memberPath + " = " + valueExpr);
    }
}

void LuaCodeGenerator::emitSwap(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1) ||
        !std::holds_alternative<std::string>(instr.operand2)) {
        return;
    }
    
    std::string var1 = getVarName(std::get<std::string>(instr.operand1));
    std::string var2 = getVarName(std::get<std::string>(instr.operand2));
    
    // Use Lua multiple assignment to swap: var1, var2 = var2, var1
    emitLine("    " + var1 + ", " + var2 + " = " + var2 + ", " + var1);
}

void LuaCodeGenerator::emitRedim(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;
    
    std::string arrayName = std::get<std::string>(instr.operand1);
    std::string luaArrayName = getArrayName(arrayName);
    
    int dims = 1;
    if (std::holds_alternative<int>(instr.operand2)) {
        dims = std::get<int>(instr.operand2);
    }
    
    bool preserve = false;
    if (std::holds_alternative<int>(instr.operand3)) {
        preserve = (std::get<int>(instr.operand3) != 0);
    }
    
    if (dims == 1) {
        emitLine("    dim = pop()");
        
        if (preserve) {
            // REDIM PRESERVE - keep existing data
            emitLine("    local old_array = " + luaArrayName);
            emitLine("    " + luaArrayName + " = {}");
            if (m_arrayBase == 0) {
                emitLine("    for i = 0, dim do");
                emitLine("        " + luaArrayName + "[i + 1] = old_array[i + 1] or 0");
                emitLine("    end");
            } else {
                emitLine("    for i = 1, dim + 1 do");
                emitLine("        " + luaArrayName + "[i] = old_array[i] or 0");
                emitLine("    end");
            }
        } else {
            // REDIM without PRESERVE - clear and resize
            emitLine("    " + luaArrayName + " = {}");
            if (m_arrayBase == 0) {
                emitLine("    for i = 0, dim do " + luaArrayName + "[i + 1] = 0 end");
            } else {
                emitLine("    for i = 1, dim + 1 do " + luaArrayName + "[i] = 0 end");
            }
        }
    } else {
        // Multi-dimensional REDIM - more complex
        for (int i = dims - 1; i >= 0; i--) {
            emitLine("    dim" + std::to_string(i) + " = pop()");
        }
        
        if (preserve) {
            emitLine("    -- REDIM PRESERVE for multi-dimensional arrays not fully supported");
            emitLine("    -- Original data may be lost");
        }
        
        emitLine("    " + luaArrayName + " = {}");
        std::string indent = "    ";
        for (int d = 0; d < dims; d++) {
            std::string loopVar = "i" + std::to_string(d);
            int startIdx = m_arrayBase;
            emitLine(indent + "for " + loopVar + " = " + std::to_string(startIdx) + 
                    ", " + std::to_string(startIdx) + " + dim" + std::to_string(d) + " do");
            indent += "  ";
            if (d < dims - 1) {
                std::string tableAccess = luaArrayName;
                for (int k = 0; k <= d; k++) {
                    tableAccess += "[i" + std::to_string(k) + "]";
                }
                emitLine(indent + "if not " + tableAccess + " then " + tableAccess + " = {} end");
            } else {
                std::string tableAccess = luaArrayName;
                for (int k = 0; k <= d; k++) {
                    tableAccess += "[i" + std::to_string(k) + "]";
                }
                emitLine(indent + tableAccess + " = 0");
            }
        }
        for (int d = 0; d < dims; d++) {
            indent = indent.substr(0, indent.length() - 2);
            emitLine(indent + "end");
        }
    }
}

void LuaCodeGenerator::emitErase(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;
    
    std::string arrayName = std::get<std::string>(instr.operand1);
    std::string luaArrayName = getArrayName(arrayName);
    
    // Clear the array by setting it to empty table
    emitLine("    " + luaArrayName + " = {}");
}

void LuaCodeGenerator::emitArrayBounds(const IRInstruction& instr) {
    if (!std::holds_alternative<std::string>(instr.operand1)) return;
    
    std::string arrayName = std::get<std::string>(instr.operand1);
    std::string luaArrayName = getArrayName(arrayName);
    
    int dimension = 1;
    if (std::holds_alternative<int>(instr.operand2)) {
        dimension = std::get<int>(instr.operand2);
    }
    
    if (instr.opcode == IROpcode::LBOUND_ARRAY) {
        // LBOUND returns the lower bound (typically 0 or 1 based on OPTION BASE)
        emitLine("    push(" + std::to_string(m_arrayBase) + ")");
    } else {
        // UBOUND returns the upper bound
        // For Lua arrays, we need to find the highest index
        if (dimension == 1) {
            emitLine("    local max_idx = 0");
            emitLine("    for k, v in pairs(" + luaArrayName + ") do");
            emitLine("        if type(k) == 'number' and k > max_idx then max_idx = k end");
            emitLine("    end");
            if (m_arrayBase == 0) {
                emitLine("    push(max_idx - 1)  -- Convert from 1-based to 0-based");
            } else {
                emitLine("    push(max_idx - 1)  -- Adjust for OPTION BASE 1");
            }
        } else {
            // Multi-dimensional arrays are more complex
            emitLine("    push(0)  -- UBOUND for dimension > 1 not fully implemented");
        }
    }
}

void LuaCodeGenerator::emitSIMD(const IRInstruction& instr) {
    // SIMD operations format:
    // operand1: result array name
    // operand2: source array A name
    // operand3: source array B name (or scalar for scale/add_scalar/sub_scalar operations)
    
    if (!std::holds_alternative<std::string>(instr.operand1) ||
        !std::holds_alternative<std::string>(instr.operand2)) {
        return;
    }
    
    std::string resultArray = std::get<std::string>(instr.operand1);
    std::string sourceArrayA = std::get<std::string>(instr.operand2);
    std::string luaResultArray = getArrayName(resultArray);
    std::string luaSourceA = getArrayName(sourceArrayA);
    
    // Determine operation type and parameters
    bool isPair = false;
    std::string opName;
    bool needsScalar = false;
    bool needsArrayB = false;
    
    switch (instr.opcode) {
        case IROpcode::SIMD_PAIR_ARRAY_ADD:
            isPair = true;
            opName = "pair_array_add";
            needsArrayB = true;
            break;
        case IROpcode::SIMD_PAIR_ARRAY_SUB:
            isPair = true;
            opName = "pair_array_sub";
            needsArrayB = true;
            break;
        case IROpcode::SIMD_PAIR_ARRAY_SCALE:
            isPair = true;
            opName = "pair_array_scale";
            needsScalar = true;
            break;
        case IROpcode::SIMD_PAIR_ARRAY_ADD_SCALAR:
            isPair = true;
            opName = "pair_array_add_scalar";
            needsScalar = true;
            break;
        case IROpcode::SIMD_PAIR_ARRAY_SUB_SCALAR:
            isPair = true;
            opName = "pair_array_sub_scalar";
            needsScalar = true;
            break;
        case IROpcode::SIMD_QUAD_ARRAY_ADD:
            isPair = false;
            opName = "quad_array_add";
            needsArrayB = true;
            break;
        case IROpcode::SIMD_QUAD_ARRAY_SUB:
            isPair = false;
            opName = "quad_array_sub";
            needsArrayB = true;
            break;
        case IROpcode::SIMD_QUAD_ARRAY_SCALE:
            isPair = false;
            opName = "quad_array_scale";
            needsScalar = true;
            break;
        case IROpcode::SIMD_QUAD_ARRAY_ADD_SCALAR:
            isPair = false;
            opName = "quad_array_add_scalar";
            needsScalar = true;
            break;
        case IROpcode::SIMD_QUAD_ARRAY_SUB_SCALAR:
            isPair = false;
            opName = "quad_array_sub_scalar";
            needsScalar = true;
            break;
        default:
            return;
    }
    
    // Mark that we use SIMD operations (for requiring the module in header)
    m_usesSIMD = true;
    
    emitLine("    -- SIMD operation: " + opName);
    emitLine("    do");
    
    if (needsArrayB) {
        // Binary array operation (A() + B())
        if (!std::holds_alternative<std::string>(instr.operand3)) {
            emitLine("    end");
            return;
        }
        std::string sourceArrayB = std::get<std::string>(instr.operand3);
        std::string luaSourceB = getArrayName(sourceArrayB);
        
        emitLine("        if _SIMD and _SIMD.is_available() then");
        emitLine("            -- Get array lengths (assuming same size)");
        emitLine("            local count = #" + luaSourceA);
        emitLine("            -- Check if arrays are FFI-backed with aligned pointers");
        emitLine("            if " + luaResultArray + ".data and " + luaSourceA + ".data and " + luaSourceB + ".data then");
        emitLine("                -- Use native SIMD acceleration");
        emitLine("                _SIMD." + opName + "(" + luaResultArray + ".data, " + luaSourceA + ".data, " + luaSourceB + ".data, count)");
        emitLine("            else");
        emitLine("                -- Fallback to Lua implementation");
        emitLine("                _SIMD." + opName + "_fallback(" + luaResultArray + ", " + luaSourceA + ", " + luaSourceB + ", count)");
        emitLine("            end");
        emitLine("        else");
        emitLine("            -- SIMD not available, use pure Lua fallback");
        emitLine("            local count = #" + luaSourceA);
        emitLine("            for i = 1, count do");
        emitLine("                " + luaResultArray + "[i] = " + luaSourceA + "[i] + " + luaSourceB + "[i]");
        emitLine("            end");
        emitLine("        end");
    } else if (needsScalar) {
        // Scalar operation (A() * scalar)
        emitLine("        local scalar = pop()  -- Get scalar value from stack");
        emitLine("        if _SIMD and _SIMD.is_available() then");
        emitLine("            local count = #" + luaSourceA);
        emitLine("            if " + luaResultArray + ".data and " + luaSourceA + ".data then");
        emitLine("                -- Use native SIMD acceleration");
        emitLine("                _SIMD." + opName + "(" + luaResultArray + ".data, " + luaSourceA + ".data, scalar, count)");
        emitLine("            else");
        emitLine("                -- Fallback to Lua implementation");
        emitLine("                _SIMD." + opName + "_fallback(" + luaResultArray + ", " + luaSourceA + ", scalar, count)");
        emitLine("            end");
        emitLine("        else");
        emitLine("            -- SIMD not available, use pure Lua fallback");
        emitLine("            local count = #" + luaSourceA);
        emitLine("            for i = 1, count do");
        emitLine("                " + luaResultArray + "[i] = " + luaSourceA + "[i] * scalar");
        emitLine("            end");
        emitLine("        end");
    }
    
    emitLine("    end");
}

} // namespace FasterBASIC
