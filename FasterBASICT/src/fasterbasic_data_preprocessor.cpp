//
// fasterbasic_data_preprocessor.cpp
// FasterBASIC - DATA Preprocessor Implementation
//
// Extracts and parses DATA statements before main parsing.
// This simplifies the compiler by handling DATA early and removing
// DATA lines from the source before the parser sees them.
//

#include "fasterbasic_data_preprocessor.h"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace FasterBASIC {

DataPreprocessor::DataPreprocessor() {
}

DataPreprocessor::~DataPreprocessor() {
}

// Parse a single data value string into typed variant
// Uses same logic as DataManager::parseValue for consistency
DataValue DataPreprocessor::parseValue(const std::string& raw) {
    // Empty string stays as string
    if (raw.empty()) {
        return raw;
    }
    
    // Trim leading/trailing whitespace
    std::string trimmed = trim(raw);
    if (trimmed.empty()) {
        return std::string("");
    }
    
    // Check for quoted strings (either single or double quotes)
    if (trimmed.length() >= 2) {
        char firstChar = trimmed[0];
        char lastChar = trimmed[trimmed.length() - 1];
        
        if ((firstChar == '"' && lastChar == '"') || 
            (firstChar == '\'' && lastChar == '\'')) {
            // Return the content without quotes as a string
            return trimmed.substr(1, trimmed.length() - 2);
        }
    }
    
    // Try to parse as integer first
    const char* str = trimmed.c_str();
    char* endptr = nullptr;
    
    // strtol handles leading +/- and whitespace
    long intValue = std::strtol(str, &endptr, 10);
    
    // If entire string was consumed and no overflow, it's an int
    if (endptr != str && *endptr == '\0') {
        // Additional check: make sure it doesn't have a decimal point or exponent
        bool hasDecimal = (trimmed.find('.') != std::string::npos);
        bool hasExponent = (trimmed.find('e') != std::string::npos || 
                           trimmed.find('E') != std::string::npos);
        
        if (!hasDecimal && !hasExponent) {
            return static_cast<int>(intValue);
        }
    }
    
    // Try to parse as floating point (handles scientific notation)
    endptr = nullptr;
    double doubleValue = std::strtod(str, &endptr);
    
    // If entire string was consumed, it's a double
    if (endptr != str && *endptr == '\0') {
        return doubleValue;
    }
    
    // Otherwise, it's a string (return trimmed version)
    return trimmed;
}

// Trim whitespace from both ends
std::string DataPreprocessor::trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && isWhitespace(str[start])) {
        start++;
    }
    while (end > start && isWhitespace(str[end - 1])) {
        end--;
    }
    
    if (start >= end) {
        return "";
    }
    
    return str.substr(start, end - start);
}

// Check if character is whitespace (static)
bool DataPreprocessor::isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Extract line number from a BASIC line (if present)
int DataPreprocessor::extractLineNumber(const std::string& line, size_t& pos) {
    // Skip leading whitespace
    while (pos < line.length() && isWhitespace(line[pos])) {
        pos++;
    }
    
    // Check if we have digits
    if (pos >= line.length() || !std::isdigit(line[pos])) {
        return -1;
    }
    
    // Parse line number
    int lineNum = 0;
    while (pos < line.length() && std::isdigit(line[pos])) {
        lineNum = lineNum * 10 + (line[pos] - '0');
        pos++;
    }
    
    // Skip whitespace after line number
    while (pos < line.length() && isWhitespace(line[pos])) {
        pos++;
    }
    
    return lineNum;
}

// Extract label from a BASIC line (if present)
std::string DataPreprocessor::extractLabel(const std::string& line, size_t& pos) {
    // Check for colon (label marker)
    if (pos >= line.length() || line[pos] != ':') {
        return "";
    }
    
    pos++; // Skip colon
    
    // Extract label name (alphanumeric + underscore)
    std::string label;
    while (pos < line.length() && 
           (std::isalnum(line[pos]) || line[pos] == '_')) {
        label += line[pos];
        pos++;
    }
    
    // Skip whitespace after label
    while (pos < line.length() && isWhitespace(line[pos])) {
        pos++;
    }
    
    return label;
}

