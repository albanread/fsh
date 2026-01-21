//
// DataManager.h
// FBRunner3 - DATA/READ/RESTORE Manager
//
// Manages BASIC DATA statements in C++ with typed values (int, float, string).
// Supports RESTORE by line number or label name.
// DATA is parsed early and cleared between script runs.
//

#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>

namespace FasterBASIC {

// Variant type for DATA values (int, double, or string)
using DataValue = std::variant<int, double, std::string>;

class DataManager {
public:
    DataManager();
    ~DataManager();

    // Initialize with typed data values
    // Values are parsed from strings into int/double/string
    void initialize(const std::vector<std::string>& rawValues);

    // Add restore point by line number
    void addRestorePoint(int lineNumber, size_t index);

    // Add restore point by label name
    void addRestorePointByLabel(const std::string& labelName, size_t index);

    // Clear all data (call at end of script or before new script)
    void clear();

    // Read operations - return typed values
    int readInt();              // Read as integer (auto-convert if needed)
    double readDouble();        // Read as double (auto-convert if needed)
    std::string readString();   // Read as string (auto-convert if needed)
    DataValue readValue();      // Read raw variant value

    // RESTORE operations
    void restore();                         // RESTORE to beginning (index 0)
    void restoreToLine(int lineNumber);     // RESTORE to line number
    void restoreToLabel(const std::string& labelName); // RESTORE to label
    void restoreToIndex(size_t index);      // RESTORE to specific index (internal)

    // Query operations
    bool hasMoreData() const;       // Check if more data available
    size_t getCurrentIndex() const; // Get current read position
    size_t getDataCount() const;    // Get total number of data values
    bool isEmpty() const;           // Check if no data loaded

    // Debug/inspection
    std::string getValueAsString(size_t index) const;
    void dumpState() const;         // Print current state for debugging

private:
    std::vector<DataValue> m_data;
    std::map<int, size_t> m_lineRestorePoints;      // Line number → index
    std::map<std::string, size_t> m_labelRestorePoints; // Label name → index
    size_t m_readPointer;

    void checkDataAvailable() const;
    
    // Helper: Parse string to appropriate type (int, double, or string)
    static DataValue parseValue(const std::string& raw);
    
    // Helper: Convert DataValue to requested type
    static int toInt(const DataValue& value);
    static double toDouble(const DataValue& value);
    static std::string toString(const DataValue& value);
};

// Exception for OUT OF DATA errors
class OutOfDataError : public std::runtime_error {
public:
    OutOfDataError() : std::runtime_error("OUT OF DATA") {}
};

} // namespace FasterBASIC

#endif // DATAMANAGER_H