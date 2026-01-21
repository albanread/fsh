//
// fasterbasic_lexer.cpp
// FasterBASIC - Lexer Implementation
//
// Tokenizes BASIC source code into a stream of tokens.
//

#include "fasterbasic_lexer.h"
#include "modular_commands.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace FasterBASIC {

// =============================================================================
// Keyword Table (Static Initialization)
// =============================================================================

std::map<std::string, TokenType> Lexer::s_keywords;
std::once_flag Lexer::s_keywordsInitFlag;

// Registry-based dynamic commands
std::map<std::string, TokenType> Lexer::s_dynamicCommands;

void Lexer::initializeKeywords() {
    std::call_once(s_keywordsInitFlag, []() {
        // Control Flow
        s_keywords["PRINT"] = TokenType::PRINT;
        s_keywords["CONSOLE"] = TokenType::CONSOLE;
        s_keywords["INPUT"] = TokenType::INPUT;
        s_keywords["LET"] = TokenType::LET;
        s_keywords["GOTO"] = TokenType::GOTO;
        s_keywords["GOSUB"] = TokenType::GOSUB;
        s_keywords["RETURN"] = TokenType::RETURN;
        s_keywords["IF"] = TokenType::IF;
        s_keywords["THEN"] = TokenType::THEN;
        s_keywords["ELSE"] = TokenType::ELSE;
        s_keywords["ELSEIF"] = TokenType::ELSEIF;
        s_keywords["ENDIF"] = TokenType::ENDIF;
        s_keywords["FOR"] = TokenType::FOR;
        s_keywords["TO"] = TokenType::TO;
        s_keywords["STEP"] = TokenType::STEP;
        s_keywords["IN"] = TokenType::IN;
        s_keywords["NEXT"] = TokenType::NEXT;
        s_keywords["WHILE"] = TokenType::WHILE;
        s_keywords["WEND"] = TokenType::WEND;
        s_keywords["ENDWHILE"] = TokenType::WEND;  // Alias for WEND
        s_keywords["REPEAT"] = TokenType::REPEAT;
        s_keywords["UNTIL"] = TokenType::UNTIL;
        s_keywords["DO"] = TokenType::DO;
        s_keywords["LOOP"] = TokenType::LOOP;
        s_keywords["DONE"] = TokenType::DONE;
        s_keywords["END"] = TokenType::END;
        s_keywords["EXIT"] = TokenType::EXIT;
        s_keywords["CASE"] = TokenType::CASE;
        s_keywords["SELECT"] = TokenType::SELECT;
        s_keywords["OF"] = TokenType::OF;
        s_keywords["WHEN"] = TokenType::WHEN;
        s_keywords["OTHERWISE"] = TokenType::OTHERWISE;
        s_keywords["ENDCASE"] = TokenType::ENDCASE;
    
        // Functions and Procedures
        s_keywords["SUB"] = TokenType::SUB;
        s_keywords["FUNCTION"] = TokenType::FUNCTION;
        s_keywords["ENDSUB"] = TokenType::ENDSUB;
        s_keywords["ENDFUNCTION"] = TokenType::ENDFUNCTION;
        s_keywords["CALL"] = TokenType::CALL;
        s_keywords["LOCAL"] = TokenType::LOCAL;
        s_keywords["SHARED"] = TokenType::SHARED;
        s_keywords["BYREF"] = TokenType::BYREF;
        s_keywords["BYVAL"] = TokenType::BYVAL;
        s_keywords["AS"] = TokenType::AS;
        s_keywords["DEF"] = TokenType::DEF;
        s_keywords["FN"] = TokenType::FN;
        s_keywords["IIF"] = TokenType::IIF;
        s_keywords["ON"] = TokenType::ON;
        s_keywords["ONEVENT"] = TokenType::ONEVENT;
    
        // Type names (for AS declarations)
        s_keywords["INTEGER"] = TokenType::KEYWORD_INTEGER;
        s_keywords["INT"] = TokenType::KEYWORD_INTEGER;  // Alias for INTEGER
        s_keywords["DOUBLE"] = TokenType::KEYWORD_DOUBLE;
        s_keywords["SINGLE"] = TokenType::KEYWORD_SINGLE;
        s_keywords["FLOAT"] = TokenType::KEYWORD_SINGLE;  // Alias for SINGLE
        s_keywords["STRING"] = TokenType::KEYWORD_STRING;
        s_keywords["LONG"] = TokenType::KEYWORD_LONG;
    
        // Data
        s_keywords["DIM"] = TokenType::DIM;
        s_keywords["REDIM"] = TokenType::REDIM;
        s_keywords["ERASE"] = TokenType::ERASE;
        s_keywords["PRESERVE"] = TokenType::PRESERVE;
        s_keywords["SWAP"] = TokenType::SWAP;
        s_keywords["INC"] = TokenType::INC;
        s_keywords["DEC"] = TokenType::DEC;
        s_keywords["DATA"] = TokenType::DATA;
        s_keywords["READ"] = TokenType::READ;
        s_keywords["RESTORE"] = TokenType::RESTORE;
        s_keywords["CONSTANT"] = TokenType::CONSTANT;
        s_keywords["TYPE"] = TokenType::TYPE;
        s_keywords["ENDTYPE"] = TokenType::ENDTYPE;
    
        // File I/O
        s_keywords["OPEN"] = TokenType::OPEN;
        s_keywords["CLOSE"] = TokenType::CLOSE;
        s_keywords["PRINT#"] = TokenType::PRINT_STREAM;
        s_keywords["INPUT#"] = TokenType::INPUT_STREAM;
        s_keywords["WRITE#"] = TokenType::WRITE_STREAM;
    
        // Other
        s_keywords["REM"] = TokenType::REM;
        s_keywords["CLS"] = TokenType::CLS;
        s_keywords["COLOR"] = TokenType::COLOR;
        s_keywords["WAIT"] = TokenType::WAIT;
        s_keywords["WAIT_MS"] = TokenType::WAIT_MS;
        s_keywords["WAIT_FRAME"] = TokenType::WAIT;  // Alias for WAIT (single frame)
        s_keywords["WAIT_FRAMES"] = TokenType::WAIT;  // Alias for WAIT (multiple frames)
        s_keywords["USING"] = TokenType::USING;
    
        // Graphics
        s_keywords["PSET"] = TokenType::PSET;
        s_keywords["LINE"] = TokenType::LINE;
        s_keywords["RECT"] = TokenType::RECT;

        s_keywords["CIRCLE"] = TokenType::CIRCLE;
        s_keywords["CIRCLEF"] = TokenType::CIRCLEF;
        s_keywords["GCLS"] = TokenType::GCLS;
        s_keywords["CLG"] = TokenType::CLG;
        s_keywords["HLINE"] = TokenType::HLINE;
    
        // Text Layer
        s_keywords["AT"] = TokenType::AT;
        s_keywords["LOCATE"] = TokenType::LOCATE;
        s_keywords["TEXTPUT"] = TokenType::TEXTPUT;
        s_keywords["TEXT_PUT"] = TokenType::TEXTPUT;  // Alias for TEXTPUT (underscore variant)
        s_keywords["PRINT_AT"] = TokenType::PRINT_AT;  // Special command with PRINT-style syntax
        s_keywords["INPUT_AT"] = TokenType::INPUT_AT;  // Special command with INPUT-style syntax
        s_keywords["TCHAR"] = TokenType::TCHAR;
        s_keywords["TEXT_PUTCHAR"] = TokenType::TCHAR;  // Alias for TCHAR (underscore variant)
        s_keywords["TGRID"] = TokenType::TGRID;
        s_keywords["TSCROLL"] = TokenType::TSCROLL;
        s_keywords["TEXT_SCROLL"] = TokenType::TSCROLL;  // Alias for TSCROLL (underscore variant)
        s_keywords["TCLEAR"] = TokenType::TCLEAR;
        s_keywords["TEXT_CLEAR"] = TokenType::TCLEAR;  // Alias for TCLEAR (underscore variant)
    
        // Sprites
        s_keywords["SPRLOAD"] = TokenType::SPRLOAD;
        s_keywords["SPRFREE"] = TokenType::SPRFREE;
        s_keywords["SPRSHOW"] = TokenType::SPRSHOW;
        s_keywords["SPRHIDE"] = TokenType::SPRHIDE;
        s_keywords["SPRMOVE"] = TokenType::SPRMOVE;
        s_keywords["SPRPOS"] = TokenType::SPRPOS;
        s_keywords["SPRTINT"] = TokenType::SPRTINT;
        s_keywords["SPRSCALE"] = TokenType::SPRSCALE;
        s_keywords["SPRROT"] = TokenType::SPRROT;
        s_keywords["SPREXPLODE"] = TokenType::SPREXPLODE;
    
    // Audio
    s_keywords["PLAY"] = TokenType::PLAY;
    s_keywords["PLAY_SOUND"] = TokenType::PLAY_SOUND;
    
// Timing
        s_keywords["VSYNC"] = TokenType::VSYNC;
        s_keywords["AFTER"] = TokenType::AFTER;
        s_keywords["EVERY"] = TokenType::EVERY;
        s_keywords["AFTERFRAMES"] = TokenType::AFTERFRAMES;
        s_keywords["EVERYFRAME"] = TokenType::EVERYFRAME;
        s_keywords["TIMER"] = TokenType::TIMER;
        s_keywords["STOP"] = TokenType::STOP;
        s_keywords["RUN"] = TokenType::RUN;
        s_keywords["MS"] = TokenType::MS;
        s_keywords["SECS"] = TokenType::SECS;
        s_keywords["FRAMES"] = TokenType::FRAMES;
        
        // Operators (word-based)
        s_keywords["MOD"] = TokenType::MOD;
        s_keywords["AND"] = TokenType::AND;
        s_keywords["OR"] = TokenType::OR;
        s_keywords["NOT"] = TokenType::NOT;
        s_keywords["XOR"] = TokenType::XOR;
        s_keywords["EQV"] = TokenType::EQV;
        s_keywords["IMP"] = TokenType::IMP;
        
        // Compiler directives
        s_keywords["OPTION"] = TokenType::OPTION;
        s_keywords["BITWISE"] = TokenType::BITWISE;
        s_keywords["LOGICAL"] = TokenType::LOGICAL;
        s_keywords["BASE"] = TokenType::BASE;
        s_keywords["EXPLICIT"] = TokenType::EXPLICIT;
        s_keywords["UNICODE"] = TokenType::UNICODE;
        s_keywords["ERROR"] = TokenType::ERROR;
        s_keywords["INCLUDE"] = TokenType::INCLUDE;
        s_keywords["ONCE"] = TokenType::ONCE;
        s_keywords["CANCELLABLE"] = TokenType::CANCELLABLE;
        s_keywords["FORCE_YIELD"] = TokenType::FORCE_YIELD;
        s_keywords["OFF"] = TokenType::OFF;
    });
}

