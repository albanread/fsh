//
// basic_syntax_highlighter.cpp
// FasterBASIC Shell - BASIC Syntax Highlighter Implementation
//
// Provides syntax highlighting for BASIC code in the screen editor
//

#include "basic_syntax_highlighter.h"
#include <algorithm>
#include <sstream>

namespace FasterBASIC {

BasicSyntaxHighlighter::BasicSyntaxHighlighter()
    : m_keywordColor(TerminalColor::BRIGHT_CYAN)
    , m_stringColor(TerminalColor::BRIGHT_YELLOW)
    , m_numberColor(TerminalColor::BRIGHT_GREEN)
    , m_commentColor(TerminalColor::BRIGHT_BLACK)
    , m_operatorColor(TerminalColor::BRIGHT_MAGENTA)
    , m_defaultColor(TerminalColor::WHITE)
    , m_lineNumberColor(TerminalColor::CYAN)
{
    initializeKeywords();
}

void BasicSyntaxHighlighter::initializeKeywords() {
    // Core BASIC keywords
    m_keywords = {
        // Control flow
        "IF", "THEN", "ELSE", "ELSEIF", "ENDIF",
        "FOR", "TO", "STEP", "NEXT",
        "WHILE", "WEND", "REPEAT", "DO", "LOOP", "UNTIL", "DONE",
        "GOTO", "GOSUB", "RETURN",
        "SELECT", "CASE", "DEFAULT", "ENDSELECT",
        "EXIT", "CONTINUE",
        
        // I/O
        "PRINT", "INPUT", "READ", "DATA", "RESTORE",
        "OPEN", "CLOSE", "GET", "PUT",
        "BGET", "BPUT", "EOF",
        "CLS", "LOCATE", "COLOR",
        
        // Variables and types
        "DIM", "REDIM", "LET", "CONST",
        "TYPE", "ENDTYPE", "AS",
        "LOCAL", "SHARED", "STATIC",
        
        // Functions and subs
        "SUB", "ENDSUB", "FUNCTION", "ENDFUNCTION",
        "DEF", "FN",
        "CALL", "BYREF", "BYVAL",
        
        // String functions
        "LEN", "LEFT", "RIGHT", "MID", "CHR", "ASC",
        "INSTR", "LCASE", "UCASE", "LTRIM", "RTRIM", "TRIM",
        "STR", "VAL", "SPACE", "STRING",
        
        // Math functions
        "ABS", "SIN", "COS", "TAN", "ATN", "SQR", "EXP", "LOG",
        "INT", "FIX", "SGN", "RND", "RANDOMIZE",
        
        // Logic operators
        "AND", "OR", "NOT", "XOR", "MOD",
        
        // Options
        "OPTION", "BASE", "EXPLICIT", "UNICODE", "BITWISE", "LOGICAL",
        
        // Other
        "REM", "END", "STOP", "ON", "ERROR",
        "SWAP", "INC", "DEC", "CLEAR", "NEW",
        "LOAD", "SAVE", "RUN", "LIST",
        
        // Graphics/Game commands
        "SPRITE", "VSPRITE", "COLLISION",
        "PSET", "LINE", "CIRCLE", "BOX", "FLOOD",
        "SCREEN", "VMODE", "PALETTE",
        "WAIT", "EVERY", "AFTER", "TIMER",
        
        // Plugin-related
        "DATETIME", "JSON", "CSV", "INI", "RECORDS",
        "LOAD", "CREATE", "PARSE", "GET", "SET",
        
        // Boolean literals
        "TRUE", "FALSE"
    };
}

bool BasicSyntaxHighlighter::isKeyword(const std::string& word) const {
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return m_keywords.find(upper) != m_keywords.end();
}

bool BasicSyntaxHighlighter::isOperator(char ch) const {
    return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^' ||
           ch == '=' || ch == '<' || ch == '>' || ch == '(' || ch == ')' ||
           ch == ',' || ch == ';' || ch == ':' || ch == '.' || ch == '&' ||
           ch == '|' || ch == '!' || ch == '[' || ch == ']';
}

bool BasicSyntaxHighlighter::isIdentifierStart(char ch) const {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool BasicSyntaxHighlighter::isIdentifierPart(char ch) const {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || 
           ch == '$' || ch == '%' || ch == '#' || ch == '!';
}

TerminalColor BasicSyntaxHighlighter::getColorForTokenType(TokenType type) const {
    switch (type) {
        case TokenType::KEYWORD: return m_keywordColor;
        case TokenType::STRING: return m_stringColor;
        case TokenType::NUMBER: return m_numberColor;
        case TokenType::COMMENT: return m_commentColor;
        case TokenType::OPERATOR: return m_operatorColor;
        case TokenType::LINE_NUMBER: return m_lineNumberColor;
        case TokenType::DEFAULT:
        default:
            return m_defaultColor;
    }
}

std::vector<TerminalColor> BasicSyntaxHighlighter::highlightLine(const std::string& line) const {
    std::vector<TerminalColor> colors(line.length(), m_defaultColor);
    
    if (line.empty()) {
        return colors;
    }
    
    size_t i = 0;
    
    // Check for line number at start
    if (std::isdigit(static_cast<unsigned char>(line[0]))) {
        while (i < line.length() && std::isdigit(static_cast<unsigned char>(line[i]))) {
            colors[i] = m_lineNumberColor;
            i++;
        }
        
        // Skip whitespace after line number
        while (i < line.length() && std::isspace(static_cast<unsigned char>(line[i]))) {
            i++;
        }
    }
    
    // Parse rest of line
    while (i < line.length()) {
        char ch = line[i];
        
        // Comments (REM or ')
        if (ch == '\'' || (i + 2 < line.length() && 
            std::toupper(line[i]) == 'R' && 
            std::toupper(line[i+1]) == 'E' && 
            std::toupper(line[i+2]) == 'M' &&
            (i + 3 >= line.length() || std::isspace(static_cast<unsigned char>(line[i+3]))))) {
            // Rest of line is a comment
            for (size_t j = i; j < line.length(); j++) {
                colors[j] = m_commentColor;
            }
            break;
        }
        
        // String literals
        if (ch == '"') {
            colors[i] = m_stringColor;
            i++;
            // Find closing quote
            while (i < line.length() && line[i] != '"') {
                colors[i] = m_stringColor;
                i++;
            }
            if (i < line.length() && line[i] == '"') {
                colors[i] = m_stringColor;
                i++;
            }
            continue;
        }
        
        // Numbers
        if (std::isdigit(static_cast<unsigned char>(ch)) || 
            (ch == '.' && i + 1 < line.length() && std::isdigit(static_cast<unsigned char>(line[i+1])))) {
            while (i < line.length() && 
                   (std::isdigit(static_cast<unsigned char>(line[i])) || 
                    line[i] == '.' || 
                    std::toupper(line[i]) == 'E' ||
                    (line[i] == '-' && i > 0 && std::toupper(line[i-1]) == 'E'))) {
                colors[i] = m_numberColor;
                i++;
            }
            continue;
        }
        
        // Keywords and identifiers
        if (isIdentifierStart(ch)) {
            size_t start = i;
            std::string word;
            
            while (i < line.length() && isIdentifierPart(line[i])) {
                word += line[i];
                i++;
            }
            
            // Check if it's a keyword
            TerminalColor wordColor = isKeyword(word) ? m_keywordColor : m_defaultColor;
            for (size_t j = start; j < i; j++) {
                colors[j] = wordColor;
            }
            continue;
        }
        
        // Operators
        if (isOperator(ch)) {
            colors[i] = m_operatorColor;
            i++;
            continue;
        }
        
        // Default (whitespace, etc.)
        i++;
    }
    
    return colors;
}

} // namespace FasterBASIC