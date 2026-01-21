//
// DataManager.cpp
// FBRunner3 - DATA/READ/RESTORE Manager
//
// Manages BASIC DATA statements in C++ with typed values (int, float, string).
// Supports RESTORE by line number or label name.
// DATA is parsed early and cleared between script runs.
//

#include "DataManager.h"
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace FasterBASIC {

DataManager::DataManager()
    : m_readPointer(0)
{
}

DataManager::~DataManager() {
}

// Parse a string value into the appropriate type (int, double, or string)
//
// Parsing rules:
// 1. Whitespace is trimmed from both ends
// 2. Quoted strings (single or double quotes) are returned as strings with quotes removed
// 3. Numeric values are parsed in this order:
//    a) Integer: whole numbers without decimal point or exponent (e.g., "42", "-123", "  0  ")
//    b) Double: numbers with decimal point or scientific notation (e.g., "3.14", "1.5e10", "-2.3E-5")
// 4. If parsing as number fails, the value is returned as a string (trimmed)
//
// Examples:
//   "42"         → int(42)
//   " -123 "     → int(-123)
//   "3.14"       → double(3.14)
//   "1.5e10"     → double(1.5e10)
//   "\"hello\""  → string("hello")
//   "'world'"    → string("world")
//   "HELLO"      → string("HELLO")
//   "  text  "   → string("text")
//
DataValue DataManager::parseValue(const std::string& raw) {
    // Empty string stays as string
    if (raw.empty()) {
        return raw;
    }
    
    // Trim leading/trailing whitespace
    size_t start = 0;
    size_t end = raw.length();
    
    while (start < end && std::isspace(static_cast<unsigned char>(raw[start]))) {
        start++;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1]))) {
        end--;
    }
    
    if (start >= end) {
        return std::string(""); // All whitespace becomes empty string
    }
    
    std::string trimmed = raw.substr(start, end - start);
    
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
    // Use a temporary to avoid modifying the original
    const char* str = trimmed.c_str();
    char* endptr = nullptr;
    
    // strtol handles leading +/- and whitespace
    long intValue = std::strtol(str, &endptr, 10);
    
    // If entire string was consumed and no overflow, it's an int
    // Check that we consumed the whole string (endptr points to null terminator)
    if (endptr != str && *endptr == '\0') {
        // Additional check: make sure it doesn't have a decimal point or exponent
        // (strtol stops at '.', 'e', 'E')
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
    // strtod handles formats like: 1.23, -4.56, 1.5e10, -3.2E-5, etc.
    if (endptr != str && *endptr == '\0') {
        return doubleValue;
    }
    
    // Otherwise, it's a string (return trimmed version)
    return trimmed;
}

void DataManager::initialize(const std::vector<std::string>& rawValues) {
    m_data.clear();
    m_lineRestorePoints.clear();
    m_labelRestorePoints.clear();
    m_readPointer = 0;
    
    // Parse all values into typed variants
    m_data.reserve(rawValues.size());
    for (const auto& raw : rawValues) {
        m_data.push_back(parseValue(raw));
    }
}

void DataManager::addRestorePoint(int lineNumber, size_t index) {
    m_lineRestorePoints[lineNumber] = index;
}

void DataManager::addRestorePointByLabel(const std::string& labelName, size_t index) {
    m_labelRestorePoints[labelName] = index;
}

void DataManager::clear() {
    m_data.clear();
    m_lineRestorePoints.clear();
    m_labelRestorePoints.clear();
    m_readPointer = 0;
}

void DataManager::checkDataAvailable() const {
    if (m_readPointer >= m_data.size()) {
        throw OutOfDataError();
    }
}

// Convert DataValue to int
int DataManager::toInt(const DataValue& value) {
    if (std::holds_alternative<int>(value)) {
        return std::get<int>(value);
    } else if (std::holds_alternative<double>(value)) {
        return static_cast<int>(std::get<double>(value));
    } else {
        // String - try to parse
        const std::string& str = std::get<std::string>(value);
        char* endptr = nullptr;
        long result = std::strtol(str.c_str(), &endptr, 10);
        if (endptr == str.c_str() || *endptr != '\0') {
            return 0; // Parse failed
        }
        return static_cast<int>(result);
    }
}

