//
//  SourceDocument.cpp
//  FasterBASIC Framework - Unified Source Code Structure
//
//  Implementation of unified source code representation.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#include "SourceDocument.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <climits>

namespace FasterBASIC {

// =============================================================================
// DocumentLocation
// =============================================================================

std::string DocumentLocation::toString() const {
    std::ostringstream oss;
    if (!filename.empty()) {
        oss << filename << ":";
    }
    oss << (lineIndex + 1);  // Convert to 1-based for display
    if (lineNumber > 0) {
        oss << " (line " << lineNumber << ")";
    }
    oss << ":" << (column + 1);
    return oss.str();
}

// =============================================================================
// SourceDocument - Construction
// =============================================================================

SourceDocument::SourceDocument()
    : m_encoding("UTF-8")
    , m_version(0)
    , m_dirty(false)
    , m_autoNumbering(false)
    , m_autoStart(10)
    , m_autoStep(10)
    , m_nextAutoNumber(10)
    , m_maxUndoStates(100)
{
    // Start empty
}

SourceDocument::SourceDocument(const std::string& filename)
    : SourceDocument()
{
    m_filename = filename;
    loadFromFile(filename);
}

SourceDocument::~SourceDocument() = default;

SourceDocument::SourceDocument(const SourceDocument& other)
    : m_lines(other.m_lines)
    , m_lineNumberIndex(other.m_lineNumberIndex)
    , m_filename(other.m_filename)
    , m_encoding(other.m_encoding)
    , m_version(other.m_version)
    , m_dirty(other.m_dirty)
    , m_autoNumbering(other.m_autoNumbering)
    , m_autoStart(other.m_autoStart)
    , m_autoStep(other.m_autoStep)
    , m_nextAutoNumber(other.m_nextAutoNumber)
    , m_undoStack(other.m_undoStack)
    , m_redoStack(other.m_redoStack)
    , m_maxUndoStates(other.m_maxUndoStates)
{
}

SourceDocument::SourceDocument(SourceDocument&& other) noexcept
    : m_lines(std::move(other.m_lines))
    , m_lineNumberIndex(std::move(other.m_lineNumberIndex))
    , m_filename(std::move(other.m_filename))
    , m_encoding(std::move(other.m_encoding))
    , m_version(other.m_version)
    , m_dirty(other.m_dirty)
    , m_autoNumbering(other.m_autoNumbering)
    , m_autoStart(other.m_autoStart)
    , m_autoStep(other.m_autoStep)
    , m_nextAutoNumber(other.m_nextAutoNumber)
    , m_undoStack(std::move(other.m_undoStack))
    , m_redoStack(std::move(other.m_redoStack))
    , m_maxUndoStates(other.m_maxUndoStates)
{
}

SourceDocument& SourceDocument::operator=(const SourceDocument& other) {
    if (this != &other) {
        m_lines = other.m_lines;
        m_lineNumberIndex = other.m_lineNumberIndex;
        m_filename = other.m_filename;
        m_encoding = other.m_encoding;
        m_version = other.m_version;
        m_dirty = other.m_dirty;
        m_autoNumbering = other.m_autoNumbering;
        m_autoStart = other.m_autoStart;
        m_autoStep = other.m_autoStep;
        m_nextAutoNumber = other.m_nextAutoNumber;
        m_undoStack = other.m_undoStack;
        m_redoStack = other.m_redoStack;
        m_maxUndoStates = other.m_maxUndoStates;
    }
    return *this;
}

SourceDocument& SourceDocument::operator=(SourceDocument&& other) noexcept {
    if (this != &other) {
        m_lines = std::move(other.m_lines);
        m_lineNumberIndex = std::move(other.m_lineNumberIndex);
        m_filename = std::move(other.m_filename);
        m_encoding = std::move(other.m_encoding);
        m_version = other.m_version;
        m_dirty = other.m_dirty;
        m_autoNumbering = other.m_autoNumbering;
        m_autoStart = other.m_autoStart;
        m_autoStep = other.m_autoStep;
        m_nextAutoNumber = other.m_nextAutoNumber;
        m_undoStack = std::move(other.m_undoStack);
        m_redoStack = std::move(other.m_redoStack);
        m_maxUndoStates = other.m_maxUndoStates;
    }
    return *this;
}

// =============================================================================
// Line Access - By Index
// =============================================================================

SourceLine& SourceDocument::getLineByIndex(size_t index) {
    static SourceLine dummy;
    if (index >= m_lines.size()) {
        return dummy;
    }
    return m_lines[index];
}

const SourceLine& SourceDocument::getLineByIndex(size_t index) const {
    static SourceLine dummy;
    if (index >= m_lines.size()) {
        return dummy;
    }
    return m_lines[index];
}

// =============================================================================
// Line Access - By Line Number
// =============================================================================

SourceLine* SourceDocument::getLineByNumber(int lineNumber) {
    auto it = m_lineNumberIndex.find(lineNumber);
    if (it == m_lineNumberIndex.end()) {
        return nullptr;
    }
    size_t index = it->second;
    if (index >= m_lines.size()) {
        return nullptr;
    }
    return &m_lines[index];
}

const SourceLine* SourceDocument::getLineByNumber(int lineNumber) const {
    auto it = m_lineNumberIndex.find(lineNumber);
    if (it == m_lineNumberIndex.end()) {
        return nullptr;
    }
    size_t index = it->second;
    if (index >= m_lines.size()) {
        return nullptr;
    }
    return &m_lines[index];
}

bool SourceDocument::hasLineNumber(int lineNumber) const {
    return m_lineNumberIndex.find(lineNumber) != m_lineNumberIndex.end();
}

std::vector<int> SourceDocument::getLineNumbers() const {
    std::vector<int> numbers;
    numbers.reserve(m_lineNumberIndex.size());
    for (const auto& pair : m_lineNumberIndex) {
        numbers.push_back(pair.first);
    }
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}

size_t SourceDocument::getIndexForLineNumber(int lineNumber) const {
    auto it = m_lineNumberIndex.find(lineNumber);
    if (it == m_lineNumberIndex.end()) {
        return static_cast<size_t>(-1);
    }
    return it->second;
}

// =============================================================================
// Line Modification - REPL Style (by line number)
// =============================================================================

void SourceDocument::setLineByNumber(int lineNumber, const std::string& text) {
    if (lineNumber <= 0) {
        return;  // Invalid line number
    }
    
    // Check if line number already exists
    auto it = m_lineNumberIndex.find(lineNumber);
    if (it != m_lineNumberIndex.end()) {
        // Replace existing line
        size_t index = it->second;
        if (index < m_lines.size()) {
            m_lines[index].text = text;
            m_lines[index].isDirty = true;
            m_lines[index].version = m_version;
        }
    } else {
        // Insert new line at correct position
        size_t insertPos = findInsertionPoint(lineNumber);
        SourceLine newLine(lineNumber, text);
        newLine.version = m_version;
        m_lines.insert(m_lines.begin() + insertPos, newLine);
        
        // Rebuild index (indices shifted)
        rebuildLineNumberIndex();
    }
    
    markDirty();
}

bool SourceDocument::deleteLineByNumber(int lineNumber) {
    auto it = m_lineNumberIndex.find(lineNumber);
    if (it == m_lineNumberIndex.end()) {
        return false;
    }
    
    size_t index = it->second;
    if (index >= m_lines.size()) {
        return false;
    }
    
    m_lines.erase(m_lines.begin() + index);
    rebuildLineNumberIndex();
    
    ensureNonEmpty();
    markDirty();
    return true;
}

// =============================================================================
// Line Modification - Editor Style (by index)
// =============================================================================

void SourceDocument::insertLineAtIndex(size_t index, const std::string& text, int lineNumber) {
    SourceLine newLine(lineNumber, text);
    newLine.version = m_version;
    
    if (index >= m_lines.size()) {
        m_lines.push_back(newLine);
    } else {
        m_lines.insert(m_lines.begin() + index, newLine);
    }
    
    if (lineNumber > 0) {
        rebuildLineNumberIndex();
    }
    
    markDirty();
}

bool SourceDocument::deleteLineAtIndex(size_t index) {
    if (index >= m_lines.size()) {
        return false;
    }
    
    int lineNumber = m_lines[index].lineNumber;
    m_lines.erase(m_lines.begin() + index);
    
    if (lineNumber > 0) {
        rebuildLineNumberIndex();
    }
    
    ensureNonEmpty();
    markDirty();
    return true;
}

bool SourceDocument::replaceLineAtIndex(size_t index, const std::string& text) {
    if (index >= m_lines.size()) {
        return false;
    }
    
    m_lines[index].text = text;
    m_lines[index].isDirty = true;
    m_lines[index].version = m_version;
    
    markDirty();
    return true;
}

bool SourceDocument::splitLine(size_t index, size_t column) {
    if (index >= m_lines.size()) {
        return false;
    }
    
    std::string& line = m_lines[index].text;
    if (column > line.length()) {
        column = line.length();
    }
    
    // Split the line
    std::string firstPart = line.substr(0, column);
    std::string secondPart = line.substr(column);
    
    m_lines[index].text = firstPart;
    m_lines[index].isDirty = true;
    m_lines[index].version = m_version;
    
    // Insert new line (unnumbered)
    insertLineAtIndex(index + 1, secondPart, 0);
    
    return true;
}

bool SourceDocument::joinWithNext(size_t index) {
    if (index >= m_lines.size() - 1) {
        return false;
    }
    
    m_lines[index].text += m_lines[index + 1].text;
    m_lines[index].isDirty = true;
    m_lines[index].version = m_version;
    
    deleteLineAtIndex(index + 1);
    
    return true;
}

// =============================================================================
// Character-Level Operations
// =============================================================================

bool SourceDocument::insertChar(size_t lineIndex, size_t column, char32_t ch) {
    if (lineIndex >= m_lines.size()) {
        return false;
    }
    
    std::string& line = m_lines[lineIndex].text;
    
    // Convert UTF-32 to UTF-8
    std::string utf8Char = utf32ToUtf8(ch);
    
    if (column > line.length()) {
        column = line.length();
    }
    
    line.insert(column, utf8Char);
    m_lines[lineIndex].isDirty = true;
    m_lines[lineIndex].version = m_version;
    
    markDirty();
    return true;
}

bool SourceDocument::deleteChar(size_t lineIndex, size_t column) {
    if (lineIndex >= m_lines.size()) {
        return false;
    }
    
    std::string& line = m_lines[lineIndex].text;
    if (column >= line.length()) {
        return false;
    }
    
    // Simple byte deletion (could be enhanced for multi-byte UTF-8)
    line.erase(column, 1);
    m_lines[lineIndex].isDirty = true;
    m_lines[lineIndex].version = m_version;
    
    markDirty();
    return true;
}

bool SourceDocument::insertText(size_t lineIndex, size_t column, const std::string& text) {
    if (lineIndex >= m_lines.size()) {
        return false;
    }
    
    // Check if text contains newlines
    if (text.find('\n') != std::string::npos || text.find('\r') != std::string::npos) {
        // Multi-line insertion - split and insert
        auto lines = splitLines(text);
        if (lines.empty()) {
            return true;
        }
        
        std::string& currentLine = m_lines[lineIndex].text;
        if (column > currentLine.length()) {
            column = currentLine.length();
        }
        
        // Insert first part
        currentLine.insert(column, lines[0]);
        m_lines[lineIndex].isDirty = true;
        
        // If multiple lines, split and insert rest
        if (lines.size() > 1) {
            std::string remainder = currentLine.substr(column + lines[0].length());
            currentLine = currentLine.substr(0, column + lines[0].length());
            
            for (size_t i = 1; i < lines.size(); ++i) {
                std::string newLineText = lines[i];
                if (i == lines.size() - 1) {
                    newLineText += remainder;
                }
                insertLineAtIndex(lineIndex + i, newLineText, 0);
            }
        }
    } else {
        // Single line insertion
        std::string& line = m_lines[lineIndex].text;
        if (column > line.length()) {
            column = line.length();
        }
        line.insert(column, text);
        m_lines[lineIndex].isDirty = true;
        m_lines[lineIndex].version = m_version;
    }
    
    markDirty();
    return true;
}

char32_t SourceDocument::getChar(size_t lineIndex, size_t column) const {
    if (lineIndex >= m_lines.size()) {
        return 0;
    }
    
    const std::string& line = m_lines[lineIndex].text;
    if (column >= line.length()) {
        return 0;
    }
    
    // Simple byte access (could be enhanced for UTF-8)
    return static_cast<char32_t>(static_cast<unsigned char>(line[column]));
}

// =============================================================================
// Range Operations
// =============================================================================

std::string SourceDocument::getTextRange(size_t startLine, size_t startCol,
                                        size_t endLine, size_t endCol) const {
    if (startLine >= m_lines.size()) {
        return "";
    }
    
    if (endLine >= m_lines.size()) {
        endLine = m_lines.size() - 1;
    }
    
    if (startLine == endLine) {
        // Single line
        const std::string& line = m_lines[startLine].text;
        if (startCol > line.length()) startCol = line.length();
        if (endCol > line.length()) endCol = line.length();
        if (startCol >= endCol) return "";
        return line.substr(startCol, endCol - startCol);
    }
    
    std::ostringstream result;
    
    // First line
    const std::string& firstLine = m_lines[startLine].text;
    if (startCol < firstLine.length()) {
        result << firstLine.substr(startCol) << "\n";
    } else {
        result << "\n";
    }
    
    // Middle lines
    for (size_t i = startLine + 1; i < endLine; ++i) {
        result << m_lines[i].text << "\n";
    }
    
    // Last line
    const std::string& lastLine = m_lines[endLine].text;
    if (endCol > lastLine.length()) {
        endCol = lastLine.length();
    }
    result << lastLine.substr(0, endCol);
    
    return result.str();
}

std::string SourceDocument::deleteRange(size_t startLine, size_t startCol,
                                       size_t endLine, size_t endCol) {
    std::string deletedText = getTextRange(startLine, startCol, endLine, endCol);
    
    if (startLine >= m_lines.size()) {
        return deletedText;
    }
    
    if (endLine >= m_lines.size()) {
        endLine = m_lines.size() - 1;
    }
    
    if (startLine == endLine) {
        // Single line deletion
        std::string& line = m_lines[startLine].text;
        if (startCol > line.length()) startCol = line.length();
        if (endCol > line.length()) endCol = line.length();
        if (startCol < endCol) {
            line.erase(startCol, endCol - startCol);
            m_lines[startLine].isDirty = true;
        }
    } else {
        // Multi-line deletion
        std::string firstPart = m_lines[startLine].text.substr(0, startCol);
        std::string lastPart = (endCol < m_lines[endLine].text.length()) 
            ? m_lines[endLine].text.substr(endCol) 
            : "";
        
        // Combine first and last parts
        m_lines[startLine].text = firstPart + lastPart;
        m_lines[startLine].isDirty = true;
        
        // Delete middle lines
        for (size_t i = endLine; i > startLine; --i) {
            deleteLineAtIndex(i);
        }
    }
    
    markDirty();
    return deletedText;
}

// =============================================================================
// Line Numbering Operations
// =============================================================================

void SourceDocument::renumber(int start, int step) {
    int currentNumber = start;
    
    for (auto& line : m_lines) {
        if (line.lineNumber > 0) {
            line.lineNumber = currentNumber;
            line.isDirty = true;
            currentNumber += step;
        }
    }
    
    rebuildLineNumberIndex();
    markDirty();
}

void SourceDocument::setAutoNumbering(bool enabled, int start, int step) {
    m_autoNumbering = enabled;
    m_autoStart = start;
    m_autoStep = step;
    m_nextAutoNumber = start;
}

int SourceDocument::getNextAutoNumber() {
    if (!m_autoNumbering) {
        return 0;
    }
    
    int result = m_nextAutoNumber;
    m_nextAutoNumber += m_autoStep;
    return result;
}

void SourceDocument::stripLineNumbers() {
    for (auto& line : m_lines) {
        if (line.lineNumber > 0) {
            line.lineNumber = 0;
            line.isDirty = true;
        }
    }
    
    m_lineNumberIndex.clear();
    markDirty();
}

void SourceDocument::assignLineNumbers(int start, int step) {
    int currentNumber = start;
    
    for (auto& line : m_lines) {
        line.lineNumber = currentNumber;
        line.isDirty = true;
        currentNumber += step;
    }
    
    rebuildLineNumberIndex();
    markDirty();
}

bool SourceDocument::hasLineNumbers() const {
    for (const auto& line : m_lines) {
        if (line.lineNumber > 0) {
            return true;
        }
    }
    return false;
}

bool SourceDocument::isMixedMode() const {
    bool hasNumbered = false;
    bool hasUnnumbered = false;
    
    for (const auto& line : m_lines) {
        if (line.lineNumber > 0) {
            hasNumbered = true;
        } else {
            hasUnnumbered = true;
        }
        
        if (hasNumbered && hasUnnumbered) {
            return true;
        }
    }
    
    return false;
}

bool SourceDocument::isFullyNumbered() const {
    for (const auto& line : m_lines) {
        if (line.lineNumber == 0) {
            return false;
        }
    }
    return !m_lines.empty();
}

// =============================================================================
// Serialization
// =============================================================================

void SourceDocument::setText(const std::string& text) {
    m_lines.clear();
    m_lineNumberIndex.clear();
    
    auto lines = splitLines(text);
    if (lines.empty()) {
        markDirty();
        return;
    }
    
    m_lines.reserve(lines.size());
    
    for (const auto& lineText : lines) {
        m_lines.emplace_back(0, lineText);
    }
    
    markDirty();
}

std::string SourceDocument::getText() const {
    if (m_lines.empty()) {
        return "";
    }
    
    std::ostringstream result;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (i > 0) {
            result << "\n";
        }
        result << m_lines[i].text;
    }
    
