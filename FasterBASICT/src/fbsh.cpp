//
// fbsh.cpp
// FasterBASIC Shell - Interactive BASIC Shell
//
// Main executable for the FasterBASIC interactive shell.
// Provides a classic BASIC programming environment with line numbers,
// immediate mode, and traditional commands like LIST, RUN, LOAD, SAVE.
//

#include "../shell/shell_core.h"
#include "../shell/help_database.h"
#include "../runtime/terminal_io.h"
#include "modular_commands.h"
#include "command_registry_core.h"
#include "plugin_loader.h"
#include "fasterbasic_data_preprocessor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

using namespace FasterBASIC;
using namespace FasterBASIC::ModularCommands;

void initializeFBSHCommandRegistry() {
    // Initialize global registry with core commands for shell use
    CommandRegistry& registry = getGlobalCommandRegistry();
    
    // Add core BASIC commands and functions
    CoreCommandRegistry::registerCoreCommands(registry);
    CoreCommandRegistry::registerCoreFunctions(registry);
    
    // Load plugins from plugins/enabled directory
    FasterBASIC::PluginSystem::initializeGlobalPluginLoader(registry);
    
    // Mark registry as initialized to prevent clearing
    markGlobalRegistryInitialized();
    
    // Add shell-specific I/O commands
    // CLS - Clear terminal screen (Unix terminal implementation)
    CommandDefinition cls("CLS",
                         "Clear the terminal screen",
                         "basic_cls", "io");
    registry.registerCommand(std::move(cls));
    
    // Add shell-specific commands (LIST, RUN, LOAD, SAVE, etc.)
    // TODO: Create shell command registry for commands like:
    // - LIST (list program lines)
    // - RUN (execute program)
    // - LOAD/SAVE (file operations)
    // - NEW (clear program)
    // - AUTO (auto line numbering)
    // - RENUM (renumber lines)
}

void showUsage(const char* programName) {
    std::cout << "FasterBASIC Shell v1.0 - Interactive BASIC Programming Environment\n";
    std::cout << "\n";
    std::cout << "Usage: " << programName << " [options] [file.bas]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help     Show this help message\n";
    std::cout << "  -v, --verbose  Enable verbose output\n";
    std::cout << "  -d, --debug    Enable debug mode\n";
    std::cout << "  -r, --run      Load and immediately run the program (then exit)\n";
    std::cout << "  -o <file>      Compile to Lua file and exit (no execution)\n";
    std::cout << "  -p <out> <in>  Preprocess BASIC file (strip REMs) and exit\n";
    std::cout << "  -f <in> [out]  Format BASIC file and write to output (or stdout)\n";
    std::cout << "  --version      Show version information\n";
    std::cout << "\n";
    std::cout << "If a .bas file is specified, it will be loaded automatically.\n";
    std::cout << "\n";
    std::cout << "Interactive Commands:\n";
    std::cout << "  LIST           List program lines\n";
    std::cout << "  LIST 10-50     List lines 10 through 50\n";
    std::cout << "  RUN            Execute the program\n";
    std::cout << "  NEW            Clear current program\n";
    std::cout << "  LOAD \"file\"    Load program from file\n";
    std::cout << "  SAVE \"file\"    Save program to file\n";
    std::cout << "  AUTO           Enable automatic line numbering\n";
    std::cout << "  RENUM          Renumber program lines\n";
    std::cout << "  HELP           Show help information\n";
    std::cout << "  QUIT           Exit the shell\n";
    std::cout << "\n";
    std::cout << "Program Entry:\n";
    std::cout << "  10 PRINT \"Hello\"   Add/replace line 10\n";
    std::cout << "  10               Delete line 10\n";
    std::cout << "\n";
}

void showVersion() {
    std::cout << "FasterBASIC Shell v1.0\n";
    std::cout << "Built on " << __DATE__ << " at " << __TIME__ << "\n";
    std::cout << "Copyright (c) 2024 FasterBASIC Project\n";
    std::cout << "\n";
    std::cout << "Features:\n";
    std::cout << "  - Interactive BASIC programming\n";
    std::cout << "  - Line-based program entry\n";
    std::cout << "  - Classic BASIC commands (LIST, RUN, LOAD, SAVE)\n";
    std::cout << "  - Terminal I/O with colors and positioning\n";
    std::cout << "  - LuaJIT-powered execution\n";
    std::cout << "  - Cross-platform compatibility\n";
}

void showWelcome() {
    std::cout << "FasterBASIC 2025\nReady.\n";
}

