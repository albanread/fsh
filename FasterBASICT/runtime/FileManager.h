//
// FileManager.h
// FBRunner3 - File I/O Manager
//
// Manages BASIC file I/O operations (OPEN, CLOSE, INPUT#, PRINT#, etc.)
// Supports sequential file access with file numbers (#1, #2, etc.)
//

#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <string>
#include <map>
#include <fstream>
#include <variant>
#include <stdexcept>
#include <memory>
#include <vector>

namespace FasterBASIC {

// File access modes
enum class FileMode {
    INPUT,      // Read-only sequential
    OUTPUT,     // Write-only sequential (create/truncate)
    APPEND,     // Write-only sequential (append)
    RANDOM      // Random access (read/write) - future
};

// Variant type for file I/O values (matches DataManager)
using FileValue = std::variant<int, double, std::string>;

// File handle information
struct FileHandle {
    std::unique_ptr<std::fstream> stream;
    FileMode mode;
    std::string filename;
    int recordLength;  // For RANDOM mode (future)
    bool isOpen;
    
    FileHandle() : mode(FileMode::INPUT), recordLength(0), isOpen(false) {}
};

class FileManager {
public:
    FileManager();
    ~FileManager();

    // Open/Close operations
    void open(int fileNumber, const std::string& filename, FileMode mode, int recordLength = 128);
    void close(int fileNumber);
    void closeAll();
    
    // BBC BASIC style file opening (returns file handle)
    int openIn(const std::string& filename);        // OPENIN - open for input only
    int openOut(const std::string& filename);       // OPENOUT - open for output only  
    int openUp(const std::string& filename);        // OPENUP - open for read/write

    // Sequential input operations
    FileValue readValue(int fileNumber);           // Read next value (whitespace/comma separated)
    std::vector<FileValue> readValues(int fileNumber, int count); // Read multiple values
    std::string readLine(int fileNumber);          // Read entire line (LINE INPUT)
    std::string readChars(int fileNumber, int count); // Read fixed number of characters (INPUT$)
    
    // BBC BASIC binary file I/O
    int readByte(int fileNumber);                  // BGET# - read single byte
    void writeByte(int fileNumber, int byte);      // BPUT# - write single byte
    
    // BBC BASIC advanced file reading
    std::string readUntilChar(int fileNumber, char terminator); // GET$#n TO char
    std::string readLineFromFile(int fileNumber);  // GET$#n (until CR/LF/NUL)
    
    // Sequential output operations
    void writeValue(int fileNumber, const FileValue& value, bool addSeparator = false);
    void writeFormatted(int fileNumber, const FileValue& value, const std::string& separator);
    void writeLine(int fileNumber, const std::string& line);
    void writeNewline(int fileNumber);
    
    // WRITE# style output (comma-separated, strings quoted)
    void writeQuoted(int fileNumber, const FileValue& value, bool isLast = true);

    // File status queries
    bool isEOF(int fileNumber) const;
    bool isOpen(int fileNumber) const;
    long getPosition(int fileNumber) const;        // LOC(n)
    long getLength(int fileNumber) const;          // LOF(n)
    
    // BBC BASIC file status and positioning
    bool isAtEOF(int fileNumber) const;            // EOF#(n)
    long getFileExtent(int fileNumber) const;      // EXT#(n) - file length
    long getFilePointer(int fileNumber) const;     // PTR#(n) - current position
    void setFilePointer(int fileNumber, long position); // PTR#n = pos - seek to position
    
    // Utility
    void clear();  // Close all files and reset state
    std::string getOpenFilesInfo() const;  // Debug info

private:
    std::map<int, FileHandle> m_files;
    int m_nextFileHandle;                          // For BBC BASIC auto-allocated file handles
    static constexpr int MAX_FILE_NUMBER = 255;
    static constexpr int MIN_FILE_NUMBER = 1;
    static constexpr int MIN_AUTO_HANDLE = 1;      // BBC BASIC auto handles start at 1
    static constexpr int MAX_AUTO_HANDLE = 255;

    // Validation helpers
    void validateFileNumber(int fileNumber) const;
    void checkFileOpen(int fileNumber) const;
    void checkFileMode(int fileNumber, FileMode expectedMode) const;
    FileHandle& getFile(int fileNumber);
    const FileHandle& getFile(int fileNumber) const;
    
    // BBC BASIC helpers
    int allocateFileHandle();                      // Find next available file handle
    void validateByteValue(int byte) const;        // Validate byte is 0-255
    
    // I/O helpers
    std::string readToken(std::fstream& stream);  // Read whitespace/comma delimited token
    FileValue parseValue(const std::string& token);
    
    // Type conversion helpers
    static int toInt(const FileValue& value);
    static double toDouble(const FileValue& value);
    static std::string toString(const FileValue& value);
    static std::string toQuotedString(const FileValue& value);
};

// Exception classes
class FileError : public std::runtime_error {
public:
    explicit FileError(const std::string& msg) : std::runtime_error(msg) {}
};

class FileNotOpenError : public FileError {
public:
    explicit FileNotOpenError(int fileNum) 
        : FileError("File #" + std::to_string(fileNum) + " not open") {}
};

class FileAlreadyOpenError : public FileError {
public:
    explicit FileAlreadyOpenError(int fileNum) 
        : FileError("File #" + std::to_string(fileNum) + " already open") {}
};

class InvalidFileNumberError : public FileError {
public:
    explicit InvalidFileNumberError(int fileNum) 
        : FileError("Invalid file number: " + std::to_string(fileNum)) {}
};

class FileIOError : public FileError {
public:
    explicit FileIOError(const std::string& operation, const std::string& filename) 
        : FileError("I/O error during " + operation + " on file: " + filename) {}
};

class BadFileModeError : public FileError {
public:
    explicit BadFileModeError(const std::string& operation) 
        : FileError("Bad file mode for operation: " + operation) {}
};

} // namespace FasterBASIC

#endif // FILEMANAGER_H