    return result.str();
}

bool SourceDocument::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    setText(content);
    m_filename = filename;
    markClean();
    
    return true;
}

bool SourceDocument::saveToFile(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content = getText();
    file.write(content.c_str(), content.length());
    file.close();
    
    return file.good();
}

std::string SourceDocument::getTextRangeByNumber(int startLineNum, int endLineNum) const {
    std::ostringstream result;
    
    for (const auto& line : m_lines) {
        if (line.lineNumber >= startLineNum) {
            if (endLineNum > 0 && line.lineNumber > endLineNum) {
                break;
            }
            if (line.lineNumber > 0) {
                result << line.lineNumber << " ";
            }
            result << line.text << "\n";
        }
    }
    
    return result.str();
}

// =============================================================================
// Undo/Redo
// =============================================================================

void SourceDocument::pushUndoState() {
    m_undoStack.push_back(captureState());
    m_redoStack.clear();  // Clear redo stack on new action
    trimUndoStack();
}

bool SourceDocument::undo() {
    if (m_undoStack.empty()) {
        return false;
    }
    
    // Save current state to redo stack
    m_redoStack.push_back(captureState());
    
    // Restore from undo stack
    UndoState state = m_undoStack.back();
    m_undoStack.pop_back();
    restoreState(state);
    
    return true;
}