// Check if a line contains a DATA statement
bool DataPreprocessor::isDataLine(const std::string& line) {
    size_t pos = 0;
    
    // Skip line number if present
    while (pos < line.length() && isWhitespace(line[pos])) pos++;
    while (pos < line.length() && std::isdigit(line[pos])) pos++;
    while (pos < line.length() && isWhitespace(line[pos])) pos++;
    
    // Skip label if present
    if (pos < line.length() && line[pos] == ':') {
        pos++;
        while (pos < line.length() && 
               (std::isalnum(line[pos]) || line[pos] == '_')) {
            pos++;
        }
        while (pos < line.length() && isWhitespace(line[pos])) pos++;
    }
    
    // Check for DATA keyword (case insensitive)
    if (pos + 4 > line.length()) {
        return false;
    }
    
    std::string keyword;
    for (size_t i = 0; i < 4 && pos + i < line.length(); i++) {
        keyword += std::toupper(line[pos + i]);
    }
    
    if (keyword != "DATA") {
        return false;
    }
    
    // Make sure DATA is not part of a larger word
    size_t afterData = pos + 4;
    if (afterData < line.length() && 
        (std::isalnum(line[afterData]) || line[afterData] == '_')) {
        return false;
    }
    
    return true;
}

// Parse DATA values from the DATA statement
std::vector<std::string> DataPreprocessor::extractDataValues(const std::string& line, size_t dataPos) {
    std::vector<std::string> values;
    
    // Find the DATA keyword and skip past it
    size_t pos = dataPos;
    while (pos < line.length()) {
        char c = std::toupper(line[pos]);
        if (c == 'D') {
            // Check if this is "DATA"
            if (pos + 4 <= line.length()) {
                std::string keyword;
                for (size_t i = 0; i < 4; i++) {
                    keyword += std::toupper(line[pos + i]);
                }
                if (keyword == "DATA") {
                    pos += 4;
                    break;
                }
            }
        }
        pos++;
    }
    
    // Skip whitespace after DATA
    while (pos < line.length() && isWhitespace(line[pos])) {
        pos++;
    }
    
    // Parse comma-separated values
    std::string currentValue;
    bool inQuotes = false;
    char quoteChar = '\0';
    
    while (pos < line.length()) {
        char c = line[pos];
        
        // Check for string literals
        if (!inQuotes && (c == '"' || c == '\'')) {
            inQuotes = true;
            quoteChar = c;
            currentValue += c;
            pos++;
            continue;
        }
        
        if (inQuotes) {
            currentValue += c;
            if (c == quoteChar) {
                inQuotes = false;
                quoteChar = '\0';
            }
            pos++;
            continue;
        }
        
        // Check for comma separator (not in quotes)
        if (c == ',' && !inQuotes) {
            // End of current value
            values.push_back(currentValue);
            currentValue.clear();
            pos++;
            
            // Skip whitespace after comma
            while (pos < line.length() && isWhitespace(line[pos])) {
                pos++;
            }
            continue;
        }
        
        // Check for comment (REM or ')
        if (!inQuotes) {
            if (c == '\'') {
                // Rest of line is comment
                break;
            }
            // Check for REM
            if (std::toupper(c) == 'R' && pos + 3 <= line.length()) {
                std::string word;
                for (size_t i = 0; i < 3; i++) {
                    word += std::toupper(line[pos + i]);
                }
                if (word == "REM") {
                    break;
                }
            }
        }
        
        // Regular character
        currentValue += c;
        pos++;
    }
    
    // Add last value if present
    if (!currentValue.empty() || pos > 0) {
        values.push_back(currentValue);
    }
    
    return values;
}

