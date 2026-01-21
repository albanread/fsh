//
//  SourceDocument.h
//  FasterBASIC Framework - Unified Source Code Structure
//
//  Single unified representation for BASIC source code that serves
//  Editor, REPL, and Compiler needs with dual indexing system.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#ifndef SOURCE_DOCUMENT_H
#define SOURCE_DOCUMENT_H

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>

namespace FasterBASIC {

// =============================================================================
// SourceLine - Individual line with metadata
// =============================================================================

struct SourceLine {
    int lineNumber;              // BASIC line number (0 = unnumbered)
    std::string text;            // UTF-8 text content
    uint64_t version;            // Version counter for change tracking
    bool isDirty;                // Modified since last compile
    
    // Constructor
    SourceLine()
        : lineNumber(0), version(0), isDirty(true) {}
    
    SourceLine(int num, const std::string& txt)
        : lineNumber(num), text(txt), version(0), isDirty(true) {}
    
    SourceLine(int num, std::string&& txt)
        : lineNumber(num), text(std::move(txt)), version(0), isDirty(true) {}
    
    // Copy and move
    SourceLine(const SourceLine&) = default;
    SourceLine(SourceLine&&) noexcept = default;
    SourceLine& operator=(const SourceLine&) = default;
    SourceLine& operator=(SourceLine&&) noexcept = default;
};

// =============================================================================
// DocumentLocation - Position in source for error reporting
// =============================================================================

struct DocumentLocation {
    std::string filename;
    size_t lineIndex;           // 0-based line index
    size_t column;              // 0-based column
    int lineNumber;             // BASIC line number (0 if unnumbered)
    
    DocumentLocation()
        : lineIndex(0), column(0), lineNumber(0) {}
    
    DocumentLocation(const std::string& file, size_t idx, size_t col, int num)
        : filename(file), lineIndex(idx), column(col), lineNumber(num) {}
    
    std::string toString() const;
};

// =============================================================================
// SourceDocument - Unified source code representation
// =============================================================================

class SourceDocument {
public:
    // =========================================================================
    // Construction
    // =========================================================================
    
    SourceDocument();
    explicit SourceDocument(const std::string& filename);
    ~SourceDocument();
    
    // Copy/move
    SourceDocument(const SourceDocument& other);
    SourceDocument(SourceDocument&& other) noexcept;
    SourceDocument& operator=(const SourceDocument& other);
    SourceDocument& operator=(SourceDocument&& other) noexcept;
    
    // =========================================================================
    // Line Access - Dual Index System
    // =========================================================================
    
    // By sequential index (0-based, for editor/iteration)
    SourceLine& getLineByIndex(size_t index);
    const SourceLine& getLineByIndex(size_t index) const;
    size_t getLineCount() const { return m_lines.size(); }
    bool isEmpty() const { return m_lines.empty() || (m_lines.size() == 1 && m_lines[0].text.empty() && m_lines[0].lineNumber == 0); }
    
    // By line number (for BASIC GOTO/GOSUB/renumber)
    SourceLine* getLineByNumber(int lineNumber);
    const SourceLine* getLineByNumber(int lineNumber) const;
    bool hasLineNumber(int lineNumber) const;
    std::vector<int> getLineNumbers() const;
    
    // Find index for a given line number
    size_t getIndexForLineNumber(int lineNumber) const;
    
    // =========================================================================
    // Line Modification - REPL Style (by line number)
    // =========================================================================
    
    /// Set line by number (insert if new, replace if exists)
    void setLineByNumber(int lineNumber, const std::string& text);
    
    /// Delete line by number
    bool deleteLineByNumber(int lineNumber);
    
    // =========================================================================
    // Line Modification - Editor Style (by index)
    // =========================================================================
    
    /// Insert new line at index position
    void insertLineAtIndex(size_t index, const std::string& text, int lineNumber = 0);
    
    /// Delete line at index
    bool deleteLineAtIndex(size_t index);
    
    /// Replace line at index
    bool replaceLineAtIndex(size_t index, const std::string& text);
    