void Lexer::initializeDynamicCommands() {
    // Clear any existing dynamic commands
    s_dynamicCommands.clear();
    
    // Initialize the global registry if not already done
    FasterBASIC::ModularCommands::initializeGlobalRegistry();
    
    // Get all registered commands and functions and create tokens for them
    auto& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    auto commandNames = registry.getCommandNames();
    auto functionNames = registry.getFunctionNames();

    
    for (const auto& commandName : commandNames) {
        // Skip PRINT_AT - it's now a special keyword with PRINT-style syntax
        if (commandName == "PRINT_AT") {
            continue;
        }
        // Skip INPUT_AT - it's now a special keyword with INPUT-style syntax
        if (commandName == "INPUT_AT") {
            continue;
        }
        s_dynamicCommands[commandName] = TokenType::REGISTRY_COMMAND;
    }
    
    for (const auto& functionName : functionNames) {
        s_dynamicCommands[functionName] = TokenType::REGISTRY_FUNCTION;
    }
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

Lexer::Lexer() 
    : m_position(0)
    , m_line(1)
    , m_column(1)
{
    initializeKeywords();
    initializeDynamicCommands();
}

Lexer::~Lexer() {
}

// =============================================================================
// Main Tokenization
// =============================================================================

bool Lexer::tokenize(const std::string& source) {
    clear();
    m_source = source;
    m_position = 0;
    m_line = 1;
    m_column = 1;
    
    while (!isAtEnd()) {
        Token token = scanToken();
        if (token.type != TokenType::UNKNOWN) {
            m_tokens.push_back(token);
        }
    }
    
    // Add EOF token
    m_tokens.push_back(Token(TokenType::END_OF_FILE, "", getCurrentLocation()));
    
    return !hasErrors();
}

void Lexer::clear() {
    m_source.clear();
    m_tokens.clear();
    m_errors.clear();
    m_position = 0;
    m_line = 1;
    m_column = 1;
}

// =============================================================================
// Token Scanning
// =============================================================================

Token Lexer::scanToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return Token(TokenType::END_OF_FILE, "", getCurrentLocation());
    }
    
    SourceLocation startLoc = getCurrentLocation();
    char c = currentChar();
    
    // Line terminator
    if (c == '\n' || c == '\r') {
        advance();
        if (c == '\r' && currentChar() == '\n') {
            advance();  // Handle \r\n
        }
        return Token(TokenType::END_OF_LINE, "", startLoc);
    }
    
    // Single-quote comment (like REM)
    if (c == '\'') {
        skipToEndOfLine();
        return Token(TokenType::END_OF_LINE, "", startLoc);
    }
    
    // Numbers (including line numbers)
    if (isDigit(c)) {
        return scanNumber();
    }
    
    // Hexadecimal numbers (&H prefix)
    if (c == '&' && (peekChar() == 'H' || peekChar() == 'h')) {
        return scanHexNumber();
    }
    
    // Strings
    if (c == '"') {
        return scanString();
    }
    
    // Identifiers and keywords
    if (isIdentifierStart(c)) {
        return scanIdentifierOrKeyword();
    }
    
    // Operators and delimiters
    return scanOperator();
}