bool SourceDocument::redo() {
    if (m_redoStack.empty()) {
        return false;
    }
    
    // Save current state to undo stack
    m_undoStack.push_back(captureState());
    
    // Restore from redo stack
    UndoState state = m_redoStack.back();
    m_redoStack.pop_back();
    restoreState(state);
    
    return true;
}

void SourceDocument::clearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

// =============================================================================
// Dirty State / Change Tracking
// =============================================================================

void SourceDocument::markClean() {
    m_dirty = false;
    for (auto& line : m_lines) {
        line.isDirty = false;
    }
}

std::vector<size_t> SourceDocument::getDirtyLines() const {
    std::vector<size_t> dirtyIndices;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].isDirty) {
            dirtyIndices.push_back(i);
        }
    }
    return dirtyIndices;
}

void SourceDocument::markLinesClean() {
    for (auto& line : m_lines) {
        line.isDirty = false;
    }
}

// =============================================================================
// Compiler Integration
// =============================================================================

std::string SourceDocument::generateSourceForCompiler() const {
    if (m_lines.empty()) {
        return "";
    }
    
    std::ostringstream result;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (i > 0) {
            result << "\n";
        }
        
        // Include line number if present
        if (m_lines[i].lineNumber > 0) {
            result << m_lines[i].lineNumber << " ";
        }
        
        result << m_lines[i].text;
    }
    
    return result.str();
}

