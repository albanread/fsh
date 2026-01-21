//
// FileManager.cpp
// FBRunner3 - File I/O Manager Implementation
//

#include "FileManager.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace FasterBASIC {

FileManager::FileManager() : m_nextFileHandle(MIN_AUTO_HANDLE) {
}

FileManager::~FileManager() {
    closeAll();
}

void FileManager::open(int fileNumber, const std::string& filename, FileMode mode, int recordLength) {
    validateFileNumber(fileNumber);
    
    // Check if file already open
    if (m_files.find(fileNumber) != m_files.end() && m_files[fileNumber].isOpen) {
        throw FileAlreadyOpenError(fileNumber);
    }
    
    // Create file handle
    FileHandle handle;
    handle.filename = filename;
    handle.mode = mode;
    handle.recordLength = recordLength;
    handle.stream = std::make_unique<std::fstream>();
    
    // Open with appropriate mode
    std::ios_base::openmode openMode = std::ios_base::binary;  // Binary mode for precise control
    
    switch (mode) {
        case FileMode::INPUT:
            openMode |= std::ios_base::in;
            break;
        case FileMode::OUTPUT:
            openMode |= std::ios_base::out | std::ios_base::trunc;
            break;
        case FileMode::APPEND:
            openMode |= std::ios_base::out | std::ios_base::app;
            break;
        case FileMode::RANDOM:
            openMode |= std::ios_base::in | std::ios_base::out;
            break;
    }
    
    handle.stream->open(filename, openMode);
    
    if (!handle.stream->is_open() || handle.stream->fail()) {
        throw FileIOError("open", filename);
    }
    
    handle.isOpen = true;
    m_files[fileNumber] = std::move(handle);
}

int FileManager::openIn(const std::string& filename) {
    int fileHandle = allocateFileHandle();
    open(fileHandle, filename, FileMode::INPUT);
    return fileHandle;
}

int FileManager::openOut(const std::string& filename) {
    int fileHandle = allocateFileHandle();
    open(fileHandle, filename, FileMode::OUTPUT);
    return fileHandle;
}

int FileManager::openUp(const std::string& filename) {
    int fileHandle = allocateFileHandle();
    open(fileHandle, filename, FileMode::RANDOM);
    return fileHandle;
}

void FileManager::close(int fileNumber) {
    validateFileNumber(fileNumber);
    
    auto it = m_files.find(fileNumber);
    if (it != m_files.end() && it->second.isOpen) {
        if (it->second.stream) {
            it->second.stream->close();
        }
        it->second.isOpen = false;
        m_files.erase(it);
    }
}

void FileManager::closeAll() {
    for (auto& pair : m_files) {
        if (pair.second.isOpen && pair.second.stream) {
            pair.second.stream->close();
        }
    }
    m_files.clear();
}

std::string FileManager::readLine(int fileNumber) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode != FileMode::INPUT) {
        throw BadFileModeError("LINE INPUT");
    }
    
    std::string line;
    if (std::getline(*handle.stream, line)) {
        return line;
    }
    
    // EOF or error
    if (handle.stream->eof()) {
        return "";
    }
    
    throw FileIOError("read line", handle.filename);
}

std::string FileManager::readChars(int fileNumber, int count) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode != FileMode::INPUT) {
        throw BadFileModeError("INPUT$");
    }
    
    if (count <= 0) {
        return "";
    }
    
    std::string result;
    result.reserve(count);
    
    char ch;
    for (int i = 0; i < count; ++i) {
        if (handle.stream->get(ch)) {
            result += ch;
        } else {
            // EOF reached before reading all requested characters
            break;
        }
    }
    
    return result;
}

int FileManager::readByte(int fileNumber) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode != FileMode::INPUT && handle.mode != FileMode::RANDOM) {
        throw BadFileModeError("BGET");
    }
    
    int ch = handle.stream->get();
    if (ch == EOF) {
        if (handle.stream->eof()) {
            return -1;  // BBC BASIC returns -1 at EOF
        }
        throw FileIOError("read byte", handle.filename);
    }
    
    return ch;
}