Token Lexer::scanNumber() {
    SourceLocation startLoc = getCurrentLocation();
    std::string numStr;
    
    // Check for 0x hexadecimal prefix
    if (currentChar() == '0' && (peekChar() == 'x' || peekChar() == 'X')) {
        return scanHexNumberCStyle();
    }
    
    // Collect integer part
    while (isDigit(currentChar())) {
        numStr += advance();
    }
    
    // Check for decimal point
    if (currentChar() == '.' && isDigit(peekChar())) {
        numStr += advance();  // consume '.'
        while (isDigit(currentChar())) {
            numStr += advance();
        }
    }
    
    // Check for scientific notation (e.g., 1.5e10, 2E-5)
    if (currentChar() == 'e' || currentChar() == 'E') {
        numStr += advance();  // consume 'e' or 'E'
        
        if (currentChar() == '+' || currentChar() == '-') {
            numStr += advance();  // consume sign
        }
        
        if (!isDigit(currentChar())) {
            addError("Invalid number format: expected digits after exponent", startLoc);
            return Token(TokenType::UNKNOWN, numStr, startLoc);
        }
        
        while (isDigit(currentChar())) {
            numStr += advance();
        }
    }
    
    // Convert to double
    double value = 0.0;
    try {
        value = std::stod(numStr);
    } catch (...) {
        addError("Invalid number: " + numStr, startLoc);
        return Token(TokenType::UNKNOWN, numStr, startLoc);
    }
    
    return Token(TokenType::NUMBER, numStr, value, startLoc);
}