void SourceDocument::forEachLine(std::function<void(const SourceLine&, size_t)> callback) const {
    for (size_t i = 0; i < m_lines.size(); ++i) {
        callback(m_lines[i], i);
    }
}

// =============================================================================
// Statistics
// =============================================================================

SourceDocument::Statistics SourceDocument::getStatistics() const {
    Statistics stats;
    
    stats.lineCount = m_lines.size();
    stats.minLineNumber = INT_MAX;
    stats.maxLineNumber = 0;
    
    for (const auto& line : m_lines) {
        stats.totalCharacters += line.text.length();
        stats.totalBytes += line.text.size();
        
        if (line.lineNumber > 0) {
            stats.numberedLines++;
            stats.hasLineNumbers = true;
            if (line.lineNumber < stats.minLineNumber) {
                stats.minLineNumber = line.lineNumber;
            }
            if (line.lineNumber > stats.maxLineNumber) {
                stats.maxLineNumber = line.lineNumber;
            }
        } else {
            stats.unnumberedLines++;
        }
    }
    
    if (!stats.hasLineNumbers) {
        stats.minLineNumber = 0;
        stats.maxLineNumber = 0;
    }
    
    stats.hasMixedNumbering = (stats.numberedLines > 0 && stats.unnumberedLines > 0);
    
    return stats;
}