void FileManager::writeByte(int fileNumber, int byte) {
    validateByteValue(byte);
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        throw BadFileModeError("BPUT");
    }
    
    handle.stream->put(static_cast<char>(byte));
    
    if (handle.stream->fail()) {
        throw FileIOError("write byte", handle.filename);
    }
}

std::string FileManager::readUntilChar(int fileNumber, char terminator) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode != FileMode::INPUT && handle.mode != FileMode::RANDOM) {
        throw BadFileModeError("GET$# TO");
    }
    
    std::string result;
    char ch;
    while (handle.stream->get(ch)) {
        if (ch == terminator) {
            break;  // Don't include terminator in result
        }
        result += ch;
    }
    
    return result;
}

std::string FileManager::readLineFromFile(int fileNumber) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode != FileMode::INPUT && handle.mode != FileMode::RANDOM) {
        throw BadFileModeError("GET$#");
    }
    
    std::string result;
    char ch;
    while (handle.stream->get(ch)) {
        if (ch == '\r') {
            // Check for CR+LF
            if (handle.stream->peek() == '\n') {
                handle.stream->get();  // Consume LF
            }
            break;
        } else if (ch == '\n') {
            break;
        } else if (ch == '\0') {
            break;
        }
        result += ch;
    }
    
    return result;
}

std::string FileManager::readToken(std::fstream& stream) {
    std::string token;
    char ch;
    bool inQuotes = false;
    bool hasContent = false;
    
    // Skip leading whitespace and commas
    while (stream.get(ch)) {
        if (ch == '\n' || ch == '\r') {
            // Newline ends token
            if (hasContent) {
                stream.unget();
                break;
            }
            continue;
        }
        if (!inQuotes && (std::isspace(ch) || ch == ',')) {
            if (hasContent) {
                break;
            }
            continue;  // Skip leading separators
        }
        
        // Start of token
        if (ch == '"') {
            inQuotes = !inQuotes;
            hasContent = true;
            continue;
        }
        
        token += ch;
        hasContent = true;
        
        if (!inQuotes) {
            // Check if next char is separator
            if (stream.peek() == ',' || stream.peek() == '\n' || stream.peek() == '\r' || std::isspace(stream.peek())) {
                break;
            }
        }
    }
    
    return token;
}

FileValue FileManager::parseValue(const std::string& token) {
    if (token.empty()) {
        return std::string("");
    }
    
    std::string trimmed = token;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
    
    if (trimmed.empty()) {
        return std::string("");
    }
    
    // Try integer first
    char* endPtr = nullptr;
    long long intVal = std::strtoll(trimmed.c_str(), &endPtr, 10);
    if (endPtr && *endPtr == '\0') {
        // Valid integer
        return static_cast<int>(intVal);
    }
    
    // Try double
    endPtr = nullptr;
    double doubleVal = std::strtod(trimmed.c_str(), &endPtr);
    if (endPtr && *endPtr == '\0') {
        // Valid double
        return doubleVal;
    }
    
    // Default to string
    return trimmed;
}

FileValue FileManager::readValue(int fileNumber) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode != FileMode::INPUT) {
        throw BadFileModeError("INPUT");
    }
    
    if (handle.stream->eof()) {
        throw FileIOError("read value (EOF)", handle.filename);
    }
    
    std::string token = readToken(*handle.stream);
    return parseValue(token);
}

std::vector<FileValue> FileManager::readValues(int fileNumber, int count) {
    std::vector<FileValue> values;
    values.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        values.push_back(readValue(fileNumber));
    }
    
    return values;
}

void FileManager::writeValue(int fileNumber, const FileValue& value, bool addSeparator) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        throw BadFileModeError("PRINT/WRITE");
    }
    
    *handle.stream << toString(value);
    
    if (addSeparator) {
        *handle.stream << " ";
    }
    
    if (handle.stream->fail()) {
        throw FileIOError("write value", handle.filename);
    }
}