Token Lexer::scanHexNumber() {
    SourceLocation startLoc = getCurrentLocation();
    std::string hexStr;
    
    advance();  // consume '&'
    advance();  // consume 'H' or 'h'
    
    // Collect hex digits
    while (isHexDigit(currentChar())) {
        hexStr += advance();
    }
    
    if (hexStr.empty()) {
        addError("Invalid hexadecimal number: expected hex digits after &H", startLoc);
        return Token(TokenType::UNKNOWN, "&H", startLoc);
    }
    
    // Convert hex string to double
    double value = 0.0;
    try {
        // Parse as unsigned long long first, then convert to double
        unsigned long long ullValue = std::stoull(hexStr, nullptr, 16);
        value = static_cast<double>(ullValue);
    } catch (...) {
        addError("Invalid hexadecimal number: " + hexStr, startLoc);
        return Token(TokenType::UNKNOWN, "&H" + hexStr, startLoc);
    }
    
    return Token(TokenType::NUMBER, "&H" + hexStr, value, startLoc);
}

Token Lexer::scanString() {
    SourceLocation startLoc = getCurrentLocation();
    std::string str;
    bool hasNonASCII = false;
    
    advance();  // consume opening "
    
    while (!isAtEnd() && currentChar() != '"' && currentChar() != '\n') {
        char c = currentChar();
        str += advance();
        
        // Check if this byte is non-ASCII (high bit set)
        // In UTF-8, any byte with value >= 128 (0x80) is part of a multi-byte sequence
        if (static_cast<unsigned char>(c) >= 128) {
            hasNonASCII = true;
        }
    }
    
    if (currentChar() != '"') {
        addError("Unterminated string", startLoc);
        return Token(TokenType::UNKNOWN, str, startLoc);
    }
    
    advance();  // consume closing "
    
    return Token(TokenType::STRING, str, startLoc, hasNonASCII);
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation startLoc = getCurrentLocation();
    std::string text;
    
    // Collect identifier characters
    while (isIdentifierChar(currentChar())) {
        text += advance();
    }
    
    // Check for type suffix (%, !, #, $)
    char suffix = currentChar();
    if (suffix == '%' || suffix == '!' || suffix == '#' || suffix == '$') {
        text += advance();
    }
    
    // Convert to uppercase for keyword matching
    std::string upperText = text;
    for (char& c : upperText) {
        c = std::toupper(c);
    }
    
    // Check if it's a keyword
    TokenType keywordType = getKeywordType(upperText);
    if (keywordType != TokenType::UNKNOWN) {
        // Special handling for REM - skip rest of line
        if (keywordType == TokenType::REM) {
            skipToEndOfLine();
        }
        return Token(keywordType, text, startLoc);
    }
    
    // It's an identifier
    return Token(TokenType::IDENTIFIER, text, startLoc);
}