// =============================================================================
// Search Operations
// =============================================================================

std::vector<SourceDocument::SearchResult> SourceDocument::find(const std::string& pattern, bool caseSensitive) const {
    std::vector<SearchResult> results;
    
    if (pattern.empty()) {
        return results;
    }
    
    for (size_t lineIdx = 0; lineIdx < m_lines.size(); ++lineIdx) {
        const std::string& line = m_lines[lineIdx].text;
        size_t pos = 0;
        
        while (pos < line.length()) {
            size_t found;
            if (caseSensitive) {
                found = line.find(pattern, pos);
            } else {
                // Simple case-insensitive search
                std::string lowerLine = line;
                std::string lowerPattern = pattern;
                std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
                found = lowerLine.find(lowerPattern, pos);
            }
            
            if (found == std::string::npos) {
                break;
            }
            
            results.emplace_back(lineIdx, found, pattern.length(), m_lines[lineIdx].lineNumber);
            pos = found + 1;
        }
    }
    
    return results;
}

size_t SourceDocument::replaceAll(const std::string& pattern, const std::string& replacement) {
    size_t count = 0;
    
    for (auto& line : m_lines) {
        std::string& text = line.text;
        size_t pos = 0;
        
        while ((pos = text.find(pattern, pos)) != std::string::npos) {
            text.replace(pos, pattern.length(), replacement);
            pos += replacement.length();
            line.isDirty = true;
            line.version = m_version;
            count++;
        }
    }
    
    if (count > 0) {
        markDirty();
    }
    
    return count;
}