// Process source code and extract DATA
DataPreprocessorResult DataPreprocessor::process(const std::string& source) {
    DataPreprocessorResult result;
    
    std::istringstream sourceStream(source);
    std::ostringstream cleanedStream;
    std::string line;
    std::string pendingLabel;
    
    while (std::getline(sourceStream, line)) {
        // Check if this is a DATA line
        if (isDataLine(line)) {
            // Parse the line
            size_t pos = 0;
            
            // Extract line number
            int lineNumber = extractLineNumber(line, pos);
            
            // Extract label
            std::string label = extractLabel(line, pos);
            
            // Use pending label if no label on this line
            if (label.empty() && !pendingLabel.empty()) {
                label = pendingLabel;
            }
            
            // Get current data index (where this DATA starts)
            size_t currentIndex = result.values.size();
            
            // Record restore points
            if (lineNumber > 0) {
                result.lineRestorePoints[lineNumber] = currentIndex;
            }
            
            if (!label.empty()) {
                result.labelRestorePoints[label] = currentIndex;
                // Also record label definition for symbol table
                result.labelDefinitions[label] = lineNumber > 0 ? lineNumber : 0;
            }
            
            // Extract and parse DATA values
            std::vector<std::string> rawValues = extractDataValues(line, pos);
            for (const auto& raw : rawValues) {
                result.values.push_back(parseValue(raw));
            }
            
            // Clear pending label after using it
            pendingLabel.clear();
            
            // Completely remove DATA line from cleaned source
            // Labels don't need to be in the AST - they're stored in DataManager
            continue;
        }
        
        // Check if this line has only a label (could precede DATA on next line)
        size_t pos = 0;
        int lineNumber = extractLineNumber(line, pos);
        std::string label = extractLabel(line, pos);
        
        // Skip whitespace after label
        while (pos < line.length() && isWhitespace(line[pos])) {
            pos++;
        }
        
        // If line has only label (and maybe line number), save it for next DATA
        if (!label.empty() && pos >= line.length()) {
            pendingLabel = label;
            // Don't include label-only lines that precede DATA
            // They'll be removed when DATA is found
        } else {
            // Regular line (not DATA) - include in cleaned source
            cleanedStream << line << "\n";
            // Clear pending label if this isn't DATA
            pendingLabel.clear();
        }
    }
    
    result.cleanedSource = cleanedStream.str();
    return result;
}

// Helper: Check if line is a REM statement
bool DataPreprocessor::isREMLine(const std::string& line, size_t pos) {
    // Check for REM keyword (case insensitive)
    if (pos + 3 > line.length()) {
        return false;
    }
    
    std::string keyword;
    for (size_t i = 0; i < 3 && pos + i < line.length(); i++) {
        keyword += std::toupper(line[pos + i]);
    }
    
    if (keyword != "REM") {
        return false;
    }
    
    // Make sure REM is not part of a larger word
    size_t afterREM = pos + 3;
    if (afterREM < line.length() && 
        (std::isalnum(line[afterREM]) || line[afterREM] == '_')) {
        return false;
    }
    
    return true;
}

// Preprocess REM statements - strips comment text but keeps line number
// Converts "1820 REM This is a comment" to "1820 REM"
std::string DataPreprocessor::preprocessREM(const std::string& source) {
    std::istringstream sourceStream(source);
    std::ostringstream outputStream;
    std::string line;
    
    while (std::getline(sourceStream, line)) {
        size_t pos = 0;
        
        // Skip leading whitespace
        while (pos < line.length() && isWhitespace(line[pos])) {
            pos++;
        }
        
        size_t lineStart = pos;
        
        // Check for line number
        bool hasLineNumber = false;
        size_t lineNumberEnd = pos;
        while (pos < line.length() && std::isdigit(line[pos])) {
            hasLineNumber = true;
            pos++;
            lineNumberEnd = pos;
        }
        
        // Skip whitespace after line number
        while (pos < line.length() && isWhitespace(line[pos])) {
            pos++;
        }
        
        // Skip optional label
        if (pos < line.length() && line[pos] == ':') {
            pos++;
            while (pos < line.length() && 
                   (std::isalnum(line[pos]) || line[pos] == '_')) {
                pos++;
            }
            while (pos < line.length() && isWhitespace(line[pos])) {
                pos++;
            }
        }
        
        // Check if this is a REM statement
        if (isREMLine(line, pos)) {
            // Output line number (if present) + REM only
            if (hasLineNumber) {
                outputStream << line.substr(lineStart, lineNumberEnd - lineStart);
                outputStream << " REM\n";
            } else {
                outputStream << "REM\n";
            }
        } else {
            // Not a REM line - check for inline REM (after a colon)
            bool foundInlineREM = false;
            size_t colonPos = pos;
            
            while (colonPos < line.length()) {
                if (line[colonPos] == ':') {
                    // Check if REM follows the colon
                    size_t afterColon = colonPos + 1;
                    while (afterColon < line.length() && isWhitespace(line[afterColon])) {
                        afterColon++;
                    }
                    
                    if (isREMLine(line, afterColon)) {
                        // Found inline REM - output everything up to and including the colon + REM
                        outputStream << line.substr(0, colonPos + 1) << " REM\n";
                        foundInlineREM = true;
                        break;
                    }
                }
                colonPos++;
            }
            
            if (!foundInlineREM) {
                // No inline REM found - output the entire line
                outputStream << line << "\n";
            }
        }
    }
    
    return outputStream.str();
}

