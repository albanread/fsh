//
// fasterbasic_parser.h
// FasterBASIC - Parser
//
// Recursive descent parser that converts tokens into an Abstract Syntax Tree (AST).
// Implements operator precedence and handles all BASIC language constructs.
//

#ifndef FASTERBASIC_PARSER_H
#define FASTERBASIC_PARSER_H

#include "fasterbasic_ast.h"
#include "fasterbasic_lexer.h"
#include "fasterbasic_options.h"
#include "../runtime/ConstantsManager.h"

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <map>
#include <set>
namespace FasterBASIC {

// =============================================================================
// LineNumberMapping - Tracks BASIC line numbers to physical line mapping
// =============================================================================

struct LineNumberMapping {
    std::map<size_t, int> physicalToBasic;  // Physical line index -> BASIC line number
    std::map<int, size_t> basicToPhysical;  // BASIC line number -> Physical line index
    
    void addMapping(size_t physicalLine, int basicLineNumber) {
        physicalToBasic[physicalLine] = basicLineNumber;
        basicToPhysical[basicLineNumber] = physicalLine;
    }
    
    // Returns pointer to BASIC line number if found, nullptr otherwise
    const int* getBasicLineNumber(size_t physicalLine) const {
        auto it = physicalToBasic.find(physicalLine);
        if (it != physicalToBasic.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // Returns pointer to physical line if found, nullptr otherwise
    const size_t* getPhysicalLine(int basicLineNumber) const {
        auto it = basicToPhysical.find(basicLineNumber);
        if (it != basicToPhysical.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    void clear() {
        physicalToBasic.clear();
        basicToPhysical.clear();
    }
};

// =============================================================================
// Parser Error
// =============================================================================

class ParserError : public std::runtime_error {
public:
    SourceLocation location;
    
    ParserError(const std::string& msg, const SourceLocation& loc)
        : std::runtime_error(msg), location(loc) {}
    
    std::string toString() const {
        return "Parser Error at " + location.toString() + ": " + what();
    }
};

// =============================================================================
// Parser
// =============================================================================

class Parser {
public:
    Parser();
    ~Parser();
    
    // Parse tokens into AST
    // Returns the AST and sets the compiler options from OPTION statements
    std::unique_ptr<Program> parse(const std::vector<Token>& tokens, const std::string& sourceFile = "");
    
    // Get line number mapping (for error reporting)
    const LineNumberMapping& getLineNumberMapping() const { return m_lineNumberMapping; }
    
    // Set include search paths (for -I command line option)
    void setIncludePaths(const std::vector<std::string>& paths) { m_includePaths = paths; }
    
    // Get compiler options collected from OPTION statements
    const CompilerOptions& getOptions() const { return m_options; }
    
    // Get parse errors (for continuing after errors)
    const std::vector<ParserError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    
    // Get collected comments (mapped by line number)
    const std::map<int, std::string>& getComments() const { return m_comments; }
    
    // Configuration
    void setStrictMode(bool strict) { m_strictMode = strict; }
    void setAllowImplicitLet(bool allow) { m_allowImplicitLet = allow; }
    void setConstantsManager(ConstantsManager* manager) { m_constantsManager = manager; }
    
private:
    // Token stream management
    const std::vector<Token>* m_tokens;
    size_t m_currentIndex;
    std::vector<ParserError> m_errors;
    
    // Constants manager (for fast constant lookup)
    ConstantsManager* m_constantsManager;
    
    // Line number preprocessing
    LineNumberMapping m_lineNumberMapping;  // Maps physical lines to BASIC line numbers
    
    // Include file handling
    struct IncludeContext {
        std::string filename;
        std::string fullPath;
        SourceLocation includeLocation;
    };
    
    std::vector<Token> m_expandedTokens;            // Storage for expanded token stream
    std::vector<IncludeContext> m_includeStack;     // Current include nesting (for error reporting)
    std::set<std::string> m_includedFiles;          // Files already included (for circular detection)
    std::set<std::string> m_onceFiles;              // Files marked with OPTION ONCE
    std::string m_currentSourceFile;                // Current file being parsed
    std::vector<std::string> m_includePaths;        // Search paths for includes (-I option)
    
    // Compiler options from OPTION statements
    CompilerOptions m_options;
    
    // Parser configuration
    bool m_strictMode;        // Strict syntax checking
    bool m_allowImplicitLet;  // Allow "X = 5" without LET keyword
    
    // Parser context state (for handling ambiguous keywords)
    bool m_inSelectCase;      // Inside SELECT CASE block (CASE is a clause, not a statement)
    
    // User-defined function/sub tracking (collected in prescan pass)
    std::set<std::string> m_userDefinedFunctions;  // Names of user-defined FUNCTIONs
    std::set<std::string> m_userDefinedSubs;       // Names of user-defined SUBs
    
    // Loop nesting tracking (for detecting mismatched loop keywords)
    enum class LoopType {
        WHILE_WEND,
        REPEAT_UNTIL,
        DO_LOOP
    };
    std::vector<std::pair<LoopType, SourceLocation>> m_loopStack;  // Stack of active loops with their locations
    
    // Auto line numbering (for programs without explicit line numbers)
    int m_autoLineNumber;     // Current auto-assigned line number
    int m_autoLineStart;      // Starting line number (default: 1000)
    int m_autoLineIncrement;  // Increment between lines (default: 10)
    
    // Inline handler generation (for DO...DONE blocks)
    int m_inlineHandlerCounter;  // Counter for generating unique handler names
    
    // Comment storage (collected during parsing, emitted during code generation)
    std::map<int, std::string> m_comments;  // Map of line number -> comment text
    int m_currentLineNumber;                 // Current line being parsed (for comment collection)
    
    // Prescan for forward references
    void prescanForFunctions();
    
    // Preprocessing - strip line numbers before parsing
    void preprocessLineNumbers(std::vector<Token>& tokens);
    
    // Include processing
    void expandIncludes(const std::vector<Token>& tokens);
    bool expandIncludesFromFile(const std::string& fullPath, const SourceLocation& includeLoc);
    std::string resolveIncludePath(const std::string& filename);
    std::string getCanonicalPath(const std::string& path);
    std::string getDirectoryPart(const std::string& path);
    bool fileExists(const std::string& path);
    
    // Current token access
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    bool isAtEnd() const;
    
    // Token consumption
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(const std::vector<TokenType>& types);
    const Token& consume(TokenType type, const std::string& errorMsg);
    
    // Skip to end of line (for error recovery)
    void synchronize();
    void skipToEndOfLine();
    void skipBlankLines();  // Skip consecutive blank lines and line numbers on blank lines
    void skipOptionalLineNumber();  // Skip line number at start of statement (for multi-line blocks)
    
    // Type suffix helpers
    TokenType parseAsType();  // Parse AS INTEGER/DOUBLE/STRING/etc.
    bool isTypeKeyword(TokenType type) const;
    TokenType asTypeToSuffix(TokenType asType) const;  // Convert AS type to suffix
    TokenType mergeTypes(TokenType suffix, TokenType asType, const std::string& varName);  // Validate and merge types
    
    // Error reporting
    void error(const std::string& message);
    void error(const std::string& message, const SourceLocation& loc);
    
    // Top-level parsing
    std::unique_ptr<Program> parseProgram();
    std::unique_ptr<ProgramLine> parseProgramLine(size_t physicalLine);
    
    // Statement parsing
    StatementPtr parseStatement();
    StatementPtr parsePrintStatement();
    StatementPtr parseConsoleStatement();
    StatementPtr parseInputStatement();
    StatementPtr parseLetStatement();
    StatementPtr parseGotoStatement();
    StatementPtr parseGosubStatement();
    StatementPtr parseOnStatement();
    StatementPtr parseOnEventStatement();
    StatementPtr parseConstantStatement();
    StatementPtr parseReturnStatement();
    StatementPtr parseExitStatement();
    StatementPtr parseIfStatement();
    StatementPtr parseCaseStatement();
    StatementPtr parseSelectCaseStatement();
    StatementPtr parseForStatement();
    StatementPtr parseNextStatement();
    StatementPtr parseWhileStatement();
    StatementPtr parseWendStatement();
    StatementPtr parseRepeatStatement();
    StatementPtr parseUntilStatement();
    StatementPtr parseDoStatement();
    StatementPtr parseLoopStatement();
    StatementPtr parseEndStatement();
    StatementPtr parseDimStatement();
    StatementPtr parseRedimStatement();
    StatementPtr parseEraseStatement();
    StatementPtr parseSwapStatement();
    StatementPtr parseIncStatement();
    StatementPtr parseDecStatement();
    StatementPtr parseTypeDeclarationStatement();
    StatementPtr parseLocalStatement();
    StatementPtr parseSharedStatement();
    StatementPtr parseDataStatement();
    StatementPtr parseReadStatement();
    StatementPtr parseRestoreStatement();
    StatementPtr parseRemStatement();
    StatementPtr parseOpenStatement();
    StatementPtr parseCloseStatement();
    StatementPtr parsePrintStreamStatement();
    StatementPtr parseInputStreamStatement();
    StatementPtr parseLineInputStreamStatement();
    StatementPtr parseWriteStreamStatement();
    StatementPtr parseOptionStatement();
    
    // Collect compiler options from OPTION statements before main parsing
    void collectOptionsFromTokens();
    
    // Validate string literals based on Unicode mode (called after collectOptionsFromTokens)
    void validateStringLiterals();
    
    StatementPtr parseDefStatement();
    StatementPtr parseFunctionStatement();
    StatementPtr parseSubStatement();
    StatementPtr parseCallStatement();
    
    // Graphics and sound statements
    StatementPtr parseClsStatement();
    StatementPtr parseGclsStatement();
    StatementPtr parseColorStatement();
    StatementPtr parseWaitStatement();
    StatementPtr parseWaitMsStatement();
    StatementPtr parsePlayStatement();
    StatementPtr parsePlaySoundStatement();
    StatementPtr parsePsetStatement();
    StatementPtr parseLineStatement();
    StatementPtr parseRectStatement();
    StatementPtr parseRectfStatement();
    StatementPtr parseCircleStatement();
    StatementPtr parseCirclefStatement();
    
    // SuperTerminal API statements - Graphics
    StatementPtr parseClgStatement();
    StatementPtr parseHlineStatement();
    StatementPtr parseVlineStatement();
    
    // SuperTerminal API statements - Text Layer
    StatementPtr parseAtStatement();
    StatementPtr parseTextputStatement();
    StatementPtr parsePrintAtStatement();
    StatementPtr parseInputAtStatement();
    StatementPtr parseRegistryCommandStatement();
    StatementPtr parseTcharStatement();
    StatementPtr parseTgridStatement();
    StatementPtr parseTscrollStatement();
    StatementPtr parseTclearStatement();
    
    // SuperTerminal API statements - Sprites
    StatementPtr parseSprloadStatement();
    StatementPtr parseSprfreeStatement();
    StatementPtr parseSprshowStatement();
    StatementPtr parsSprhideStatement();
    StatementPtr parseSprmoveStatement();
    StatementPtr parseSprposStatement();
    StatementPtr parseSprtintStatement();
    StatementPtr parseSprscaleStatement();
    StatementPtr parseSprrotStatement();
    StatementPtr parseSprexplodeStatement();
    
    // SuperTerminal API statements - Audio
    // (Audio commands now handled via modular registry)
    
    // SuperTerminal API statements - Timing
    StatementPtr parseVsyncStatement();
    
    // Timer event statements
    StatementPtr parseAfterStatement();
    StatementPtr parseEveryStatement();
    StatementPtr parseAfterFramesStatement();
    StatementPtr parseEveryFrameStatement();
    StatementPtr parseRunStatement();
    StatementPtr parseTimerStatement();
    
    // Expression parsing (with operator precedence)
    ExpressionPtr parseExpression();
    ExpressionPtr parseLogicalImp();
    ExpressionPtr parseLogicalEqv();
    ExpressionPtr parseLogicalOr();
    ExpressionPtr parseLogicalXor();
    ExpressionPtr parseLogicalAnd();
    ExpressionPtr parseLogicalNot();
    ExpressionPtr parseComparison();
    ExpressionPtr parseAdditive();
    ExpressionPtr parseMultiplicative();
    ExpressionPtr parseUnary();
    ExpressionPtr parsePower();
    ExpressionPtr parsePostfix();
    ExpressionPtr parsePrimary();
    ExpressionPtr parseRegistryFunctionExpression();
    
    // Helper functions
    bool isStartOfExpression() const;
    bool isStartOfStatement() const;
    bool isAssignment() const;
    TokenType peekTypeSuffix() const;
    std::string parseVariableName(TokenType& outSuffix);
    int parseLineNumber();
    
    // Parse multiple comma-separated expressions
    std::vector<ExpressionPtr> parseExpressionList();
};

} // namespace FasterBASIC

#endif // FASTERBASIC_PARSER_H