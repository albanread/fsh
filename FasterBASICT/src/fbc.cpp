//
// FasterBASIC Compiler (fbc)
// Compiles BASIC source code to optimized Lua for LuaJIT execution
//

#include "fasterbasic_lexer.h"
#include "fasterbasic_parser.h"
#include "fasterbasic_semantic.h"
#include "fasterbasic_optimizer.h"
#include "fasterbasic_peephole.h"
#include "fasterbasic_cfg.h"
#include "fasterbasic_ircode.h"
#include "fasterbasic_lua_codegen.h"
#include "fasterbasic_data_preprocessor.h"
#include "modular_commands.h"
#include "command_registry_core.h"
#include "plugin_loader.h"
#include "../runtime/data_lua_bindings.h"
#include "../runtime/terminal_lua_bindings.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <chrono>
#include <csignal>
#include <atomic>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Runtime module registration functions
extern "C" void register_unicode_module(lua_State* L);
extern "C" void register_bitwise_module(lua_State* L);
extern "C" void register_constants_module(lua_State* L);
extern "C" void set_constants_manager(FasterBASIC::ConstantsManager* manager);

// File I/O bindings
namespace FasterBASIC {
    void register_fileio_functions(lua_State* L);
    void clear_fileio_state();
    void registerDataBindings(lua_State* L);
    void registerTerminalBindings(lua_State* L);
    void registerTimerBindings(lua_State* L);
}

using namespace FasterBASIC;
using namespace FasterBASIC::ModularCommands;

// Global flag for script interruption (set by SIGINT handler)
static std::atomic<bool> g_shouldStopScript(false);

// Copy text to macOS clipboard using pbcopy
void copyToClipboard(const std::string& text) {
    FILE* pipe = popen("pbcopy", "w");
    if (pipe) {
        fwrite(text.c_str(), sizeof(char), text.length(), pipe);
        pclose(pipe);
    }
}

// Extract BASIC line number and clean error message for clipboard
std::string formatErrorForClipboard(const std::string& luaError) {
    // The error format can be:
    // "[string ...]:nnn: Runtime error at BASIC line N: [string ...]:nnn: actual error message"
    // We want to extract: "Runtime error at BASIC line N: actual error message"
    
    size_t linePos = luaError.find("Runtime error at BASIC line ");
    if (linePos != std::string::npos) {
        // Find where "BASIC line N" ends
        size_t lineNumStart = linePos + 28; // Length of "Runtime error at BASIC line "
        size_t firstColon = luaError.find(":", lineNumStart);
        
        if (firstColon != std::string::npos) {
            // Extract "Runtime error at BASIC line N"
            std::string lineInfo = luaError.substr(linePos, firstColon - linePos);
            
            // Get everything after the colon
            std::string remainder = luaError.substr(firstColon + 1);
            
            // Trim leading whitespace from remainder first
            while (!remainder.empty() && (isspace(remainder.front()) || remainder.front() == '\n')) {
                remainder.erase(0, 1);
            }
            
            // Strip all [string "..."]:nnn: prefixes recursively
            std::string errorMsg = remainder;
            size_t bracketPos;
            while ((bracketPos = errorMsg.find("[string ")) != std::string::npos) {
                // Find the closing ]: 
                size_t endPos = errorMsg.find("]: ", bracketPos);
                if (endPos != std::string::npos) {
                    // Remove the [string "..."]:nnn: part
                    errorMsg = errorMsg.substr(endPos + 3);
                } else {
                    break;
                }
            }
            
            // Trim whitespace
            while (!errorMsg.empty() && (isspace(errorMsg.back()) || errorMsg.back() == '\n')) {
                errorMsg.pop_back();
            }
            while (!errorMsg.empty() && (isspace(errorMsg.front()) || errorMsg.front() == '\n')) {
                errorMsg.erase(0, 1);
            }
            
            return lineInfo + ": " + errorMsg;
        }
    }
    
    // If no BASIC line number found, return the original error
    return luaError;
}

// Signal handler for Ctrl+C (SIGINT)
void signalHandler(int signal) {
    if (signal == SIGINT) {
        g_shouldStopScript.store(true);
        std::cerr << "\n^C (Interrupted by user)\n";
    }
}

// Lua binding for shouldStopScript()
static int lua_shouldStopScript(lua_State* L) {
    lua_pushboolean(L, g_shouldStopScript.load());
    return 1;
}