// Helper: Extract line number from start of a BASIC line
int DataPreprocessor::extractLineNumber(const std::string& line) {
    size_t pos = 0;
    
    // Skip leading whitespace
    while (pos < line.length() && isWhitespace(line[pos])) {
        pos++;
    }
    
    // Check if we have digits
    if (pos >= line.length() || !std::isdigit(line[pos])) {
        return -1;
    }
    
    // Parse line number
    int lineNum = 0;
    while (pos < line.length() && std::isdigit(line[pos])) {
        lineNum = lineNum * 10 + (line[pos] - '0');
        pos++;
    }
    
    return lineNum;
}

// Pass 1: Collect all GOTO/GOSUB/ON GOTO/RESTORE target line numbers
std::set<int> DataPreprocessor::collectGotoTargets(const std::string& source) {
    std::set<int> targets;
    std::istringstream sourceStream(source);
    std::string line;
    
    while (std::getline(sourceStream, line)) {
        // Convert to uppercase for case-insensitive matching
        std::string upperLine = line;
        for (char& c : upperLine) {
            c = std::toupper(c);
        }
        
        // Skip REM lines (comments)
        if (upperLine.find("REM") != std::string::npos) {
            size_t remPos = upperLine.find("REM");
            // Make sure REM is a keyword, not part of another word
            bool isKeyword = (remPos == 0 || !std::isalnum(upperLine[remPos - 1]));
            if (isKeyword && (remPos + 3 >= upperLine.length() || !std::isalnum(upperLine[remPos + 3]))) {
                continue;
            }
        }
        
        // Look for GOTO, GOSUB, ON GOTO, ON GOSUB, RESTORE, IF...THEN line_number
        
        // GOTO line_number
        size_t pos = upperLine.find("GOTO");
        if (pos != std::string::npos) {
            // Make sure GOTO is a keyword
            bool isKeyword = (pos == 0 || !std::isalnum(upperLine[pos - 1]));
            if (isKeyword && (pos + 4 >= upperLine.length() || !std::isalnum(upperLine[pos + 4]))) {
                // Extract the line number after GOTO
                pos += 4;
                while (pos < upperLine.length() && isWhitespace(upperLine[pos])) pos++;
                
                // Check for ON GOTO (comma-separated list)
                size_t onPos = upperLine.rfind("ON", pos);
                if (onPos != std::string::npos && onPos < pos - 10) {
                    // ON GOTO - parse comma-separated list
                    std::string numStr;
                    while (pos < upperLine.length()) {
                        if (std::isdigit(upperLine[pos])) {
                            numStr += upperLine[pos];
                        } else if (upperLine[pos] == ',') {
                            if (!numStr.empty()) {
                                targets.insert(std::stoi(numStr));
                                numStr.clear();
                            }
                        } else if (!isWhitespace(upperLine[pos])) {
                            break;
                        }
                        pos++;
                    }
                    if (!numStr.empty()) {
                        targets.insert(std::stoi(numStr));
                    }
                } else {
                    // Regular GOTO - single line number
                    std::string numStr;
                    while (pos < upperLine.length() && std::isdigit(upperLine[pos])) {
                        numStr += upperLine[pos];
                        pos++;
                    }
                    if (!numStr.empty()) {
                        targets.insert(std::stoi(numStr));
                    }
                }
            }
        }
        
        // GOSUB line_number
        pos = upperLine.find("GOSUB");
        if (pos != std::string::npos) {
            bool isKeyword = (pos == 0 || !std::isalnum(upperLine[pos - 1]));
            if (isKeyword && (pos + 5 >= upperLine.length() || !std::isalnum(upperLine[pos + 5]))) {
                pos += 5;
                while (pos < upperLine.length() && isWhitespace(upperLine[pos])) pos++;
                
                // Check for ON GOSUB
                size_t onPos = upperLine.rfind("ON", pos);
                if (onPos != std::string::npos && onPos < pos - 10) {
                    // ON GOSUB - parse comma-separated list
                    std::string numStr;
                    while (pos < upperLine.length()) {
                        if (std::isdigit(upperLine[pos])) {
                            numStr += upperLine[pos];
                        } else if (upperLine[pos] == ',') {
                            if (!numStr.empty()) {
                                targets.insert(std::stoi(numStr));
                                numStr.clear();
                            }
                        } else if (!isWhitespace(upperLine[pos])) {
                            break;
                        }
                        pos++;
                    }
                    if (!numStr.empty()) {
                        targets.insert(std::stoi(numStr));
                    }
                } else {
                    // Regular GOSUB
                    std::string numStr;
                    while (pos < upperLine.length() && std::isdigit(upperLine[pos])) {
                        numStr += upperLine[pos];
                        pos++;
                    }
                    if (!numStr.empty()) {
                        targets.insert(std::stoi(numStr));
                    }
                }
            }
        }
        
        // RESTORE line_number
        pos = upperLine.find("RESTORE");
        if (pos != std::string::npos) {
            bool isKeyword = (pos == 0 || !std::isalnum(upperLine[pos - 1]));
            if (isKeyword && (pos + 7 >= upperLine.length() || !std::isalnum(upperLine[pos + 7]))) {
                pos += 7;
                while (pos < upperLine.length() && isWhitespace(upperLine[pos])) pos++;
                std::string numStr;
                while (pos < upperLine.length() && std::isdigit(upperLine[pos])) {
                    numStr += upperLine[pos];
                    pos++;
                }
                if (!numStr.empty()) {
                    targets.insert(std::stoi(numStr));
                }
            }
        }
        
        // IF...THEN line_number (simple case)
        pos = upperLine.find("THEN");
        if (pos != std::string::npos) {
            pos += 4;
            while (pos < upperLine.length() && isWhitespace(upperLine[pos])) pos++;
            // Check if followed by a line number (not GOTO or a statement)
            if (pos < upperLine.length() && std::isdigit(upperLine[pos])) {
                std::string numStr;
                while (pos < upperLine.length() && std::isdigit(upperLine[pos])) {
                    numStr += upperLine[pos];
                    pos++;
                }
                if (!numStr.empty()) {
                    targets.insert(std::stoi(numStr));
                }
            }
        }
    }
    
    return targets;
}

