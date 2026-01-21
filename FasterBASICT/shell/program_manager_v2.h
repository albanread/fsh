//
// program_manager_v2.h
// FasterBASIC Shell - Program Storage and Management V2
//
// Compatibility wrapper around REPLView that maintains the old ProgramManager API.
// This allows gradual migration from ProgramManager to the unified SourceDocument architecture.
//

#ifndef PROGRAM_MANAGER_V2_H
#define PROGRAM_MANAGER_V2_H

#include "REPLView.h"
#include "../src/SourceDocument.h"
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <memory>

namespace FasterBASIC {

class ProgramManagerV2 {
public:
    ProgramManagerV2();
    ~ProgramManagerV2();

    // Line management
    void setLine(int lineNumber, const std::string& code);
    void setLineRaw(int lineNumber, const std::string& code); // No trimming - for screen editor
    void deleteLine(int lineNumber);
    std::string getLine(int lineNumber) const;
    bool hasLine(int lineNumber) const;
    void clear();

    // Program queries
    bool isEmpty() const;
    bool isModified() const;
    void setModified(bool modified = true);
    size_t getLineCount() const;
    
    // Line number operations
    std::vector<int> getLineNumbers() const;
    int getFirstLineNumber() const;
    int getLastLineNumber() const;
    int getNextLineNumber(int currentLine) const;
    int getPreviousLineNumber(int currentLine) const;

    // Program generation
    std::string generateProgram() const;
    std::string generateProgramRange(int startLine, int endLine = -1) const;
    
    // Renumbering
    void renumber(int startLine = 10, int step = 10);
    
    // File operations
    void setFilename(const std::string& filename);
    std::string getFilename() const;
    bool hasFilename() const;

    // Listing operations
    struct ListRange {
        int startLine;
        int endLine;
        bool hasStart;
        bool hasEnd;
        
        ListRange() : startLine(0), endLine(0), hasStart(false), hasEnd(false) {}
        ListRange(int start, int end) : startLine(start), endLine(end), hasStart(true), hasEnd(true) {}
    };
    
    std::vector<std::pair<int, std::string>> getLines(const ListRange& range) const;
    std::vector<std::pair<int, std::string>> getAllLines() const;

    // Statistics
    struct ProgramStats {
        size_t lineCount;
        size_t totalCharacters;
        int minLineNumber;
        int maxLineNumber;
        bool hasGaps;
        
        ProgramStats() : lineCount(0), totalCharacters(0), minLineNumber(0), maxLineNumber(0), hasGaps(false) {}
    };
    
    ProgramStats getStatistics() const;

    // Auto-numbering support
    void setAutoMode(bool enabled, int start = 10, int step = 10);
    bool isAutoMode() const;
    int getNextAutoLine();
    void incrementAutoLine();

    // Undo/Redo support (new functionality from REPLView)
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    void clearUndoHistory();

    // Access to underlying REPLView (for migration purposes)
    REPLView& getREPLView() { return *m_replView; }
    const REPLView& getREPLView() const { return *m_replView; }
    std::shared_ptr<SourceDocument> getDocument() { return m_document; }

private:
    std::shared_ptr<SourceDocument> m_document;
    std::unique_ptr<REPLView> m_replView;
    
    // Cached modified flag (REPLView tracks dirty flag internally)
    // We maintain this for exact API compatibility
    mutable bool m_modifiedFlag;
    
    // Helper to sync modified flag
    void updateModifiedFlag() const;
};

} // namespace FasterBASIC

#endif // PROGRAM_MANAGER_V2_H