// =============================================================================
// Utility Functions
// =============================================================================

void SourceDocument::clear() {
    m_lines.clear();
    m_lineNumberIndex.clear();
    m_filename.clear();
    m_encoding = "UTF-8";
    m_version = 0;
    m_dirty = false;
    m_autoNumbering = false;
    m_autoStart = 10;
    m_autoStep = 10;
    m_nextAutoNumber = 10;
    clearUndoHistory();
}

bool SourceDocument::isValidPosition(size_t lineIndex, size_t column) const {
    if (m_lines.empty() || lineIndex >= m_lines.size()) {
        return false;
    }
    return column <= m_lines[lineIndex].text.length();
}

void SourceDocument::clampPosition(size_t& lineIndex, size_t& column) const {
    if (m_lines.empty()) {
        lineIndex = 0;
        column = 0;
        return;
    }
    
    if (lineIndex >= m_lines.size()) {
        lineIndex = m_lines.size() - 1;
    }
    
    if (lineIndex < m_lines.size() && column > m_lines[lineIndex].text.length()) {
        column = m_lines[lineIndex].text.length();
    }
}

DocumentLocation SourceDocument::getLocation(size_t lineIndex, size_t column) const {
    int lineNumber = 0;
    if (lineIndex < m_lines.size()) {
        lineNumber = m_lines[lineIndex].lineNumber;
    }
    return DocumentLocation(m_filename, lineIndex, column, lineNumber);
}