void initializeFBCCommandRegistry() {
    // Initialize global registry with core commands for compiler use
    CommandRegistry& registry = getGlobalCommandRegistry();
    

    
    // Add core BASIC commands and functions
    CoreCommandRegistry::registerCoreCommands(registry);
    CoreCommandRegistry::registerCoreFunctions(registry);
    
    // Load plugins from plugins/enabled directory
    FasterBASIC::PluginSystem::initializeGlobalPluginLoader(registry);
    
    // Mark registry as initialized to prevent clearing
    markGlobalRegistryInitialized();
    
    // Add compiler-specific commands if needed (future enhancement)
    // For now, fbc uses only core commands
}

void printUsage(const char* programName) {
    std::cerr << "FasterBASIC Compiler and Runner - Compiles and runs BASIC programs\n\n";
    std::cerr << "Usage: " << programName << " [options] <input.bas>\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -o <file>      Write Lua output to <file> and exit (compile-only mode)\n";
    std::cerr << "  -p <file>      Write preprocessed BASIC to <file> and exit\n";
    std::cerr << "  -l <file>      Write BASIC with line numbers converted to labels and exit\n";
    std::cerr << "  -t             Time program execution and display elapsed time\n";
    std::cerr << "  -c             Emit comments in generated Lua\n";
    std::cerr << "  -v, --verbose  Verbose output (compilation stats)\n";
    std::cerr << "  -h, --help     Show this help message\n";
    std::cerr << "  --profile      Show detailed timing for each compilation phase\n";
    std::cerr << "\nOptimization Options:\n";
    std::cerr << "  --opt-ast      Enable AST optimizer (constant folding, dead code)\n";
    std::cerr << "  --opt-peep     Enable peephole optimizer (IR-level optimizations)\n";
    std::cerr << "  --opt-all      Enable all optimizers (AST + peephole)\n";
    std::cerr << "  --opt-stats    Show detailed optimization statistics\n";
    std::cerr << "\nBehavior:\n";
    std::cerr << "  Default:       Compile and run program immediately (no optimizers)\n";
    std::cerr << "  With -o:       Compile to file only (no execution)\n";
    std::cerr << "  With -t:       Compile, run, and show execution time\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << programName << " program.bas              # Compile and run\n";
    std::cerr << "  " << programName << " -t program.bas           # Compile, run, and time\n";
    std::cerr << "  " << programName << " --profile prog.bas       # Show compilation phase timings\n";
    std::cerr << "  " << programName << " --opt-all -t prog.bas    # With optimizers + timing\n";
    std::cerr << "  " << programName << " -o program.lua prog.bas  # Compile to file only\n";
    std::cerr << "  " << programName << " -p preprocessed.bas p.bas # Preprocess only (strip REMs)\n";
    std::cerr << "  " << programName << " -l labeled.bas prog.bas   # Convert line numbers to labels\n";
}

