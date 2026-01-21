//
// program_manager_v2.cpp
// FasterBASIC Shell - Program Storage and Management V2
//
// Compatibility wrapper around REPLView that maintains the old ProgramManager API.
// This allows gradual migration from ProgramManager to the unified SourceDocument architecture.
//

#include "program_manager_v2.h"
#include <algorithm>

namespace FasterBASIC {

ProgramManagerV2::ProgramManagerV2() 
    : m_document(std::make_shared<SourceDocument>())
    , m_replView(std::make_unique<REPLView>(m_document))
    , m_modifiedFlag(false)
{
}

ProgramManagerV2::~ProgramManagerV2() {
}

void ProgramManagerV2::setLine(int lineNumber, const std::string& code) {
    if (lineNumber <= 0 || lineNumber > 65535) {
        return;
    }
    
    // Push undo state before modification
    m_replView->pushUndoState();
    
    // Trim whitespace from code (matching old behavior)
    std::string trimmedCode = code;
    size_t start = trimmedCode.find_first_not_of(" \t");
    if (start == std::string::npos) {
        // Empty line - delete it
        deleteLine(lineNumber);
        return;
    }
    
    size_t end = trimmedCode.find_last_not_of(" \t\r\n");
    trimmedCode = trimmedCode.substr(start, end - start + 1);
    
    if (trimmedCode.empty()) {
        // Empty line - delete it
        deleteLine(lineNumber);
        return;
    }
    
    m_replView->setLine(lineNumber, trimmedCode);
    m_modifiedFlag = true;
}

void ProgramManagerV2::setLineRaw(int lineNumber, const std::string& code) {
    if (lineNumber <= 0 || lineNumber > 65535) {
        return;
    }
    
    // Push undo state before modification
    m_replView->pushUndoState();
    
    // No trimming - preserve exact content for screen editor
    // Allow empty lines
    m_replView->setLine(lineNumber, code);
    m_modifiedFlag = true;
}

void ProgramManagerV2::deleteLine(int lineNumber) {
    // Push undo state before modification
    m_replView->pushUndoState();
    
    if (m_replView->deleteLine(lineNumber)) {
        m_modifiedFlag = true;
    }
}

std::string ProgramManagerV2::getLine(int lineNumber) const {
    return m_replView->getLine(lineNumber);
}

bool ProgramManagerV2::hasLine(int lineNumber) const {
    return m_replView->hasLine(lineNumber);
}

void ProgramManagerV2::clear() {
    m_replView->clear();
    m_modifiedFlag = false;
}

bool ProgramManagerV2::isEmpty() const {
    return m_replView->isEmpty();
}

bool ProgramManagerV2::isModified() const {
    updateModifiedFlag();
    return m_modifiedFlag;
}

void ProgramManagerV2::setModified(bool modified) {
    m_modifiedFlag = modified;
    if (!modified) {
        m_replView->markSaved();
    }
}

size_t ProgramManagerV2::getLineCount() const {
    return m_replView->getLineCount();
}

std::vector<int> ProgramManagerV2::getLineNumbers() const {
    return m_replView->getLineNumbers();
}

int ProgramManagerV2::getFirstLineNumber() const {
    int first = m_replView->getFirstLineNumber();
    return (first == 0) ? -1 : first;  // Convert 0 to -1 for old API compatibility
}

int ProgramManagerV2::getLastLineNumber() const {
    int last = m_replView->getLastLineNumber();
    return (last == 0) ? -1 : last;  // Convert 0 to -1 for old API compatibility
}

int ProgramManagerV2::getNextLineNumber(int currentLine) const {
    auto lineNumbers = m_replView->getLineNumbers();
    auto it = std::upper_bound(lineNumbers.begin(), lineNumbers.end(), currentLine);
    if (it != lineNumbers.end()) {
        return *it;
    }
    return -1;
}

int ProgramManagerV2::getPreviousLineNumber(int currentLine) const {
    auto lineNumbers = m_replView->getLineNumbers();
    auto it = std::lower_bound(lineNumbers.begin(), lineNumbers.end(), currentLine);
    if (it != lineNumbers.begin()) {
        --it;
        return *it;
    }
    return -1;
}

std::string ProgramManagerV2::generateProgram() const {
    return m_replView->generateSource();
}

std::string ProgramManagerV2::generateProgramRange(int startLine, int endLine) const {
    auto lines = m_replView->listRange(startLine, endLine);
    std::string result;
    
    for (const auto& line : lines) {
        result += std::to_string(line.first) + " " + line.second + "\n";
    }
    
    return result;
}

void ProgramManagerV2::renumber(int startLine, int step) {
    // Push undo state before modification
    m_replView->pushUndoState();
    
    // REPLView::renumber now handles GOTO/GOSUB reference updates via formatter
    m_replView->renumber(startLine, step);
    m_modifiedFlag = true;
}

void ProgramManagerV2::setFilename(const std::string& filename) {
    m_replView->setFilename(filename);
}

std::string ProgramManagerV2::getFilename() const {
    return m_replView->getFilename();
}

bool ProgramManagerV2::hasFilename() const {
    return m_replView->hasFilename();
}

std::vector<std::pair<int, std::string>> ProgramManagerV2::getLines(const ListRange& range) const {
    if (!range.hasStart && !range.hasEnd) {
        return m_replView->list();
    }
    
    int startLine = range.hasStart ? range.startLine : -1;
    int endLine = range.hasEnd ? range.endLine : -1;
    
    return m_replView->listRange(startLine, endLine);
}

std::vector<std::pair<int, std::string>> ProgramManagerV2::getAllLines() const {
    return m_replView->list();
}

ProgramManagerV2::ProgramStats ProgramManagerV2::getStatistics() const {
    auto stats = m_replView->getStatistics();
    
    ProgramStats result;
    result.lineCount = stats.lineCount;
    result.totalCharacters = stats.totalCharacters;
    result.minLineNumber = stats.minLineNumber;
    result.maxLineNumber = stats.maxLineNumber;
    result.hasGaps = stats.hasGaps;
    
    return result;
}

void ProgramManagerV2::setAutoMode(bool enabled, int start, int step) {
    m_replView->setAutoNumbering(enabled, start, step);
}

bool ProgramManagerV2::isAutoMode() const {
    return m_replView->isAutoNumberingEnabled();
}

int ProgramManagerV2::getNextAutoLine() {
    return m_replView->getNextLineNumber();
}

void ProgramManagerV2::incrementAutoLine() {
    // In the new architecture, getNextLineNumber() automatically increments
    // So this is a no-op for compatibility
}

bool ProgramManagerV2::undo() {
    return m_replView->undo();
}

bool ProgramManagerV2::redo() {
    return m_replView->redo();
}

bool ProgramManagerV2::canUndo() const {
    return m_replView->canUndo();
}

bool ProgramManagerV2::canRedo() const {
    return m_replView->canRedo();
}

void ProgramManagerV2::clearUndoHistory() {
    m_replView->clearUndoHistory();
}

// Private helper functions

void ProgramManagerV2::updateModifiedFlag() const {
    // Sync with document's dirty flag
    if (m_replView->isModified()) {
        m_modifiedFlag = true;
    }
}

} // namespace FasterBASIC