int main(int argc, char* argv[]) {
    // Initialize modular commands registry
    initializeFBSHCommandRegistry();
    
    // Initialize help database
    FasterBASIC::HelpSystem::initializeGlobalHelpDatabase();
    
    bool verbose = false;
    bool debug = false;
    bool autoRun = false;
    std::string loadFile;
    std::string outputFile;
    std::string preprocessOutputFile;
    std::string preprocessInputFile;
    std::string formatInputFile;
    std::string formatOutputFile;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            showUsage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            showVersion();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-d" || arg == "--debug") {
            debug = true;
        } else if (arg == "-r" || arg == "--run") {
            autoRun = true;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o requires output filename\n";
                return 1;
            }
        } else if (arg == "-p") {
            if (i + 2 < argc) {
                preprocessOutputFile = argv[++i];
                preprocessInputFile = argv[++i];
            } else {
                std::cerr << "Error: -p requires output and input filenames\n";
                return 1;
            }
        } else if (arg == "-f") {
            if (i + 1 < argc) {
                formatInputFile = argv[++i];
                // Check if there's an optional output file
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    formatOutputFile = argv[++i];
                }
            } else {
                std::cerr << "Error: -f requires input filename\n";
                return 1;
            }
        } else if (arg.length() > 0 && arg[0] != '-') {
            // Assume it's a filename to auto-load
            loadFile = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return 1;
        }
    }
    
    // Handle format mode (non-interactive)
    if (!formatInputFile.empty()) {
        try {
            // Read input file
            std::ifstream inFile(formatInputFile);
            if (!inFile) {
                std::cerr << "Error: Could not open input file: " << formatInputFile << "\n";
                return 1;
            }
            
            std::string source((std::istreambuf_iterator<char>(inFile)),
                              std::istreambuf_iterator<char>());
            inFile.close();
            
            if (verbose) {
                std::cout << "Formatting BASIC code...\n";
            }
            
            // Format the code
            FasterBASIC::FormatterOptions options;
            options.start_line = 1000;
            options.step = 10;
            options.indent_spaces = 2;
            options.update_references = true;
            options.add_indentation = true;
            
            const FasterBASIC::ModularCommands::CommandRegistry& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
            FasterBASIC::FormatterResult result = FasterBASIC::formatBasicCode(source, options, &registry);
            
            if (!result.success) {
                std::cerr << "Error: Formatting failed: " << result.error_message << "\n";
                return 1;
            }
            
            // Write output
            if (formatOutputFile.empty()) {
                // Write to stdout
                std::cout << result.formatted_code;
            } else {
                // Write to file
                std::ofstream outFile(formatOutputFile);
                if (!outFile) {
                    std::cerr << "Error: Could not open output file: " << formatOutputFile << "\n";
                    return 1;
                }
                
                outFile << result.formatted_code;
                outFile.close();
                
                if (verbose) {
                    std::cout << "Formatted code written to: " << formatOutputFile << "\n";
                }
            }
            
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error during formatting: " << e.what() << "\n";
            return 1;
        }
    }
    
    // Handle preprocessing mode (non-interactive)
    if (!preprocessOutputFile.empty()) {
        try {
            // Read input file
            std::ifstream inFile(preprocessInputFile);
            if (!inFile) {
                std::cerr << "Error: Could not open input file: " << preprocessInputFile << "\n";
                return 1;
            }
            
            std::string source((std::istreambuf_iterator<char>(inFile)),
                              std::istreambuf_iterator<char>());
            inFile.close();
            
            if (verbose) {
                std::cout << "Preprocessing REM statements...\n";
            }
            
            // Preprocess
            std::string preprocessed = FasterBASIC::DataPreprocessor::preprocessREM(source);
            
            // Write output
            std::ofstream outFile(preprocessOutputFile);
            if (!outFile) {
                std::cerr << "Error: Could not open output file: " << preprocessOutputFile << "\n";
                return 1;
            }
            
            outFile << preprocessed;
            outFile.close();
            
            if (verbose) {
                std::cout << "Preprocessed source written to: " << preprocessOutputFile << "\n";
            }
            
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error during preprocessing: " << e.what() << "\n";
            return 1;
        }
    }
    
    // Handle compile-only mode (non-interactive)
    if (!outputFile.empty()) {
        if (loadFile.empty()) {
            std::cerr << "Error: -o requires an input .bas file\n";
            return 1;
        }
        
        try {
            ShellCore shell;
            shell.setVerbose(verbose);
            shell.setDebug(debug);
            
            // Load the file
            if (!shell.loadProgram(loadFile)) {
                std::cerr << "Error: Failed to load program: " << loadFile << "\n";
                return 1;
            }
            
            if (verbose) {
                std::cout << "Compiling to: " << outputFile << "\n";
            }
            
            // Compile and save
            if (!shell.compileToFile(outputFile)) {
                std::cerr << "Error: Failed to compile to file: " << outputFile << "\n";
                return 1;
            }
            
            if (verbose) {
                std::cout << "Successfully compiled to: " << outputFile << "\n";
            }
            
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error during compilation: " << e.what() << "\n";
            return 1;
        }
    }
    
    try {
        // Initialize the shell
        ShellCore shell;
        shell.setVerbose(verbose);
        shell.setDebug(debug);
        
        if (verbose) {
            std::cout << "Starting FasterBASIC Shell...\n";
            std::cout << "Verbose mode: ON\n";
            if (debug) {
                std::cout << "Debug mode: ON\n";
            }
        }
        
        // Show welcome message
        showWelcome();
        
        // Auto-load file if specified
        if (!loadFile.empty()) {
            std::cout << "Loading \"" << loadFile << "\"...\n";
            if (shell.loadProgram(loadFile)) {
                std::cout << "Program loaded successfully.\n";
                
                // Auto-run if requested
                if (autoRun) {
                    std::cout << "Running program...\n\n";
                    shell.runProgram();
                    // Exit after running
                    return 0;
                }
            } else {
                std::cout << "Failed to load program.\n";
                if (autoRun) {
                    return 1;
                }
            }
            std::cout << "\n";
        }
        
        // Run the interactive shell
        shell.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
    
    if (verbose) {
        std::cout << "FasterBASIC Shell terminated normally.\n";
    }
    
    return 0;
}