// Convert DataValue to double
double DataManager::toDouble(const DataValue& value) {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    } else if (std::holds_alternative<int>(value)) {
        return static_cast<double>(std::get<int>(value));
    } else {
        // String - try to parse
        const std::string& str = std::get<std::string>(value);
        char* endptr = nullptr;
        double result = std::strtod(str.c_str(), &endptr);
        if (endptr == str.c_str() || *endptr != '\0') {
            return 0.0; // Parse failed
        }
        return result;
    }
}

// Convert DataValue to string
std::string DataManager::toString(const DataValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<int>(value)) {
        return std::to_string(std::get<int>(value));
    } else {
        return std::to_string(std::get<double>(value));
    }
}

int DataManager::readInt() {
    checkDataAvailable();
    const DataValue& value = m_data[m_readPointer++];
    return toInt(value);
}

double DataManager::readDouble() {
    checkDataAvailable();
    const DataValue& value = m_data[m_readPointer++];
    return toDouble(value);
}

std::string DataManager::readString() {
    checkDataAvailable();
    const DataValue& value = m_data[m_readPointer++];
    return toString(value);
}

DataValue DataManager::readValue() {
    checkDataAvailable();
    return m_data[m_readPointer++];
}

void DataManager::restore() {
    m_readPointer = 0;
}

void DataManager::restoreToLine(int lineNumber) {
    auto it = m_lineRestorePoints.find(lineNumber);
    if (it != m_lineRestorePoints.end()) {
        m_readPointer = it->second;
    } else {
        // Line not found - restore to beginning as fallback
        std::cerr << "[DataManager] Warning: RESTORE " << lineNumber 
                  << " - line not found, restoring to beginning\n";
        m_readPointer = 0;
    }
}

void DataManager::restoreToLabel(const std::string& labelName) {
    auto it = m_labelRestorePoints.find(labelName);
    if (it != m_labelRestorePoints.end()) {
        m_readPointer = it->second;
    } else {
        // Label not found - restore to beginning as fallback
        std::cerr << "[DataManager] Warning: RESTORE " << labelName 
                  << " - label not found, restoring to beginning\n";
        m_readPointer = 0;
    }
}

void DataManager::restoreToIndex(size_t index) {
    if (index < m_data.size()) {
        m_readPointer = index;
    } else {
        m_readPointer = 0;
    }
}

bool DataManager::hasMoreData() const {
    return m_readPointer < m_data.size();
}

size_t DataManager::getCurrentIndex() const {
    return m_readPointer;
}

size_t DataManager::getDataCount() const {
    return m_data.size();
}

bool DataManager::isEmpty() const {
    return m_data.empty();
}

std::string DataManager::getValueAsString(size_t index) const {
    if (index >= m_data.size()) {
        return "<out of bounds>";
    }
    return toString(m_data[index]);
}

void DataManager::dumpState() const {
    std::cout << "\n[DataManager State]\n";
    std::cout << "  Total values: " << m_data.size() << "\n";
    std::cout << "  Read pointer: " << m_readPointer << "\n";
    std::cout << "  Line restore points: " << m_lineRestorePoints.size() << "\n";
    for (const auto& [line, index] : m_lineRestorePoints) {
        std::cout << "    Line " << line << " → index " << index << "\n";
    }
    std::cout << "  Label restore points: " << m_labelRestorePoints.size() << "\n";
    for (const auto& [label, index] : m_labelRestorePoints) {
        std::cout << "    Label '" << label << "' → index " << index << "\n";
    }
    std::cout << "  Data values:\n";
    for (size_t i = 0; i < std::min(m_data.size(), size_t(10)); i++) {
        std::cout << "    [" << i << "] = " << getValueAsString(i);
        if (std::holds_alternative<int>(m_data[i])) {
            std::cout << " (int)";
        } else if (std::holds_alternative<double>(m_data[i])) {
            std::cout << " (double)";
        } else {
            std::cout << " (string)";
        }
        std::cout << "\n";
    }
    if (m_data.size() > 10) {
        std::cout << "    ... (" << (m_data.size() - 10) << " more)\n";
    }
    std::cout << "\n";
}

} // namespace FasterBASIC