// Pass 2: Convert target line numbers to labels and rewrite GOTO references
std::string DataPreprocessor::convertLineNumbersToLabels(const std::string& source,
                                                         const std::set<int>& targets) {
    std::istringstream sourceStream(source);
    std::ostringstream outputStream;
    std::string line;
    
    while (std::getline(sourceStream, line)) {
        // Extract line number if present
        int lineNum = extractLineNumber(line);
        
        if (lineNum > 0) {
            // Find where the line number ends
            size_t pos = 0;
            while (pos < line.length() && isWhitespace(line[pos])) pos++;
            while (pos < line.length() && std::isdigit(line[pos])) pos++;
            
            // Check if this line is a GOTO/GOSUB target
            if (targets.find(lineNum) != targets.end()) {
                // Add label before the line (label: syntax)
                outputStream << "L" << lineNum << ": ";
            }
            
            // Skip whitespace after line number
            while (pos < line.length() && isWhitespace(line[pos])) pos++;
            
            // Output the rest of the line (without line number)
            outputStream << line.substr(pos);
        } else {
            // No line number - output as-is
            outputStream << line;
        }
        
        outputStream << "\n";
    }
    
    // Now convert GOTO/GOSUB/RESTORE/THEN references to label names
    // Use WHITELIST approach: only replace numbers in specific control flow contexts
    std::string result = outputStream.str();
    
    // Process line by line to identify control flow keywords
    std::istringstream resultStream(result);
    std::ostringstream finalOutput;
    std::string resultLine;
    
    while (std::getline(resultStream, resultLine)) {
        // Convert line to uppercase for keyword detection
        std::string upperLine = resultLine;
        for (char& c : upperLine) {
            c = std::toupper(c);
        }
        
        // Track if we made any replacements on this line
        bool modified = false;
        std::string modifiedLine = resultLine;
        
        // WHITELIST: Only replace numbers after these keywords:
        // 1. GOTO number
        // 2. GOSUB number
        // 3. RESTORE number
        // 4. THEN number (but not THEN statement)
        // 5. ON ... GOTO/GOSUB number,number,...
        
        // Find GOTO keyword
        size_t gotoPos = upperLine.find("GOTO");
        if (gotoPos != std::string::npos) {
            bool isKeyword = (gotoPos == 0 || !std::isalnum(upperLine[gotoPos - 1]));
            if (isKeyword && (gotoPos + 4 >= upperLine.length() || !std::isalnum(upperLine[gotoPos + 4]))) {
                // Found GOTO - replace numbers after it
                modifiedLine = replaceNumbersAfterKeyword(modifiedLine, gotoPos + 4, targets);
                modified = true;
            }
        }
        
        // Find GOSUB keyword
        size_t gosubPos = upperLine.find("GOSUB");
        if (gosubPos != std::string::npos) {
            bool isKeyword = (gosubPos == 0 || !std::isalnum(upperLine[gosubPos - 1]));
            if (isKeyword && (gosubPos + 5 >= upperLine.length() || !std::isalnum(upperLine[gosubPos + 5]))) {
                // Found GOSUB - replace numbers after it
                modifiedLine = replaceNumbersAfterKeyword(modifiedLine, gosubPos + 5, targets);
                modified = true;
            }
        }
        
        // Find RESTORE keyword
        size_t restorePos = upperLine.find("RESTORE");
        if (restorePos != std::string::npos) {
            bool isKeyword = (restorePos == 0 || !std::isalnum(upperLine[restorePos - 1]));
            if (isKeyword && (restorePos + 7 >= upperLine.length() || !std::isalnum(upperLine[restorePos + 7]))) {
                // Found RESTORE - replace numbers after it
                modifiedLine = replaceNumbersAfterKeyword(modifiedLine, restorePos + 7, targets);
                modified = true;
            }
        }
        
        // Find THEN keyword (for IF...THEN number)
        size_t thenPos = upperLine.find("THEN");
        if (thenPos != std::string::npos) {
            bool isKeyword = (thenPos == 0 || !std::isalnum(upperLine[thenPos - 1]));
            if (isKeyword && (thenPos + 4 >= upperLine.length() || !std::isalnum(upperLine[thenPos + 4]))) {
                // Found THEN - replace first number after it (if it's a line number, not a statement)
                modifiedLine = replaceNumbersAfterKeyword(modifiedLine, thenPos + 4, targets, true);
                modified = true;
            }
        }
        
        finalOutput << modifiedLine << "\n";
    }
    
    result = finalOutput.str();
    
    return result;
}