    /// Split line at column (creates new line)
    bool splitLine(size_t index, size_t column);
    
    /// Join line with next line
    bool joinWithNext(size_t index);
    
    // =========================================================================
    // Character-Level Operations (for editor)
    // =========================================================================
    
    /// Insert character at position
    bool insertChar(size_t lineIndex, size_t column, char32_t ch);
    
    /// Delete character at position
    bool deleteChar(size_t lineIndex, size_t column);
    
    /// Insert text at position (may contain newlines)
    bool insertText(size_t lineIndex, size_t column, const std::string& text);
    
    /// Get character at position
    char32_t getChar(size_t lineIndex, size_t column) const;
    
    // =========================================================================
    // Range Operations
    // =========================================================================
    
    /// Get text range
    std::string getTextRange(size_t startLine, size_t startCol,
                            size_t endLine, size_t endCol) const;
    
    /// Delete text range
    std::string deleteRange(size_t startLine, size_t startCol,
                           size_t endLine, size_t endCol);
    
    // =========================================================================
    // Line Numbering Operations
    // =========================================================================
    
    /// Renumber all lines with line numbers
    void renumber(int start = 10, int step = 10);
    
    /// Auto-numbering mode (for REPL)
    void setAutoNumbering(bool enabled, int start = 10, int step = 10);
    bool isAutoNumberingEnabled() const { return m_autoNumbering; }
    int getNextAutoNumber();
    
    /// Strip all line numbers (convert to unnumbered)
    void stripLineNumbers();
    
    /// Assign line numbers to all lines
    void assignLineNumbers(int start = 10, int step = 10);
    
    /// Check document numbering style
    bool hasLineNumbers() const;        // Any line has a number
    bool isMixedMode() const;           // Some numbered, some not
    bool isFullyNumbered() const;       // All lines numbered
    
    // =========================================================================
    // Serialization
    // =========================================================================
    
    /// Set entire content from string (splits on newlines)
    void setText(const std::string& text);
    
    /// Get entire content as string
    std::string getText() const;
    
    /// Load from file
    bool loadFromFile(const std::string& filename);
    
    /// Save to file
    bool saveToFile(const std::string& filename) const;
    
    /// Get text range by line numbers (for LIST command)
    std::string getTextRangeByNumber(int startLineNum, int endLineNum = -1) const;
    
    // =========================================================================
    // Undo/Redo
    // =========================================================================
    
    /// Push current state onto undo stack
    void pushUndoState();
    
    /// Undo last change
    bool undo();
    
    /// Redo last undone change
    bool redo();
    
    /// Check if undo/redo available
    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    
    /// Clear undo/redo history
    void clearUndoHistory();
    
    /// Set maximum undo states (0 = unlimited)
    void setMaxUndoStates(size_t max) { m_maxUndoStates = max; }
    size_t getMaxUndoStates() const { return m_maxUndoStates; }
    
    /// Get undo stack size
    size_t getUndoStackSize() const { return m_undoStack.size(); }
    size_t getRedoStackSize() const { return m_redoStack.size(); }
    
    // =========================================================================
    // Dirty State / Change Tracking
    // =========================================================================
    
    bool isDirty() const { return m_dirty; }
    void markClean();
    void markDirty() { m_dirty = true; incrementVersion(); }
    
    uint64_t getVersion() const { return m_version; }
    
    /// Get indices of lines changed since last markLinesClean()
    std::vector<size_t> getDirtyLines() const;
    
    /// Mark all lines as clean (after successful compile)
    void markLinesClean();
    
    // =========================================================================
    // Metadata
    // =========================================================================
    
    void setFilename(const std::string& filename) { m_filename = filename; }
    std::string getFilename() const { return m_filename; }
    bool hasFilename() const { return !m_filename.empty(); }
    
    void setEncoding(const std::string& encoding) { m_encoding = encoding; }
    std::string getEncoding() const { return m_encoding; }
    
    // =========================================================================
    // Compiler Integration
    // =========================================================================
    
