//
//  REPLView.h
//  FasterBASIC Framework - REPL View Adapter
//
//  Adapter class that provides REPL-oriented interface to SourceDocument.
//  Designed for interactive shells with line-numbered BASIC programs.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#ifndef REPL_VIEW_H
#define REPL_VIEW_H

#include "../src/SourceDocument.h"
#include <memory>
#include <vector>
#include <string>
#include <utility>

namespace FasterBASIC {

// =============================================================================
// REPLView - REPL-oriented view of SourceDocument
// =============================================================================

class REPLView {
public:
    // =========================================================================
    // Construction
    // =========================================================================
    
    /// Create view wrapping a SourceDocument
    explicit REPLView(std::shared_ptr<SourceDocument> document);
    
    /// Destructor
    ~REPLView();
    
    // No copy (shared pointer semantics)
    REPLView(const REPLView&) = delete;
    REPLView& operator=(const REPLView&) = delete;
    
    // Move allowed
    REPLView(REPLView&&) noexcept = default;
    REPLView& operator=(REPLView&&) noexcept = default;
    
    // =========================================================================
    // Line-Numbered Operations (REPL Style)
    // =========================================================================
    
    /// Set or replace a line by number
    /// @param lineNumber BASIC line number (must be > 0)
    /// @param text Line content (without line number)
    void setLine(int lineNumber, const std::string& text);
    
    /// Delete a line by number
    /// @param lineNumber BASIC line number
    /// @return true if line existed and was deleted
    bool deleteLine(int lineNumber);
    
    /// Get line text by number
    /// @param lineNumber BASIC line number
    /// @return Line text (empty if not found)
    std::string getLine(int lineNumber) const;
    
    /// Check if line number exists
    /// @param lineNumber BASIC line number
    /// @return true if line exists
    bool hasLine(int lineNumber) const;
    
    // =========================================================================
    // Program Management
    // =========================================================================
    
    /// Clear entire program
    void clear();
    
    /// Check if program is empty
    bool isEmpty() const;
    
    /// Get number of lines
    size_t getLineCount() const;
    
    /// Check if program has been modified
    bool isModified() const;
    
    /// Mark program as saved (clear dirty flag)
    void markSaved();
    
    // =========================================================================
    // Listing Operations
    // =========================================================================
    
    /// Line number and text pair
    using LineEntry = std::pair<int, std::string>;
    
    /// List all lines
    /// @return Vector of (lineNumber, text) pairs sorted by line number
    std::vector<LineEntry> list() const;
    
    /// List range of lines by line number
    /// @param startLineNum First line number (inclusive)
    /// @param endLineNum Last line number (inclusive, -1 = end)
    /// @return Vector of (lineNumber, text) pairs
    std::vector<LineEntry> listRange(int startLineNum, int endLineNum = -1) const;
    
    /// Get formatted listing (with line numbers)
    /// @param startLineNum First line number (-1 = all)
    /// @param endLineNum Last line number (-1 = end)
    /// @return Formatted string with line numbers and text
    std::string getFormattedListing(int startLineNum = -1, int endLineNum = -1) const;
    
    // =========================================================================
    // Line Numbering
    // =========================================================================
    
    /// Renumber all lines
    /// @param start Starting line number (default: 10)
    /// @param step Increment between lines (default: 10)
    void renumber(int start = 10, int step = 10);
    
    /// Enable/disable auto line numbering
    /// @param enabled Enable auto-numbering
    /// @param start Starting line number (default: 10)
    /// @param step Increment between lines (default: 10)
    void setAutoNumbering(bool enabled, int start = 10, int step = 10);
    
    /// Check if auto-numbering is enabled
    bool isAutoNumberingEnabled() const;
    
    /// Get next auto line number (and increment counter)
    /// @return Next line number (0 if auto-numbering disabled)
    int getNextLineNumber();
    
    /// Get all line numbers (sorted)
    /// @return Vector of line numbers
    std::vector<int> getLineNumbers() const;
    
    /// Get first line number
    /// @return First line number (0 if empty)
    int getFirstLineNumber() const;
    
    /// Get last line number
    /// @return Last line number (0 if empty)
    int getLastLineNumber() const;
    
    // =========================================================================
    // File Operations
    // =========================================================================
    
    /// Save program to file
    /// @param filename File path
    /// @return true if successful
    bool save(const std::string& filename);
    
    /// Load program from file
    /// @param filename File path
    /// @return true if successful
    bool load(const std::string& filename);
    
    /// Get current filename
    /// @return Filename (empty if not set)
    std::string getFilename() const;
    
    /// Set filename without saving
    void setFilename(const std::string& filename);
    
    /// Check if filename is set
    bool hasFilename() const;
    
    // =========================================================================
    // Undo/Redo
    // =========================================================================
    
    /// Push current state for undo
    void pushUndoState();
    
    /// Undo last change
    /// @return true if undo was performed
    bool undo();
    
    /// Redo last undone change
    /// @return true if redo was performed
    bool redo();
    
    /// Check if undo is available
    bool canUndo() const;
    
    /// Check if redo is available
    bool canRedo() const;
    
    /// Clear undo history
    void clearUndoHistory();
    
    // =========================================================================
    // Program Execution Preparation
    // =========================================================================
    
    /// Generate source code for compiler
    /// @return Source code with line numbers
    std::string generateSource() const;
    
    /// Get source for execution (numbered lines)
    /// @return Source code ready for compiler
    std::string getExecutableSource() const;
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    /// Program statistics
    struct Statistics {
        size_t lineCount;           // Total lines
        size_t totalCharacters;     // Total character count
        int minLineNumber;          // Lowest line number
        int maxLineNumber;          // Highest line number
        bool hasGaps;               // True if gaps in line numbering
        int largestGap;             // Largest gap between consecutive lines
        
        Statistics()
            : lineCount(0), totalCharacters(0)
            , minLineNumber(0), maxLineNumber(0)
            , hasGaps(false), largestGap(0) {}
    };
    
    /// Get program statistics
    Statistics getStatistics() const;
    
    // =========================================================================
    // Direct Document Access
    // =========================================================================
    
    /// Get underlying document (const)
    const SourceDocument& getDocument() const { return *m_document; }
    
    /// Get underlying document (mutable - use with care)
    SourceDocument& getDocument() { return *m_document; }
    
    /// Get shared pointer to document
    std::shared_ptr<SourceDocument> getDocumentPtr() { return m_document; }

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    std::shared_ptr<SourceDocument> m_document;     // Underlying document
};

} // namespace FasterBASIC

#endif // REPL_VIEW_H