std::vector<std::string> SourceDocument::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    
    for (size_t i = 0; i < text.length(); ++i) {
        char ch = text[i];
        
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (ch == '\r') {
            // Handle \r\n
            if (i + 1 < text.length() && text[i + 1] == '\n') {
                ++i;
            }
            lines.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    
    // Add last line if not empty or if text ends with newline
    if (!current.empty() || (!text.empty() && (text.back() == '\n' || text.back() == '\r'))) {
        lines.push_back(current);
    }
    
    return lines;
}

std::vector<char32_t> SourceDocument::utf8ToUtf32(const std::string& utf8) {
    std::vector<char32_t> result;
    // Simplified UTF-8 decoding
    for (size_t i = 0; i < utf8.length(); ++i) {
        unsigned char c = utf8[i];
        if (c < 0x80) {
            result.push_back(c);
        } else {
            // Multi-byte character (simplified)
            result.push_back(c);  // Would need proper UTF-8 decoding
        }
    }
    return result;
}

std::string SourceDocument::utf32ToUtf8(char32_t codepoint) {
    // Simplified UTF-8 encoding
    if (codepoint < 0x80) {
        return std::string(1, static_cast<char>(codepoint));
    }
    // Would need proper UTF-8 encoding for higher codepoints
    return std::string(1, static_cast<char>(codepoint & 0xFF));
}

size_t SourceDocument::utf8Length(const std::string& utf8) {
    size_t length = 0;
    for (size_t i = 0; i < utf8.length(); ++i) {
        unsigned char c = utf8[i];
        if ((c & 0xC0) != 0x80) {  // Not a continuation byte
            ++length;
        }
    }
    return length;
}

// =============================================================================
// Internal Helpers
// =============================================================================

void SourceDocument::rebuildLineNumberIndex() {
    m_lineNumberIndex.clear();
    
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].lineNumber > 0) {
            m_lineNumberIndex[m_lines[i].lineNumber] = i;
        }
    }
}

size_t SourceDocument::findInsertionPoint(int lineNumber) const {
    // Binary search for insertion point
    size_t left = 0;
    size_t right = m_lines.size();
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        
        int midLineNumber = m_lines[mid].lineNumber;
        if (midLineNumber == 0 || midLineNumber < lineNumber) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return left;
}

SourceDocument::UndoState SourceDocument::captureState() const {
    UndoState state;
    state.lines = m_lines;
    state.lineNumberIndex = m_lineNumberIndex;
    state.version = m_version;
    return state;
}

void SourceDocument::restoreState(const UndoState& state) {
    m_lines = state.lines;
    m_lineNumberIndex = state.lineNumberIndex;
    m_version = state.version;
    markDirty();
}

void SourceDocument::trimUndoStack() {
    if (m_maxUndoStates == 0) {
        return;  // Unlimited
    }
    
    while (m_undoStack.size() > m_maxUndoStates) {
        m_undoStack.pop_front();
    }
}

void SourceDocument::ensureNonEmpty() {
    // Allow empty documents
}

void SourceDocument::validateIndices() const {
    // Debug validation
    for (const auto& pair : m_lineNumberIndex) {
        int lineNumber = pair.first;
        size_t index = pair.second;
        
        assert(index < m_lines.size());
        assert(m_lines[index].lineNumber == lineNumber);
    }
}

} // namespace FasterBASIC