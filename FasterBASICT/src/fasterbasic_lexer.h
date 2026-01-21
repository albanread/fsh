//
// fasterbasic_lexer.h
// FasterBASIC - Lexer (Tokenizer)
//
// Converts BASIC source code into a stream of tokens.
// Handles line numbers, keywords, identifiers, literals, operators, etc.
//

#ifndef FASTERBASIC_LEXER_H
#define FASTERBASIC_LEXER_H

#include "fasterbasic_token.h"
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <memory>
#include <mutex>

namespace FasterBASIC {

// =============================================================================
// Lexer Error
// =============================================================================

struct LexerError {
    std::string message;
    SourceLocation location;
    
    LexerError(const std::string& msg, const SourceLocation& loc)
        : message(msg), location(loc) {}
    
    std::string toString() const {
        return "Lexer Error at " + location.toString() + ": " + message;
    }
};

// =============================================================================
// Lexer
// =============================================================================

class Lexer {
public:
    Lexer();
    ~Lexer();
    
    // Tokenize source code
    bool tokenize(const std::string& source);
    
    // Get tokenized results
    const std::vector<Token>& getTokens() const { return m_tokens; }
    const std::vector<LexerError>& getErrors() const { return m_errors; }
    
    // Check if tokenization was successful
    bool hasErrors() const { return !m_errors.empty(); }
    
    // Print tokens for debugging
    void printTokens(std::ostream& os) const;
    void printTokensDetailed(std::ostream& os) const;
    
    // Clear state
    void clear();
    
private:
    // Source code state
    std::string m_source;
    size_t m_position;
    int m_line;
    int m_column;
    
    // Results
    std::vector<Token> m_tokens;
    std::vector<LexerError> m_errors;
    
    // Keyword lookup table
    static std::map<std::string, TokenType> s_keywords;
    static std::once_flag s_keywordsInitFlag;
    static void initializeKeywords();
    
    // Registry-based dynamic commands
    static std::map<std::string, TokenType> s_dynamicCommands;
    static void initializeDynamicCommands();
    
    // Character inspection
    char currentChar() const;
    char peekChar(int offset = 1) const;
    bool isAtEnd() const;
    
    // Character consumption
    char advance();
    bool match(char expected);
    void skipWhitespace();
    void skipToEndOfLine();
    
    // Source location
    SourceLocation getCurrentLocation() const;
    
    // Token creation
    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& value);
    void addToken(TokenType type, const std::string& value, double numberValue);
    
    // Error reporting
    void addError(const std::string& message);
    void addError(const std::string& message, const SourceLocation& loc);
    
    // Token recognition
    Token scanToken();
    Token scanNumber();
    Token scanHexNumber();
    Token scanHexNumberCStyle();
    Token scanString();
    Token scanIdentifierOrKeyword();
    Token scanOperator();
    
    // Helper functions
    bool isDigit(char c) const;
    bool isHexDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    bool isIdentifierStart(char c) const;
    bool isIdentifierChar(char c) const;
    
    // Keyword recognition
    TokenType getKeywordType(const std::string& text) const;
    bool isKeyword(const std::string& text) const;
};

// =============================================================================
// Lexer Implementation (inline methods)
// =============================================================================

inline char Lexer::currentChar() const {
    if (isAtEnd()) return '\0';
    return m_source[m_position];
}

inline char Lexer::peekChar(int offset) const {
    size_t pos = m_position + offset;
    if (pos >= m_source.length()) return '\0';
    return m_source[pos];
}

inline bool Lexer::isAtEnd() const {
    return m_position >= m_source.length();
}

inline char Lexer::advance() {
    if (isAtEnd()) return '\0';
    
    char c = m_source[m_position++];
    if (c == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    return c;
}

inline bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (currentChar() != expected) return false;
    advance();
    return true;
}

inline SourceLocation Lexer::getCurrentLocation() const {
    return SourceLocation(m_line, m_column);
}

inline void Lexer::addToken(TokenType type) {
    m_tokens.push_back(Token(type, "", getCurrentLocation()));
}

inline void Lexer::addToken(TokenType type, const std::string& value) {
    m_tokens.push_back(Token(type, value, getCurrentLocation()));
}

inline void Lexer::addToken(TokenType type, const std::string& value, double numberValue) {
    m_tokens.push_back(Token(type, value, numberValue, getCurrentLocation()));
}

inline void Lexer::addError(const std::string& message) {
    m_errors.push_back(LexerError(message, getCurrentLocation()));
}

inline void Lexer::addError(const std::string& message, const SourceLocation& loc) {
    m_errors.push_back(LexerError(message, loc));
}

inline bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

inline bool Lexer::isHexDigit(char c) const {
    return (c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

inline bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

inline bool Lexer::isIdentifierStart(char c) const {
    return isAlpha(c) || c == '_';
}

inline bool Lexer::isIdentifierChar(char c) const {
    return isAlphaNumeric(c) || c == '_';
}

} // namespace FasterBASIC

#endif // FASTERBASIC_LEXER_H