void FileManager::writeFormatted(int fileNumber, const FileValue& value, const std::string& separator) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        throw BadFileModeError("PRINT");
    }
    
    *handle.stream << toString(value);
    
    if (separator == ";") {
        // Semicolon - no separator
    } else if (separator == ",") {
        // Comma - add space or tab
        *handle.stream << " ";
    } else if (separator.empty() || separator == "\n") {
        // Newline
        *handle.stream << "\n";
    }
    
    if (handle.stream->fail()) {
        throw FileIOError("write formatted", handle.filename);
    }
}

void FileManager::writeLine(int fileNumber, const std::string& line) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        throw BadFileModeError("PRINT");
    }
    
    *handle.stream << line << "\n";
    
    if (handle.stream->fail()) {
        throw FileIOError("write line", handle.filename);
    }
}

void FileManager::writeNewline(int fileNumber) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        throw BadFileModeError("PRINT");
    }
    
    *handle.stream << "\n";
}

void FileManager::writeQuoted(int fileNumber, const FileValue& value, bool isLast) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        throw BadFileModeError("WRITE");
    }
    
    *handle.stream << toQuotedString(value);
    
    if (!isLast) {
        *handle.stream << ",";
    } else {
        *handle.stream << "\n";
    }
    
    if (handle.stream->fail()) {
        throw FileIOError("write quoted", handle.filename);
    }
}

bool FileManager::isEOF(int fileNumber) const {
    if (!isOpen(fileNumber)) {
        return true;
    }
    
    const FileHandle& handle = getFile(fileNumber);
    return handle.stream->eof() || handle.stream->peek() == EOF;
}

bool FileManager::isOpen(int fileNumber) const {
    auto it = m_files.find(fileNumber);
    return it != m_files.end() && it->second.isOpen;
}

long FileManager::getPosition(int fileNumber) const {
    checkFileOpen(fileNumber);
    const FileHandle& handle = getFile(fileNumber);
    
    if (handle.mode == FileMode::INPUT) {
        return static_cast<long>(handle.stream->tellg());
    } else {
        return static_cast<long>(handle.stream->tellp());
    }
}

long FileManager::getLength(int fileNumber) const {
    checkFileOpen(fileNumber);
    const FileHandle& handle = getFile(fileNumber);
    
    auto currentPos = handle.stream->tellg();
    handle.stream->seekg(0, std::ios::end);
    auto length = handle.stream->tellg();
    handle.stream->seekg(currentPos);  // Restore position
    
    return static_cast<long>(length);
}

bool FileManager::isAtEOF(int fileNumber) const {
    return isEOF(fileNumber);  // Delegate to existing implementation
}

long FileManager::getFileExtent(int fileNumber) const {
    return getLength(fileNumber);  // Delegate to existing implementation
}

long FileManager::getFilePointer(int fileNumber) const {
    return getPosition(fileNumber);  // Delegate to existing implementation
}

void FileManager::setFilePointer(int fileNumber, long position) {
    checkFileOpen(fileNumber);
    FileHandle& handle = getFile(fileNumber);
    
    if (position < 0) {
        throw FileIOError("set file pointer (negative position)", handle.filename);
    }
    
    if (handle.mode == FileMode::INPUT || handle.mode == FileMode::RANDOM) {
        handle.stream->seekg(position);
    }
    if (handle.mode == FileMode::OUTPUT || handle.mode == FileMode::APPEND || handle.mode == FileMode::RANDOM) {
        handle.stream->seekp(position);
    }
    
    if (handle.stream->fail()) {
        throw FileIOError("set file pointer", handle.filename);
    }
}

void FileManager::clear() {
    closeAll();
}

std::string FileManager::getOpenFilesInfo() const {
    std::ostringstream oss;
    oss << "Open files: " << m_files.size() << "\n";
    for (const auto& pair : m_files) {
        oss << "  #" << pair.first << ": " << pair.second.filename;
        if (pair.second.isOpen) {
            oss << " (open, ";
            switch (pair.second.mode) {
                case FileMode::INPUT: oss << "INPUT"; break;
                case FileMode::OUTPUT: oss << "OUTPUT"; break;
                case FileMode::APPEND: oss << "APPEND"; break;
                case FileMode::RANDOM: oss << "RANDOM"; break;
            }
            oss << ")\n";
        } else {
            oss << " (closed)\n";
        }
    }
    return oss.str();
}

