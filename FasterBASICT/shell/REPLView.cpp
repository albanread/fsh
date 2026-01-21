//
//  REPLView.cpp
//  FasterBASIC Framework - REPL View Adapter
//
//  Implementation of REPL-oriented view adapter for SourceDocument.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#include "REPLView.h"
#include "../src/basic_formatter_lib.h"
#include <sstream>
#include <algorithm>

namespace FasterBASIC {

// =============================================================================
// Construction
// =============================================================================

REPLView::REPLView(std::shared_ptr<SourceDocument> document)
    : m_document(document)
{
    if (!m_document) {
        m_document = std::make_shared<SourceDocument>();
    }
}

REPLView::~REPLView() = default;

// =============================================================================
// Line-Numbered Operations
// =============================================================================

void REPLView::setLine(int lineNumber, const std::string& text) {
    if (lineNumber <= 0) {
        return;  // Invalid line number for REPL
    }
    
    m_document->setLineByNumber(lineNumber, text);
}

bool REPLView::deleteLine(int lineNumber) {
    return m_document->deleteLineByNumber(lineNumber);
}

std::string REPLView::getLine(int lineNumber) const {
    const SourceLine* line = m_document->getLineByNumber(lineNumber);
    if (line) {
        return line->text;
    }
    return "";
}

bool REPLView::hasLine(int lineNumber) const {
    return m_document->hasLineNumber(lineNumber);
}

// =============================================================================
// Program Management
// =============================================================================

void REPLView::clear() {
    m_document->clear();
}

bool REPLView::isEmpty() const {
    return m_document->isEmpty();
}

size_t REPLView::getLineCount() const {
    return m_document->getLineCount();
}

bool REPLView::isModified() const {
    return m_document->isDirty();
}

void REPLView::markSaved() {
    m_document->markClean();
}

// =============================================================================
// Listing Operations
// =============================================================================

std::vector<REPLView::LineEntry> REPLView::list() const {
    std::vector<LineEntry> result;
    
    auto lineNumbers = m_document->getLineNumbers();
    result.reserve(lineNumbers.size());
    
    for (int lineNum : lineNumbers) {
        const SourceLine* line = m_document->getLineByNumber(lineNum);
        if (line) {
            result.emplace_back(lineNum, line->text);
        }
    }
    
    return result;
}

std::vector<REPLView::LineEntry> REPLView::listRange(int startLineNum, int endLineNum) const {
    std::vector<LineEntry> result;
    
    auto lineNumbers = m_document->getLineNumbers();
    
    for (int lineNum : lineNumbers) {
        if (lineNum < startLineNum) {
            continue;
        }
        if (endLineNum > 0 && lineNum > endLineNum) {
            break;
        }
        
        const SourceLine* line = m_document->getLineByNumber(lineNum);
        if (line) {
            result.emplace_back(lineNum, line->text);
        }
    }
    
    return result;
}

std::string REPLView::getFormattedListing(int startLineNum, int endLineNum) const {
    std::ostringstream result;
    
    std::vector<LineEntry> lines;
    if (startLineNum < 0) {
        lines = list();
    } else {
        lines = listRange(startLineNum, endLineNum);
    }
    
    for (const auto& entry : lines) {
        result << entry.first << " " << entry.second << "\n";
    }
    
    return result.str();
}

// =============================================================================
// Line Numbering
// =============================================================================

void REPLView::renumber(int start, int step) {
    // Use formatter to renumber with GOTO/GOSUB reference updates
    renumberREPLView(*this, start, step);
}

void REPLView::setAutoNumbering(bool enabled, int start, int step) {
    m_document->setAutoNumbering(enabled, start, step);
}

bool REPLView::isAutoNumberingEnabled() const {
    return m_document->isAutoNumberingEnabled();
}

int REPLView::getNextLineNumber() {
    return m_document->getNextAutoNumber();
}

std::vector<int> REPLView::getLineNumbers() const {
    return m_document->getLineNumbers();
}

int REPLView::getFirstLineNumber() const {
    auto numbers = m_document->getLineNumbers();
    if (numbers.empty()) {
        return 0;
    }
    return numbers.front();
}

int REPLView::getLastLineNumber() const {
    auto numbers = m_document->getLineNumbers();
    if (numbers.empty()) {
        return 0;
    }
    return numbers.back();
}

// =============================================================================
// File Operations
// =============================================================================

bool REPLView::save(const std::string& filename) {
    bool success = m_document->saveToFile(filename);
    if (success) {
        m_document->setFilename(filename);
        m_document->markClean();
    }
    return success;
}

bool REPLView::load(const std::string& filename) {
    bool success = m_document->loadFromFile(filename);
    if (success) {
        m_document->setFilename(filename);
        m_document->markClean();
    }
    return success;
}

std::string REPLView::getFilename() const {
    return m_document->getFilename();
}

void REPLView::setFilename(const std::string& filename) {
    m_document->setFilename(filename);
}

bool REPLView::hasFilename() const {
    return m_document->hasFilename();
}

// =============================================================================
// Undo/Redo
// =============================================================================

void REPLView::pushUndoState() {
    m_document->pushUndoState();
}

bool REPLView::undo() {
    return m_document->undo();
}

bool REPLView::redo() {
    return m_document->redo();
}

bool REPLView::canUndo() const {
    return m_document->canUndo();
}

bool REPLView::canRedo() const {
    return m_document->canRedo();
}

void REPLView::clearUndoHistory() {
    m_document->clearUndoHistory();
}

// =============================================================================
// Program Execution Preparation
// =============================================================================

std::string REPLView::generateSource() const {
    return m_document->generateSourceForCompiler();
}

std::string REPLView::getExecutableSource() const {
    return m_document->generateSourceForCompiler();
}

// =============================================================================
// Statistics
// =============================================================================

REPLView::Statistics REPLView::getStatistics() const {
    Statistics stats;
    
    auto docStats = m_document->getStatistics();
    
    stats.lineCount = docStats.lineCount;
    stats.totalCharacters = docStats.totalCharacters;
    stats.minLineNumber = docStats.minLineNumber;
    stats.maxLineNumber = docStats.maxLineNumber;
    
    // Check for gaps in line numbering
    auto lineNumbers = m_document->getLineNumbers();
    if (lineNumbers.size() > 1) {
        int largestGap = 0;
        for (size_t i = 1; i < lineNumbers.size(); ++i) {
            int gap = lineNumbers[i] - lineNumbers[i-1];
            if (gap > 1) {
                stats.hasGaps = true;
            }
            if (gap > largestGap) {
                largestGap = gap;
            }
        }
        stats.largestGap = largestGap;
    }
    
    return stats;
}

} // namespace FasterBASIC