int main(int argc, char** argv) {
    // Initialize modular commands registry
    initializeFBCCommandRegistry();
    
    std::string inputFile;
    std::string outputFile;
    std::string preprocessOutputFile;
    std::string labelOutputFile;
    bool emitComments = false;
    bool verbose = false;
    bool timeExecution = false;
    bool enableASTOptimizer = false;
    bool enablePeepholeOptimizer = false;
    bool showOptStats = false;
    bool showProfile = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0) {
            emitComments = true;
        } else if (strcmp(argv[i], "-t") == 0) {
            timeExecution = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--opt-ast") == 0) {
            enableASTOptimizer = true;
        } else if (strcmp(argv[i], "--opt-peep") == 0) {
            enablePeepholeOptimizer = true;
        } else if (strcmp(argv[i], "--opt-all") == 0) {
            enableASTOptimizer = true;
            enablePeepholeOptimizer = true;
        } else if (strcmp(argv[i], "--opt-stats") == 0) {
            showOptStats = true;
        } else if (strcmp(argv[i], "--profile") == 0) {
            showProfile = true;
            verbose = true;  // Auto-enable verbose for profiling
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o requires an output filename\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                preprocessOutputFile = argv[++i];
            } else {
                std::cerr << "Error: -p requires an output filename\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 < argc) {
                labelOutputFile = argv[++i];
            } else {
                std::cerr << "Error: -l requires an output filename\n";
                return 1;
            }
        } else if (argv[i][0] == '-') {
            std::cerr << "Error: Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            if (inputFile.empty()) {
                inputFile = argv[i];
            } else {
                std::cerr << "Error: Multiple input files specified\n";
                return 1;
            }
        }
    }
    
    if (inputFile.empty()) {
        std::cerr << "Error: No input file specified\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        auto compileStartTime = std::chrono::high_resolution_clock::now();
        auto phaseStartTime = compileStartTime;
        
        // Read source file
        if (verbose) {
            std::cerr << "Reading: " << inputFile << "\n";
        }
        
        std::ifstream file(inputFile);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << inputFile << "\n";
            return 1;
        }
        
        std::string source((std::istreambuf_iterator<char>(file)), 
                          std::istreambuf_iterator<char>());
        file.close();
        
        if (verbose) {
            std::cerr << "Source size: " << source.length() << " bytes\n";
        }
        
        auto readEndTime = std::chrono::high_resolution_clock::now();
        double readMs = std::chrono::duration<double, std::milli>(readEndTime - phaseStartTime).count();
        
        // Preprocess REM statements (strip comment text to simplify parsing)
        if (verbose) {
            std::cerr << "Preprocessing REM statements...\n";
        }
        source = DataPreprocessor::preprocessREM(source);
        
        // Preprocess line numbers to labels (convert GOTO/GOSUB targets to symbolic labels)
        if (verbose) {
            std::cerr << "Converting line numbers to labels...\n";
        }
        source = DataPreprocessor::preprocessLineNumbersToLabels(source);
        
        // If -p option was specified, save preprocessed output and exit
        if (!preprocessOutputFile.empty()) {
            std::ofstream outFile(preprocessOutputFile);
            if (!outFile) {
                std::cerr << "Error: Could not open output file: " << preprocessOutputFile << "\n";
                return 1;
            }
            outFile << source;
            outFile.close();
            
            if (verbose) {
                std::cerr << "Preprocessed source written to: " << preprocessOutputFile << "\n";
            }
            return 0;
        }
        
        // If -l option was specified, convert line numbers to labels and exit
        if (!labelOutputFile.empty()) {
            std::string labeled = DataPreprocessor::preprocessLineNumbersToLabels(source);
            
            std::ofstream outFile(labelOutputFile);
            if (!outFile) {
                std::cerr << "Error: Could not open output file: " << labelOutputFile << "\n";
                return 1;
            }
            outFile << labeled;
            outFile.close();
            
            if (verbose) {
                std::cerr << "Line numbers converted to labels, written to: " << labelOutputFile << "\n";
            }
            return 0;
        }
        
        // Lexical analysis
        phaseStartTime = std::chrono::high_resolution_clock::now();
        if (verbose) {
            std::cerr << "Lexing...\n";
        }
        
        Lexer lexer;
        lexer.tokenize(source);
        auto tokens = lexer.getTokens();
        
        auto lexEndTime = std::chrono::high_resolution_clock::now();
        double lexMs = std::chrono::duration<double, std::milli>(lexEndTime - phaseStartTime).count();
        
        if (verbose) {
            std::cerr << "Tokens: " << tokens.size() << "\n";
        }
        
        // Parsing
        phaseStartTime = std::chrono::high_resolution_clock::now();
        if (verbose) {
            std::cerr << "Parsing...\n";
        }
        
        // Create semantic analyzer early to get ConstantsManager
        SemanticAnalyzer semantic;
        
        // Ensure constants are loaded before parsing (for fast constant lookup)
        semantic.ensureConstantsLoaded();
        
        Parser parser;
        parser.setConstantsManager(&semantic.getConstantsManager());
        auto ast = parser.parse(tokens, inputFile);
        
        auto parseEndTime = std::chrono::high_resolution_clock::now();
        double parseMs = std::chrono::duration<double, std::milli>(parseEndTime - phaseStartTime).count();
        
        // Check for parser errors - if parsing failed, don't continue
        if (!ast || parser.hasErrors()) {
            std::cerr << "\nParsing failed with errors:\n";
            for (const auto& error : parser.getErrors()) {
                std::cerr << "  " << error.toString() << "\n";
            }
            std::cerr << "Compilation aborted.\n";
            return 1;
        }
        
        // Get compiler options from OPTION statements (collected during parsing)
        const auto& compilerOptions = parser.getOptions();
        
        if (verbose) {
            std::cerr << "Program lines: " << ast->lines.size() << "\n";
            std::cerr << "Compiler options: arrayBase=" << compilerOptions.arrayBase 
                      << " unicodeMode=" << compilerOptions.unicodeMode << "\n";
        }
        
        // Semantic analysis (semantic analyzer already created earlier)
        phaseStartTime = std::chrono::high_resolution_clock::now();
        if (verbose) {
            std::cerr << "Semantic analysis...\n";
        }
        
        semantic.analyze(*ast, compilerOptions);
        
        auto semanticEndTime = std::chrono::high_resolution_clock::now();
        double semanticMs = std::chrono::duration<double, std::milli>(semanticEndTime - phaseStartTime).count();
        
        if (verbose) {
            const auto& symTable = semantic.getSymbolTable();
            size_t varCount = symTable.variables.size();
            size_t funcCount = symTable.functions.size();
            size_t labelCount = symTable.lineNumbers.size();
            std::cerr << "Symbols: " << varCount << " variables, " 
                     << funcCount << " functions, " << labelCount << " labels\n";
        }
        
        // AST Optimization (constant folding, dead code elimination)
        double astOptMs = 0.0;
        if (enableASTOptimizer) {
            phaseStartTime = std::chrono::high_resolution_clock::now();
            if (verbose) {
                std::cerr << "Optimizing AST...\n";
            }
            
            ASTOptimizer astOptimizer;
            astOptimizer.setOptimizationLevel(1);
            astOptimizer.optimize(*ast, semantic.getSymbolTable());
            
            auto astOptEndTime = std::chrono::high_resolution_clock::now();
            astOptMs = std::chrono::duration<double, std::milli>(astOptEndTime - phaseStartTime).count();
            
            if (verbose || showOptStats) {
                std::cerr << astOptimizer.generateReport();
            }
        }
        
        // Control flow graph
        phaseStartTime = std::chrono::high_resolution_clock::now();
        if (verbose) {
            std::cerr << "Building CFG...\n";
        }
        
        CFGBuilder cfgBuilder;
        auto cfg = cfgBuilder.build(*ast, semantic.getSymbolTable());
        
        auto cfgEndTime = std::chrono::high_resolution_clock::now();
        double cfgMs = std::chrono::duration<double, std::milli>(cfgEndTime - phaseStartTime).count();
        
        if (verbose) {
            std::cerr << "CFG blocks: " << cfg->blocks.size() << "\n";
        }
        
        // IR generation
        phaseStartTime = std::chrono::high_resolution_clock::now();
        if (verbose) {
            std::cerr << "Generating IR...\n";
        }
        
        IRGenerator irGen;
        auto irCode = irGen.generate(*cfg, semantic.getSymbolTable());
        
        auto irEndTime = std::chrono::high_resolution_clock::now();
        double irMs = std::chrono::duration<double, std::milli>(irEndTime - phaseStartTime).count();
        
        if (verbose) {
            std::cerr << "IR instructions: " << irCode->instructions.size() << "\n";
        }
        
        // Peephole Optimization (IR-level optimizations)
        double peepholeMs = 0.0;
        if (enablePeepholeOptimizer) {
            phaseStartTime = std::chrono::high_resolution_clock::now();
            if (verbose) {
                std::cerr << "Running peephole optimizer...\n";
            }
            
            PeepholeOptimizer peepholeOpt;
            peepholeOpt.setOptimizationLevel(1);
            peepholeOpt.optimize(*irCode);
            
            auto peepholeEndTime = std::chrono::high_resolution_clock::now();
            peepholeMs = std::chrono::duration<double, std::milli>(peepholeEndTime - phaseStartTime).count();
            
            if (verbose || showOptStats) {
                std::cerr << peepholeOpt.generateReport();
            }
            
            if (verbose) {
                std::cerr << "IR instructions after peephole: " << irCode->instructions.size() << "\n";
            }
        }
        
        // Lua code generation
        phaseStartTime = std::chrono::high_resolution_clock::now();
        if (verbose) {
            std::cerr << "Generating Lua code...\n";
        }
        
        LuaCodeGenConfig config;
        config.emitComments = emitComments;
        LuaCodeGenerator luaGen(config);
        std::string luaCode = luaGen.generate(*irCode);
        
        auto codegenEndTime = std::chrono::high_resolution_clock::now();
        double codegenMs = std::chrono::duration<double, std::milli>(codegenEndTime - phaseStartTime).count();
        
        if (verbose) {
            std::cerr << "Generated Lua size: " << luaCode.length() << " bytes\n";
        }
        
        auto compileEndTime = std::chrono::high_resolution_clock::now();
        double totalCompileMs = std::chrono::duration<double, std::milli>(compileEndTime - compileStartTime).count();
        
        // Show detailed profiling if requested
        if (showProfile) {
            std::cerr << "\n=== Compilation Phase Timing ===\n";
            std::cerr << "  File I/O:          " << std::fixed << std::setprecision(3) << readMs << " ms\n";
            std::cerr << "  Lexer:             " << std::fixed << std::setprecision(3) << lexMs << " ms\n";
            std::cerr << "  Parser:            " << std::fixed << std::setprecision(3) << parseMs << " ms\n";
            std::cerr << "  Semantic:          " << std::fixed << std::setprecision(3) << semanticMs << " ms\n";
            if (enableASTOptimizer) {
                std::cerr << "  AST Optimizer:     " << std::fixed << std::setprecision(3) << astOptMs << " ms\n";
            }
            std::cerr << "  CFG Builder:       " << std::fixed << std::setprecision(3) << cfgMs << " ms\n";
            std::cerr << "  IR Generator:      " << std::fixed << std::setprecision(3) << irMs << " ms\n";
            if (enablePeepholeOptimizer) {
                std::cerr << "  Peephole Opt:      " << std::fixed << std::setprecision(3) << peepholeMs << " ms\n";
            }
            std::cerr << "  Lua CodeGen:       " << std::fixed << std::setprecision(3) << codegenMs << " ms\n";
            std::cerr << "  --------------------------------\n";
            std::cerr << "  Total Compile:     " << std::fixed << std::setprecision(3) << totalCompileMs << " ms\n";
            
            // Calculate percentages
            std::cerr << "\n=== Percentage Breakdown ===\n";
            std::cerr << "  File I/O:          " << std::fixed << std::setprecision(1) << (readMs / totalCompileMs * 100) << "%\n";
            std::cerr << "  Lexer:             " << std::fixed << std::setprecision(1) << (lexMs / totalCompileMs * 100) << "%\n";
            std::cerr << "  Parser:            " << std::fixed << std::setprecision(1) << (parseMs / totalCompileMs * 100) << "%\n";
            std::cerr << "  Semantic:          " << std::fixed << std::setprecision(1) << (semanticMs / totalCompileMs * 100) << "%\n";
            if (enableASTOptimizer) {
                std::cerr << "  AST Optimizer:     " << std::fixed << std::setprecision(1) << (astOptMs / totalCompileMs * 100) << "%\n";
            }
            std::cerr << "  CFG Builder:       " << std::fixed << std::setprecision(1) << (cfgMs / totalCompileMs * 100) << "%\n";
            std::cerr << "  IR Generator:      " << std::fixed << std::setprecision(1) << (irMs / totalCompileMs * 100) << "%\n";
            if (enablePeepholeOptimizer) {
                std::cerr << "  Peephole Opt:      " << std::fixed << std::setprecision(1) << (peepholeMs / totalCompileMs * 100) << "%\n";
            }
            std::cerr << "  Lua CodeGen:       " << std::fixed << std::setprecision(1) << (codegenMs / totalCompileMs * 100) << "%\n";
            std::cerr << "\n";
        }
        
        // If output file is specified, write to file and exit (compile-only mode)
        if (!outputFile.empty()) {
            if (verbose) {
                std::cerr << "Writing: " << outputFile << "\n";
            }
            
            std::ofstream outFile(outputFile);
            if (!outFile.is_open()) {
                std::cerr << "Error: Cannot write to file: " << outputFile << "\n";
                return 1;
            }
            outFile << luaCode;
            outFile.close();
            
            if (verbose) {
                std::cerr << "Compilation successful!\n";
            }
            
            return 0;
        }
        
        // Default behavior: compile and run via embedded LuaJIT
        if (verbose) {
            std::cerr << "Compilation successful! Running program...\n";
        }
        
        // Create Lua state
        lua_State* L = luaL_newstate();
        if (!L) {
            std::cerr << "Error: Cannot create Lua state\n";
            return 1;
        }
        
        // Open standard libraries
        luaL_openlibs(L);
        
        // Register runtime modules (unicode, bitwise, constants, file I/O) directly in Lua state
        // This makes them always available without needing external shared libraries
        register_unicode_module(L);
        register_bitwise_module(L);
        register_constants_module(L);
        
        // Add runtime directory to Lua's package.path so modules can require() each other
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "path");
        std::string currentPath = lua_tostring(L, -1);
        lua_pop(L, 1);
        std::string newPath = currentPath + ";./runtime/?.lua;./runtime/?/init.lua";
        lua_pushstring(L, newPath.c_str());
        lua_setfield(L, -2, "path");
        lua_pop(L, 1);
        
        // Load plugin runtime files into Lua state
        // NOTE: Files are loaded with luaL_dofile() in the order specified by each plugin.
        // Multi-file plugins must register their modules via package.loaded['module_name']
        // at the TOP of each file (before any code that might require them).
        // Functions should be defined as global (function name()) not table members (function M.name())
        // so they are accessible from generated BASIC code.
        // See plugin_loader.cpp header comments for detailed plugin development guidelines.
        auto runtimeFiles = FasterBASIC::PluginSystem::getGlobalPluginLoader().getRequiredRuntimeFiles();
        for (const auto& file : runtimeFiles) {
            std::string filepath = "runtime/" + file;
            if (luaL_dofile(L, filepath.c_str()) != 0) {
                std::cerr << "Warning: Failed to load plugin runtime: " << filepath << std::endl;
                if (lua_isstring(L, -1)) {
                    std::cerr << "  Error: " << lua_tostring(L, -1) << std::endl;
                    lua_pop(L, 1);
                }
            }
        }
        
        // Copy constants from semantic analyzer to runtime
        set_constants_manager(&semantic.getConstantsManager());
        
        FasterBASIC::register_fileio_functions(L);
        FasterBASIC::registerDataBindings(L);
        FasterBASIC::registerTerminalBindings(L);
        FasterBASIC::registerTimerBindings(L);
        
        // Register shouldStopScript for Ctrl+C interruption
        lua_pushcfunction(L, lua_shouldStopScript);
        lua_setglobal(L, "shouldStopScript");
        
        // Install signal handler for Ctrl+C
        std::signal(SIGINT, signalHandler);
        
        // Reset the stop flag before running
        g_shouldStopScript.store(false);
        
        // Initialize DATA segment from IR code
        if (!irCode->dataValues.empty()) {
            FasterBASIC::initializeDataManager(irCode->dataValues);
            
            // Set up restore points
            for (const auto& entry : irCode->dataLineRestorePoints) {
                FasterBASIC::addDataRestorePoint(entry.first, entry.second);
            }
            
            for (const auto& entry : irCode->dataLabelRestorePoints) {
                FasterBASIC::addDataRestorePointByLabel(entry.first, entry.second);
            }
        }
        
        // Register stub functions for standalone mode (no graphics/terminal)
        // Note: CLS is now handled by terminal bindings, so we don't need the stub
        
        // Note: basic_print is now handled by terminal bindings
        
        // Execute the program
        int exitCode = 0;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Load and execute the Lua code
        if (luaL_loadstring(L, luaCode.c_str()) != 0) {
            std::cerr << "Error loading Lua code: " << lua_tostring(L, -1) << "\n";
            lua_close(L);
            return 1;
        }
        
        if (lua_pcall(L, 0, 0, 0) != 0) {
            std::string errorMsg = lua_tostring(L, -1);
            std::cerr << errorMsg << "\n";
            
            // Copy formatted error message to clipboard
            std::string clipboardMsg = formatErrorForClipboard(errorMsg);
            copyToClipboard(clipboardMsg);
            std::cerr << "(Error copied to clipboard)\n";
            
            exitCode = 1;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        
        // Clean up Lua state
        lua_close(L);
        
        // Display timing if requested
        if (timeExecution) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            double seconds = duration.count() / 1000000.0;
            std::cerr << "\nExecution time: " << seconds << " seconds\n";
        }
        
        return exitCode;
        
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("Compilation error: ") + e.what();
        std::cerr << errorMsg << "\n";
        
        // Copy error message to clipboard
        copyToClipboard(errorMsg);
        std::cerr << "(Error copied to clipboard)\n";
        
        return 1;
    }
}