void FileManager::validateFileNumber(int fileNumber) const {
    if (fileNumber < MIN_FILE_NUMBER || fileNumber > MAX_FILE_NUMBER) {
        throw InvalidFileNumberError(fileNumber);
    }
}

void FileManager::checkFileOpen(int fileNumber) const {
    auto it = m_files.find(fileNumber);
    if (it == m_files.end() || !it->second.isOpen) {
        throw FileNotOpenError(fileNumber);
    }
}

void FileManager::checkFileMode(int fileNumber, FileMode expectedMode) const {
    checkFileOpen(fileNumber);
    const FileHandle& handle = getFile(fileNumber);
    if (handle.mode != expectedMode) {
        throw BadFileModeError("mode mismatch");
    }
}

FileHandle& FileManager::getFile(int fileNumber) {
    auto it = m_files.find(fileNumber);
    if (it == m_files.end() || !it->second.isOpen) {
        throw FileNotOpenError(fileNumber);
    }
    return it->second;
}

int FileManager::allocateFileHandle() {
    // Find next available file handle starting from m_nextFileHandle
    for (int handle = m_nextFileHandle; handle <= MAX_AUTO_HANDLE; ++handle) {
        if (m_files.find(handle) == m_files.end()) {
            m_nextFileHandle = handle + 1;
            if (m_nextFileHandle > MAX_AUTO_HANDLE) {
                m_nextFileHandle = MIN_AUTO_HANDLE;  // Wrap around
            }
            return handle;
        }
    }
    
    // Wrap around and search from beginning
    for (int handle = MIN_AUTO_HANDLE; handle < m_nextFileHandle; ++handle) {
        if (m_files.find(handle) == m_files.end()) {
            m_nextFileHandle = handle + 1;
            return handle;
        }
    }
    
    throw FileError("No available file handles");
}

void FileManager::validateByteValue(int byte) const {
    if (byte < 0 || byte > 255) {
        throw FileError("Byte value must be 0-255, got: " + std::to_string(byte));
    }
}

const FileHandle& FileManager::getFile(int fileNumber) const {
    auto it = m_files.find(fileNumber);
    if (it == m_files.end() || !it->second.isOpen) {
        throw FileNotOpenError(fileNumber);
    }
    return it->second;
}

int FileManager::toInt(const FileValue& value) {
    if (std::holds_alternative<int>(value)) {
        return std::get<int>(value);
    } else if (std::holds_alternative<double>(value)) {
        return static_cast<int>(std::get<double>(value));
    } else {
        // Try to parse string
        std::string str = std::get<std::string>(value);
        try {
            return std::stoi(str);
        } catch (...) {
            return 0;
        }
    }
}

double FileManager::toDouble(const FileValue& value) {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    } else if (std::holds_alternative<int>(value)) {
        return static_cast<double>(std::get<int>(value));
    } else {
        // Try to parse string
        std::string str = std::get<std::string>(value);
        try {
            return std::stod(str);
        } catch (...) {
            return 0.0;
        }
    }
}

std::string FileManager::toString(const FileValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<int>(value)) {
        return std::to_string(std::get<int>(value));
    } else {
        double d = std::get<double>(value);
        std::ostringstream oss;
        oss << d;
        return oss.str();
    }
}

std::string FileManager::toQuotedString(const FileValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        std::string str = std::get<std::string>(value);
        // Escape quotes in string
        std::string escaped;
        for (char ch : str) {
            if (ch == '"') {
                escaped += "\"\"";  // BASIC-style quote escaping
            } else {
                escaped += ch;
            }
        }
        return "\"" + escaped + "\"";
    } else if (std::holds_alternative<int>(value)) {
        return std::to_string(std::get<int>(value));
    } else {
        double d = std::get<double>(value);
        std::ostringstream oss;
        oss << d;
        return oss.str();
    }
}

} // namespace FasterBASIC