    /// Generate source text for compiler (preserves line numbers if present)
    std::string generateSourceForCompiler() const;
    
    /// Get direct access to lines (for fast iteration)
    const std::vector<SourceLine>& getLines() const { return m_lines; }
    
    /// Iterate over lines with callback
    void forEachLine(std::function<void(const SourceLine&, size_t index)> callback) const;
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct Statistics {
        size_t lineCount;
        size_t totalCharacters;
        size_t totalBytes;
        int minLineNumber;
        int maxLineNumber;
        bool hasLineNumbers;
        bool hasMixedNumbering;
        size_t numberedLines;
        size_t unnumberedLines;
        
        Statistics()
            : lineCount(0), totalCharacters(0), totalBytes(0)
            , minLineNumber(0), maxLineNumber(0)
            , hasLineNumbers(false), hasMixedNumbering(false)
            , numberedLines(0), unnumberedLines(0) {}
    };
    
    Statistics getStatistics() const;
    
    // =========================================================================
    // Search Operations
    // =========================================================================
    
    struct SearchResult {
        size_t lineIndex;
        size_t column;
        size_t length;
        int lineNumber;         // For display
        
        SearchResult(size_t idx, size_t col, size_t len, int num)
            : lineIndex(idx), column(col), length(len), lineNumber(num) {}
    };
    
    /// Find all occurrences of pattern
    std::vector<SearchResult> find(const std::string& pattern, bool caseSensitive = true) const;
    
    /// Replace all occurrences
    size_t replaceAll(const std::string& pattern, const std::string& replacement);
    
    // =========================================================================
    // Utility Functions
    // =========================================================================
    
    /// Clear entire document
    void clear();
    
    /// Validate position
    bool isValidPosition(size_t lineIndex, size_t column) const;
    
    /// Clamp position to valid range
    void clampPosition(size_t& lineIndex, size_t& column) const;
    
    /// Get source location for position
    DocumentLocation getLocation(size_t lineIndex, size_t column) const;
    
    /// Split text by newlines
    static std::vector<std::string> splitLines(const std::string& text);
    
    /// UTF-8 utilities
    static std::vector<char32_t> utf8ToUtf32(const std::string& utf8);
    static std::string utf32ToUtf8(char32_t codepoint);
    static size_t utf8Length(const std::string& utf8);
    
private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    // Primary storage: sequential vector for fast iteration
    std::vector<SourceLine> m_lines;
    
    // Secondary index: line number -> vector index for O(1) lookup
    std::unordered_map<int, size_t> m_lineNumberIndex;
    
    // Metadata
    std::string m_filename;
    std::string m_encoding;             // Default: "UTF-8"
    uint64_t m_version;                 // Increments on each change
    bool m_dirty;                       // Modified since last save
    
    // Auto-numbering state (for REPL)
    bool m_autoNumbering;
    int m_autoStart;
    int m_autoStep;
    int m_nextAutoNumber;
    
    // Undo/redo stacks
    struct UndoState {
        std::vector<SourceLine> lines;
        std::unordered_map<int, size_t> lineNumberIndex;
        uint64_t version;
        
        UndoState() : version(0) {}
    };
    
    std::deque<UndoState> m_undoStack;
    std::deque<UndoState> m_redoStack;
    size_t m_maxUndoStates;             // 0 = unlimited
    
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Rebuild line number index from scratch
    void rebuildLineNumberIndex();
    
    /// Increment version counter
    void incrementVersion() { ++m_version; }
    
    /// Find insertion point for line number (binary search)
    size_t findInsertionPoint(int lineNumber) const;
    
    /// Capture current state for undo
    UndoState captureState() const;
    
    /// Restore state from undo
    void restoreState(const UndoState& state);
    
    /// Trim undo stack if needed
    void trimUndoStack();
    
    /// Ensure at least one line exists
    void ensureNonEmpty();
    
    /// Validate internal consistency (debug)
    void validateIndices() const;
};

} // namespace FasterBASIC

#endif // SOURCE_DOCUMENT_H