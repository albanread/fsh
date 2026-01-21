//
// basic_formatter_lib.h
// FasterBASIC - BASIC Code Formatter Library Interface
//
// Library interface for formatting BASIC code with renumbering and indentation.
// This can be used by editors, IDEs, and other tools.
//

#ifndef BASIC_FORMATTER_LIB_H
#define BASIC_FORMATTER_LIB_H

#include <string>
#include <map>

namespace FasterBASIC {

namespace ModularCommands {
    class CommandRegistry;
}

// Forward declarations
class SourceDocument;
class REPLView;
class ConstantsManager;

// =============================================================================
// Formatter Options
// =============================================================================

struct FormatterOptions {
    int start_line;          // Starting line number (default: 1000)
    int step;                // Line number increment (default: 10)
    int indent_spaces;       // Spaces per indent level (default: 2)
    bool update_references;  // Update GOTO/GOSUB/RESTORE (default: true)
    bool add_indentation;    // Add control flow indentation (default: true)
    
    // Constructor with defaults
    FormatterOptions() 
        : start_line(1000)
        , step(10)
        , indent_spaces(2)
        , update_references(true)
        , add_indentation(true)
    {}
    
    // Preset configurations
    static FormatterOptions Classic() {
        FormatterOptions opts;
        opts.start_line = 10;
        opts.step = 10;
        return opts;
    }
    
    static FormatterOptions Modern() {
        FormatterOptions opts;
        opts.start_line = 1000;
        opts.step = 10;
        return opts;
    }
    
    static FormatterOptions Tight() {
        FormatterOptions opts;
        opts.start_line = 100;
        opts.step = 1;
        return opts;
    }
    
    static FormatterOptions RenumberOnly() {
        FormatterOptions opts;
        opts.add_indentation = false;
        return opts;
    }
    
    static FormatterOptions IndentOnly() {
        FormatterOptions opts;
        opts.update_references = false;
        opts.start_line = 0;  // Keep original line numbers
        return opts;
    }
};

// =============================================================================
// Formatter Result
// =============================================================================

struct FormatterResult {
    bool success;                          // True if formatting succeeded
    std::string formatted_code;            // The formatted BASIC code
    std::string error_message;             // Error message if failed
    int lines_processed;                   // Number of lines processed
    std::map<int, int> line_number_map;    // Old line -> new line mapping
    
    FormatterResult() 
        : success(false)
        , lines_processed(0)
    {}
};

// =============================================================================
// Main Formatter Functions
// =============================================================================

/// Format BASIC code with renumbering and indentation
/// @param source_code The BASIC source code to format
/// @param options Formatter options (line numbers, indentation, etc.)
/// @param registry Optional command registry for plugin command recognition
/// @param constants Optional constants manager to prevent uppercasing constants
/// @return FormatterResult with formatted code or error
FormatterResult formatBasicCode(const std::string& source_code, 
                                 const FormatterOptions& options = FormatterOptions(),
                                 const FasterBASIC::ModularCommands::CommandRegistry* registry = nullptr,
                                 const ConstantsManager* constants = nullptr);

/// Format BASIC code in-place (convenience function)
/// @param source_code The BASIC source code (will be modified)
/// @param options Formatter options
/// @param registry Optional command registry for plugin command recognition
/// @param constants Optional constants manager to prevent uppercasing constants
/// @return True if successful, false otherwise
bool formatBasicCodeInPlace(std::string& source_code,
                            const FormatterOptions& options = FormatterOptions(),
                            const FasterBASIC::ModularCommands::CommandRegistry* registry = nullptr,
                            const ConstantsManager* constants = nullptr);

/// Quick format with preset (convenience functions)
FormatterResult formatClassic(const std::string& source_code);
FormatterResult formatModern(const std::string& source_code);
FormatterResult formatTight(const std::string& source_code);

/// Renumber only (no indentation changes)
FormatterResult renumberBasicCode(const std::string& source_code,
                                   int start_line = 1000,
                                   int step = 10);

/// Remove line numbers (strip all line numbers from code)
FormatterResult removeLineNumbers(const std::string& source_code);

/// Indent only (no line number changes)
FormatterResult indentBasicCode(const std::string& source_code);

// =============================================================================
// SourceDocument/REPLView Convenience Functions
// =============================================================================

/// Format SourceDocument in-place
/// @param document The document to format
/// @param options Formatter options
/// @return True if successful, false otherwise
bool formatDocument(SourceDocument& document, 
                   const FormatterOptions& options = FormatterOptions());

/// Format REPLView in-place
/// @param view The REPL view to format
/// @param options Formatter options
/// @return True if successful, false otherwise
bool formatREPLView(REPLView& view,
                   const FormatterOptions& options = FormatterOptions());

/// Renumber SourceDocument
/// @param document The document to renumber
/// @param start_line Starting line number
/// @param step Step between line numbers
/// @return True if successful, false otherwise
bool renumberDocument(SourceDocument& document, int start_line = 10, int step = 10);

/// Renumber REPLView
/// @param view The REPL view to renumber
/// @param start_line Starting line number
/// @param step Step between line numbers
/// @return True if successful, false otherwise
bool renumberREPLView(REPLView& view, int start_line = 10, int step = 10);

// =============================================================================
// Validation Functions
// =============================================================================

/// Check if code has valid line numbers
bool hasValidLineNumbers(const std::string& source_code);

/// Count lines with line numbers
int countNumberedLines(const std::string& source_code);

/// Detect the current line number range
/// @param source_code The BASIC source code
/// @param out_min Minimum line number found
/// @param out_max Maximum line number found
/// @return True if any numbered lines found
bool detectLineNumberRange(const std::string& source_code, 
                          int& out_min, 
                          int& out_max);

} // namespace FasterBASIC

#endif // BASIC_FORMATTER_LIB_H