Token Lexer::scanOperator() {
    SourceLocation startLoc = getCurrentLocation();
    char c = advance();
    
    switch (c) {
        // Single-character operators
        case '+': return Token(TokenType::PLUS, "+", startLoc);
        case '-': return Token(TokenType::MINUS, "-", startLoc);
        case '*': return Token(TokenType::MULTIPLY, "*", startLoc);
        case '/': return Token(TokenType::DIVIDE, "/", startLoc);
        case '\\': return Token(TokenType::INT_DIVIDE, "\\", startLoc);
        case '^': return Token(TokenType::POWER, "^", startLoc);
        case '(': return Token(TokenType::LPAREN, "(", startLoc);
        case ')': return Token(TokenType::RPAREN, ")", startLoc);
        case ',': return Token(TokenType::COMMA, ",", startLoc);
        case ';': return Token(TokenType::SEMICOLON, ";", startLoc);
        case ':': return Token(TokenType::COLON, ":", startLoc);
        case '?': return Token(TokenType::QUESTION, "?", startLoc);
        case '#': return Token(TokenType::HASH, "#", startLoc);
        case '.': return Token(TokenType::DOT, ".", startLoc);
        
        // Comparison operators
        case '=':
            return Token(TokenType::EQUAL, "=", startLoc);
        
        case '<':
            if (match('>')) {
                return Token(TokenType::NOT_EQUAL, "<>", startLoc);
            } else if (match('=')) {
                return Token(TokenType::LESS_EQUAL, "<=", startLoc);
            }
            return Token(TokenType::LESS_THAN, "<", startLoc);
        
        case '>':
            if (match('=')) {
                return Token(TokenType::GREATER_EQUAL, ">=", startLoc);
            }
            return Token(TokenType::GREATER_THAN, ">", startLoc);
        
        case '!':
            if (match('=')) {
                return Token(TokenType::NOT_EQUAL, "!=", startLoc);
            }
            // ! by itself is a type suffix, but we handle it in identifiers
            // If we get here, it's probably an error or part of something else
            addError(std::string("Unexpected character: ") + c, startLoc);
            return Token(TokenType::UNKNOWN, std::string(1, c), startLoc);
        
        case '&':
            // & by itself is invalid (we handle &H in scanToken)
            addError(std::string("Unexpected character: ") + c + " (use &H for hex numbers)", startLoc);
            return Token(TokenType::UNKNOWN, std::string(1, c), startLoc);
        
        default:
            // Unknown character
            addError(std::string("Unexpected character: ") + c, startLoc);
            return Token(TokenType::UNKNOWN, std::string(1, c), startLoc);
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = currentChar();
        if (c == ' ' || c == '\t') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skipToEndOfLine() {
    while (!isAtEnd() && currentChar() != '\n' && currentChar() != '\r') {
        advance();
    }
}

TokenType Lexer::getKeywordType(const std::string& text) const {
    // First check static keywords
    auto it = s_keywords.find(text);
    if (it != s_keywords.end()) {
        return it->second;
    }
    
    // Then check dynamic registry commands
    auto dynIt = s_dynamicCommands.find(text);
    if (dynIt != s_dynamicCommands.end()) {
        return dynIt->second;
    }
    
    return TokenType::UNKNOWN;
}

bool Lexer::isKeyword(const std::string& text) const {
    return s_keywords.find(text) != s_keywords.end() || 
           s_dynamicCommands.find(text) != s_dynamicCommands.end();
}

// =============================================================================
// Output / Printing
// =============================================================================

void Lexer::printTokens(std::ostream& os) const {
    os << "Token Stream (" << m_tokens.size() << " tokens):\n";
    os << "----------------------------------------\n";
    
    for (const auto& token : m_tokens) {
        os << token.toString() << " ";
    }
    os << "\n";
}

void Lexer::printTokensDetailed(std::ostream& os) const {
    os << "Token Stream (Detailed)\n";
    os << "========================================\n";
    os << "Total tokens: " << m_tokens.size() << "\n";
    os << "----------------------------------------\n";
    
    int lineNum = -1;
    for (const auto& token : m_tokens) {
        // Print line separator for new lines
        if (token.location.line != lineNum) {
            if (lineNum != -1) {
                os << "\n";
            }
            lineNum = token.location.line;
            os << "Line " << std::setw(4) << lineNum << ": ";
        }
        
        os << std::setw(15) << std::left << tokenTypeToString(token.type);
        
        if (token.type == TokenType::NUMBER) {
            os << " = " << token.numberValue;
        } else if (token.type == TokenType::STRING) {
            os << " = \"" << token.value << "\"";
        } else if (token.type == TokenType::IDENTIFIER) {
            os << " = " << token.value;
        } else if (!token.value.empty() && token.value != tokenTypeToString(token.type)) {
            os << " = " << token.value;
        }
        
        os << "\n";
        
        if (token.type == TokenType::END_OF_LINE) {
            // Already handled by line change detection
        }
    }
    
    os << "----------------------------------------\n";
    
    if (hasErrors()) {
        os << "\nErrors (" << m_errors.size() << "):\n";
        os << "----------------------------------------\n";
        for (const auto& error : m_errors) {
            os << error.toString() << "\n";
        }
    }
}

Token Lexer::scanHexNumberCStyle() {
    SourceLocation startLoc = getCurrentLocation();
    std::string hexStr;
    
    advance();  // consume '0'
    advance();  // consume 'x' or 'X'
    
    // Collect hex digits
    while (isHexDigit(currentChar())) {
        hexStr += advance();
    }
    
    if (hexStr.empty()) {
        addError("Invalid hexadecimal number: expected hex digits after 0x", startLoc);
        return Token(TokenType::UNKNOWN, "0x", startLoc);
    }
    
    // Convert hex string to double
    double value = 0.0;
    try {
        // Parse as unsigned long long first, then convert to double
        unsigned long long ullValue = std::stoull(hexStr, nullptr, 16);
        value = static_cast<double>(ullValue);
    } catch (...) {
        addError("Invalid hexadecimal number: " + hexStr, startLoc);
        return Token(TokenType::UNKNOWN, "0x" + hexStr, startLoc);
    }
    
    return Token(TokenType::NUMBER, "0x" + hexStr, value, startLoc);
}

} // namespace FasterBASIC