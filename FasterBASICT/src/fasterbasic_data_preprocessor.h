//
// fasterbasic_data_preprocessor.h
// FasterBASIC - DATA Preprocessor
//
// Extracts and parses DATA statements before main parsing.
// This simplifies the compiler by handling DATA early and removing
// DATA lines from the source before the parser sees them.
//

#ifndef FASTERBASIC_DATA_PREPROCESSOR_H
#define FASTERBASIC_DATA_PREPROCESSOR_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <variant>

namespace FasterBASIC {

// Typed data value (same as DataManager uses)
using DataValue = std::variant<int, double, std::string>;

// Result of DATA preprocessing
struct DataPreprocessorResult {
    std::vector<DataValue> values;                      // All DATA values in order
    std::map<int, size_t> lineRestorePoints;            // Line number -> index
    std::map<std::string, size_t> labelRestorePoints;   // Label name -> index
    std::map<std::string, int> labelDefinitions;        // Label name -> line number (for symbol table)
    std::string cleanedSource;                          // Source with DATA lines removed
    
    DataPreprocessorResult() = default;
};

// DATA Preprocessor
// Scans source code for DATA statements, extracts and parses them,
// then removes them from the source before main parsing
class DataPreprocessor {
public:
    DataPreprocessor();
    ~DataPreprocessor();
    
    // Process source code and extract DATA
    // Returns cleaned source (without DATA lines) and collected DATA values
    DataPreprocessorResult process(const std::string& source);
    
    // Preprocess REM statements - strips comment text but keeps line number
    // Converts "1820 REM This is a comment" to "1820 REM"
    // This simplifies parsing by avoiding complex comment text parsing
    static std::string preprocessREM(const std::string& source);
    
    // Preprocess line numbers to labels for GOTO/GOSUB targets
    // Two-pass process:
    //   Pass 1: Collect all GOTO/GOSUB/ON GOTO/RESTORE target line numbers
    //   Pass 2: Convert those lines to labels (e.g., "60 PRINT" -> ":L60 PRINT")
    //           and convert GOTO references (e.g., "GOTO 60" -> "GOTO L60")
    // This simplifies the parser and makes GOTO resolution trivial
    static std::string preprocessLineNumbersToLabels(const std::string& source);
    
private:
    // Parse a single data value string into typed variant
    DataValue parseValue(const std::string& raw);
    
    // Check if a line is a DATA statement
    bool isDataLine(const std::string& line);
    
    // Extract line number from a BASIC line (if present)
    int extractLineNumber(const std::string& line, size_t& pos);
    
    // Extract label from a BASIC line (if present)
    std::string extractLabel(const std::string& line, size_t& pos);
    
    // Parse DATA values from the DATA statement
    std::vector<std::string> extractDataValues(const std::string& line, size_t dataPos);
    
    // Trim whitespace from both ends
    std::string trim(const std::string& str);
    
    // Check if character is whitespace
    static bool isWhitespace(char c);
    
    // Helper for REM preprocessing
    static bool isREMLine(const std::string& line, size_t pos);
    
    // Helpers for line number to label preprocessing
    static std::set<int> collectGotoTargets(const std::string& source);
    static int extractLineNumber(const std::string& line);
    static std::string convertLineNumbersToLabels(const std::string& source, 
                                                   const std::set<int>& targets);
    static std::string replaceNumbersAfterKeyword(const std::string& line,
                                                   size_t startPos,
                                                   const std::set<int>& targets,
                                                   bool onlyFirst = false);
};

} // namespace FasterBASIC

#endif // FASTERBASIC_DATA_PREPROCESSOR_H