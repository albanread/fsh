//
// basic_syntax_highlighter.h
// FasterBASIC Shell - BASIC Syntax Highlighter
//
// Provides syntax highlighting for BASIC code in the screen editor
//

#ifndef BASIC_SYNTAX_HIGHLIGHTER_H
#define BASIC_SYNTAX_HIGHLIGHTER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <cctype>
#include "../runtime/terminal_io.h"

namespace FasterBASIC {

class BasicSyntaxHighlighter {
public:
    BasicSyntaxHighlighter();
    
    // Highlight a line of BASIC code
    // Returns a vector of colors, one per character
    std::vector<TerminalColor> highlightLine(const std::string& line) const;
    
    // Configure colors
    void setKeywordColor(TerminalColor color) { m_keywordColor = color; }
    void setStringColor(TerminalColor color) { m_stringColor = color; }
    void setNumberColor(TerminalColor color) { m_numberColor = color; }
    void setCommentColor(TerminalColor color) { m_commentColor = color; }
    void setOperatorColor(TerminalColor color) { m_operatorColor = color; }
    void setDefaultColor(TerminalColor color) { m_defaultColor = color; }
    void setLineNumberColor(TerminalColor color) { m_lineNumberColor = color; }
    
private:
    // Token types
    enum class TokenType {
        DEFAULT,
        KEYWORD,
        STRING,
        NUMBER,
        COMMENT,
        OPERATOR,
        LINE_NUMBER
    };
    
    // Check if word is a BASIC keyword
    bool isKeyword(const std::string& word) const;
    
    // Check if character is an operator
    bool isOperator(char ch) const;
    
    // Check if character can start an identifier
    bool isIdentifierStart(char ch) const;
    
    // Check if character can be part of an identifier
    bool isIdentifierPart(char ch) const;
    
    // Get color for token type
    TerminalColor getColorForTokenType(TokenType type) const;
    
    // Color settings
    TerminalColor m_keywordColor;
    TerminalColor m_stringColor;
    TerminalColor m_numberColor;
    TerminalColor m_commentColor;
    TerminalColor m_operatorColor;
    TerminalColor m_defaultColor;
    TerminalColor m_lineNumberColor;
    
    // BASIC keywords
    std::unordered_set<std::string> m_keywords;
    
    // Initialize keyword set
    void initializeKeywords();
};

} // namespace FasterBASIC

#endif // BASIC_SYNTAX_HIGHLIGHTER_H