// Helper function to replace target numbers after a keyword position
std::string DataPreprocessor::replaceNumbersAfterKeyword(const std::string& line, 
                                                         size_t startPos, 
                                                         const std::set<int>& targets,
                                                         bool onlyFirst) {
    std::string result = line;
    size_t pos = startPos;
    
    // Skip whitespace after keyword
    while (pos < result.length() && isWhitespace(result[pos])) {
        pos++;
    }
    
    // Now look for numbers (possibly comma-separated for ON GOTO/GOSUB)
    bool foundAny = false;
    while (pos < result.length()) {
        // Skip whitespace
        while (pos < result.length() && isWhitespace(result[pos])) {
            pos++;
        }
        
        // Check if we have a digit
        if (pos >= result.length() || !std::isdigit(result[pos])) {
            break;
        }
        
        // Extract the number
        size_t numStart = pos;
        std::string numStr;
        while (pos < result.length() && std::isdigit(result[pos])) {
            numStr += result[pos];
            pos++;
        }
        
        // Check if this number is in our targets list
        if (!numStr.empty()) {
            int num = std::stoi(numStr);
            if (targets.find(num) != targets.end()) {
                // Replace it with label
                std::string labelStr = "L" + numStr;
                result.replace(numStart, numStr.length(), labelStr);
                // Adjust pos for the length change
                pos = numStart + labelStr.length();
                foundAny = true;
            }
        }
        
        // If onlyFirst is true (for THEN), stop after first number
        if (onlyFirst && foundAny) {
            break;
        }
        
        // Skip whitespace
        while (pos < result.length() && isWhitespace(result[pos])) {
            pos++;
        }
        
        // Check for comma (for ON GOTO/GOSUB lists)
        if (pos < result.length() && result[pos] == ',') {
            pos++; // Skip comma and continue
        } else {
            // No comma, we're done with this list
            break;
        }
    }
    
    return result;
}

// Preprocess line numbers to labels - two-pass process
std::string DataPreprocessor::preprocessLineNumbersToLabels(const std::string& source) {
    // Pass 1: Collect all GOTO/GOSUB targets
    std::set<int> targets = collectGotoTargets(source);
    
    // Pass 2: Convert targets to labels and rewrite GOTO/GOSUB statements
    return convertLineNumbersToLabels(source, targets);
}

} // namespace FasterBASIC