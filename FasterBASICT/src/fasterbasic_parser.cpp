//
// fasterbasic_parser.cpp
// FasterBASIC - Parser Implementation
//
// Implements recursive descent parser with operator precedence for BASIC language.
// Converts token stream into Abstract Syntax Tree (AST).
//

#include "fasterbasic_parser.h"
#include "fasterbasic_lexer.h"
#include "modular_commands.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <set>
#include <climits>

// For realpath()
#ifndef _WIN32
#include <limits.h>
#else
#include <stdlib.h>
#endif

namespace FasterBASIC {

// =============================================================================
// Constructor/Destructor
// =============================================================================

Parser::Parser()
    : m_tokens(nullptr)
    , m_currentIndex(0)
    , m_constantsManager(nullptr)
    , m_strictMode(false)
    , m_allowImplicitLet(true)
    , m_inSelectCase(false)
    , m_autoLineNumber(1000)
    , m_autoLineStart(1000)
    , m_autoLineIncrement(10)
    , m_inlineHandlerCounter(0)
    , m_currentLineNumber(0)
{
}

Parser::~Parser() = default;

// =============================================================================
// Token Stream Management
// =============================================================================

const Token& Parser::current() const {
    if (m_currentIndex >= m_tokens->size()) {
        static Token eofToken(TokenType::END_OF_FILE, "", SourceLocation(0, 0));
        return eofToken;
    }
    return (*m_tokens)[m_currentIndex];
}

const Token& Parser::peek(int offset) const {
    size_t index = m_currentIndex + offset;
    if (index >= m_tokens->size()) {
        static Token eofToken(TokenType::END_OF_FILE, "", SourceLocation(0, 0));
        return eofToken;
    }
    return (*m_tokens)[index];
}

bool Parser::isAtEnd() const {
    return m_currentIndex >= m_tokens->size() ||
           current().type == TokenType::END_OF_FILE;
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        m_currentIndex++;
    }
    return (*m_tokens)[m_currentIndex - 1];
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(const std::vector<TokenType>& types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& errorMsg) {
    if (check(type)) {
        return advance();
    }
    error(errorMsg);
    throw ParserError(errorMsg, current().location);
}

void Parser::synchronize() {
    skipToEndOfLine();
}

void Parser::skipToEndOfLine() {
    while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
        advance();
    }
    if (current().type == TokenType::END_OF_LINE) {
        advance();
    }
}

void Parser::skipBlankLines() {
    // Skip consecutive END_OF_LINE tokens (blank lines)
    // Also skip line numbers that appear on otherwise blank lines
    while (!isAtEnd()) {
        if (current().type == TokenType::END_OF_LINE) {
            advance();
        } else if (current().type == TokenType::NUMBER && peek().type == TokenType::END_OF_LINE) {
            // Line number followed by EOL - skip both
            advance(); // skip number
            advance(); // skip EOL
        } else {
            break;
        }
    }
}

void Parser::skipOptionalLineNumber() {
    // NO-OP: Line numbers are now stripped during preprocessing phase
    // This function is kept for API compatibility but does nothing
    // Line number information is preserved in m_lineNumberMapping for error reporting
}

// =============================================================================
// Error Reporting
// =============================================================================

void Parser::error(const std::string& message) {
    error(message, current().location);
}

void Parser::error(const std::string& message, const SourceLocation& loc) {
    m_errors.emplace_back(message, loc);
    // Error is collected; caller will check hasErrors() and display via dialog
}

// =============================================================================
// Top-Level Parsing
// =============================================================================

std::unique_ptr<Program> Parser::parse(const std::vector<Token>& tokens, const std::string& sourceFile) {
    m_currentSourceFile = sourceFile.empty() ? "<stdin>" : sourceFile;
    m_errors.clear();
    m_loopStack.clear();  // Reset loop nesting tracking
    m_options.reset();    // Reset compiler options
    m_lineNumberMapping.clear(); // Reset line number mapping

    // Reset auto line numbering for each parse
    m_autoLineNumber = m_autoLineStart;

    // FIRST: Expand all INCLUDE statements (preprocessing phase)
    expandIncludes(tokens);

    // Now parse the expanded token stream
    m_tokens = &m_expandedTokens;
    m_currentIndex = 0;

    // SECOND: Preprocess line numbers - strip them and build mapping
    // This simplifies multi-line parsing by removing line numbers from token stream
    preprocessLineNumbers(m_expandedTokens);

    // THIRD: Collect OPTION statements (compiler directives)
    // These must be processed before parsing the AST
    collectOptionsFromTokens();

    // FOURTH: Validate string literals based on Unicode mode
    // In ASCII mode, non-ASCII characters in string literals are errors
    validateStringLiterals();

    // FIFTH: Prescan for user-defined functions and subs
    // This allows forward references (calling functions before they're defined)
    prescanForFunctions();

    // Reset token position for main parsing
    m_currentIndex = 0;

    return parseProgram();
}

void Parser::preprocessLineNumbers(std::vector<Token>& tokens) {
    // Strip BASIC line numbers from the token stream and build a mapping
    // This makes parsing multi-line constructs much simpler since we don't
    // need to constantly skip over line numbers

    std::vector<Token> strippedTokens;
    strippedTokens.reserve(tokens.size());

    size_t currentPhysicalLine = 0;
    bool expectingLineNumber = true; // Line numbers can only appear at start of line

    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& token = tokens[i];

        // Track which physical line we're on
        if (token.type == TokenType::END_OF_LINE) {
            expectingLineNumber = true;
            currentPhysicalLine++;
            strippedTokens.push_back(token);
            continue;
        }

        // Check if this is a line number at the start of a line
        if (expectingLineNumber && token.type == TokenType::NUMBER) {
            // Look ahead to confirm this is a line number, not just a number in an expression
            // Line numbers are followed by a keyword, identifier, or EOL (for blank numbered lines)
            bool isLineNumber = false;

            if (i + 1 < tokens.size()) {
                TokenType nextType = tokens[i + 1].type;

                // Line numbers are followed by:
                // - A statement keyword (PRINT, LET, IF, etc.)
                // - A registry command/function (CIRCLE_SET_POSITION, etc.)
                // - An identifier (for implicit LET)
                // - END_OF_LINE (blank line with just a number)
                // - REM (comment)
                // - Colon (in case of ": REM" or similar)

                if (nextType == TokenType::END_OF_LINE ||
                    nextType == TokenType::END_OF_FILE ||
                    nextType == TokenType::COLON ||
                    tokens[i + 1].isKeyword() ||
                    nextType == TokenType::REGISTRY_COMMAND ||
                    nextType == TokenType::REGISTRY_FUNCTION ||
                    nextType == TokenType::IDENTIFIER) {
                    isLineNumber = true;
                }
            } else {
                // Number at end of file - treat as line number
                isLineNumber = true;
            }

            if (isLineNumber) {
                // This is a BASIC line number - record it and skip it
                int lineNum = static_cast<int>(token.numberValue);
                m_lineNumberMapping.addMapping(currentPhysicalLine, lineNum);
                expectingLineNumber = false;
                continue; // Skip this token
            }
        }

        // Not a line number - include it in stripped tokens
        expectingLineNumber = false;
        strippedTokens.push_back(token);
    }

    // Replace the token vector with the stripped version
    tokens = std::move(strippedTokens);
}

void Parser::collectOptionsFromTokens() {
    // Scan through tokens looking for OPTION statements
    // These must appear at the beginning of the program
    size_t savedIndex = m_currentIndex;

    while (!isAtEnd()) {
        // Skip line numbers, EOLs, and REM statements
        if (match(TokenType::NUMBER) || match(TokenType::END_OF_LINE)) {
            continue;
        }

        // Skip REM statements (comments)
        if (current().type == TokenType::REM) {
            // Skip to end of line
            while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
                advance();
            }
            continue;
        }

        // Check for OPTION statement
        if (current().type == TokenType::OPTION) {
            advance(); // consume OPTION

            if (match(TokenType::BITWISE)) {
                m_options.bitwiseOperators = true;
            } else if (match(TokenType::LOGICAL)) {
                m_options.bitwiseOperators = false;
            } else if (match(TokenType::BASE)) {
                if (current().type == TokenType::NUMBER) {
                    int base = static_cast<int>(current().numberValue);
                    advance();
                    if (base == 0 || base == 1) {
                        m_options.arrayBase = base;
                    } else {
                        error("OPTION BASE must be 0 or 1");
                    }
                } else {
                    error("Expected number after OPTION BASE");
                }
            } else if (match(TokenType::EXPLICIT)) {
                m_options.explicitDeclarations = true;
            } else if (match(TokenType::UNICODE)) {
                m_options.unicodeMode = true;
            } else if (match(TokenType::ERROR)) {
                m_options.errorTracking = true;
            } else if (match(TokenType::CANCELLABLE)) {
                if (match(TokenType::ON)) {
                    m_options.cancellableLoops = true;
                } else if (match(TokenType::OFF)) {
                    m_options.cancellableLoops = false;
                } else {
                    error("Expected ON or OFF after OPTION CANCELLABLE");
                }
            } else if (match(TokenType::FORCE_YIELD)) {
                m_options.forceYieldEnabled = true;
                // Check for optional instruction budget
                if (current().type == TokenType::NUMBER) {
                    int budget = static_cast<int>(current().numberValue);
                    advance();
                    if (budget < 100) {
                        error("OPTION FORCE_YIELD budget must be at least 100");
                        budget = 100;
                    }
                    if (budget > 1000000) {
                        error("OPTION FORCE_YIELD budget cannot exceed 1,000,000");
                        budget = 1000000;
                    }
                    m_options.forceYieldBudget = budget;
                }
                // If no number, keep default budget (10000)
            } else {
                error("Unknown OPTION type");
            }

            // Skip to end of line
            while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
                advance();
            }
            continue;
        }

        // Stop at first non-OPTION statement
        // OPTION directives must appear at the beginning
        break;
    }

    // Restore position
    m_currentIndex = savedIndex;
}

void Parser::validateStringLiterals() {
    // Validate all string literals in the token stream
    // In ASCII mode, string literals with non-ASCII characters are errors
    // In UNICODE mode, non-ASCII characters are allowed (UTF-8 will be converted to codepoints)

    if (m_options.unicodeMode) {
        // Unicode mode: all strings are allowed (UTF-8 will be decoded to codepoints)
        return;
    }

    // ASCII mode: check for non-ASCII characters in string literals
    for (const Token& token : *m_tokens) {
        if (token.type == TokenType::STRING && token.hasNonASCII) {
            // Report error with location information
            std::ostringstream oss;
            oss << "Non-ASCII characters in string literal are not allowed in ASCII mode.\n"
                << "Use OPTION UNICODE to enable Unicode string support.\n"
                << "String value: \"" << token.value << "\"";
            error(oss.str(), token.location);
            // Error will cause exit, but continue checking for completeness
        }
    }
}

std::unique_ptr<Program> Parser::parseProgram() {
    auto program = std::make_unique<Program>();

    // Reserve capacity based on token count estimate
    // Estimate: ~10 tokens per line on average
    size_t estimatedLines = m_tokens->size() / 10;
    if (estimatedLines > 0) {
        program->lines.reserve(estimatedLines);
    }

    size_t currentPhysicalLine = 0;
    
    while (!isAtEnd()) {
        // Skip empty lines (including consecutive blank lines)
        skipBlankLines();

        if (isAtEnd()) break;

        try {
            auto line = parseProgramLine(currentPhysicalLine);
            if (line) {
                program->addLine(std::move(line));
            }
            // Increment physical line after processing
            currentPhysicalLine++;
        } catch (const ParserError& e) {
            // Error already recorded - stop immediately
            return nullptr;
        }

        // Check if any errors were recorded (even without exception)
        if (hasErrors()) {
            return nullptr;
        }
    }

    // Check for unclosed loops at end of program
    if (!m_loopStack.empty()) {
        auto& unclosedLoop = m_loopStack.back();
        std::string loopTypeName;
        std::string expectedEnd;
        switch (unclosedLoop.first) {
            case LoopType::WHILE_WEND:
                loopTypeName = "WHILE";
                expectedEnd = "WEND";
                break;
            case LoopType::REPEAT_UNTIL:
                loopTypeName = "REPEAT";
                expectedEnd = "UNTIL";
                break;
            case LoopType::DO_LOOP:
                loopTypeName = "DO";
                expectedEnd = "LOOP";
                break;
        }
        error(loopTypeName + " loop started at line " +
              std::to_string(unclosedLoop.second.line) +
              " is missing closing " + expectedEnd, unclosedLoop.second);
        return nullptr;
    }

    return program;
}

std::unique_ptr<ProgramLine> Parser::parseProgramLine(size_t physicalLine) {
    // Check if this line had a BASIC line number (stored during preprocessing)
    int lineNumber = 0;
    bool hasLineNumber = false;
    
    const int* mappedLineNumber = m_lineNumberMapping.getBasicLineNumber(physicalLine);
    if (mappedLineNumber != nullptr) {
        lineNumber = *mappedLineNumber;
        hasLineNumber = true;
    }

    // If the line is just a REM statement, collect it
    if (current().type == TokenType::REM) {
        // Use the line number if present, otherwise use a special marker for unnumbered comments
        int commentLineNum = hasLineNumber ? lineNumber : m_autoLineNumber;
        m_currentLineNumber = commentLineNum;

        advance(); // consume REM

        // Collect comment text
        std::string comment;
        while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
            if (!comment.empty()) comment += " ";
            comment += current().value;
            advance();
        }

        // Store comment
        m_comments[commentLineNum] = comment;

        // Consume end of line
        if (current().type == TokenType::END_OF_LINE) {
            advance();
        }

        // If this is a numbered line, create an empty ProgramLine so that GOTO/GOSUB targets work
        // Otherwise, return nullptr (unnumbered comments can be skipped)
        if (hasLineNumber) {
            auto line = std::make_unique<ProgramLine>();
            line->lineNumber = commentLineNum;
            // Leave statements empty - this is just a marker for the CFG
            return line;
        }

        return nullptr;
    }

    // Normal line with statements
    auto line = std::make_unique<ProgramLine>();

    if (hasLineNumber) {
        line->lineNumber = lineNumber;
    } else {
        // Auto-assign line number if not present
        line->lineNumber = m_autoLineNumber;
        m_autoLineNumber += m_autoLineIncrement;
    }

    // Track current line number for comment collection
    m_currentLineNumber = line->lineNumber;

    // Parse statements separated by colons
    while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
        auto stmt = parseStatement();
        if (stmt) {
            line->addStatement(std::move(stmt));
        }

        // Multiple statements on one line separated by colon
        if (match(TokenType::COLON)) {
            continue;
        } else {
            break;
        }
    }

    // Consume end of line
    if (current().type == TokenType::END_OF_LINE) {
        advance();
    }

    return line;
}

// =============================================================================
// Statement Parsing
// =============================================================================

StatementPtr Parser::parseStatement() {
    // Skip any leading colons (statement separators)
    while (current().type == TokenType::COLON) {
        advance();
    }

    if (isAtEnd() || current().type == TokenType::END_OF_LINE) {
        return nullptr;
    }

    // Check for label definition: labelname: (identifier/keyword followed by colon)
    // Must be at start of statement (not after an expression)
    if ((current().type == TokenType::IDENTIFIER || current().isKeyword()) && 
        peek().type == TokenType::COLON) {
        std::string labelName = current().value;
        advance(); // consume label name
        advance(); // consume colon
        return std::make_unique<LabelStatement>(labelName);
    }

    TokenType type = current().type;

    switch (type) {
        case TokenType::PRINT:
        case TokenType::QUESTION:
            return parsePrintStatement();
        case TokenType::CONSOLE:
            return parseConsoleStatement();
        case TokenType::INPUT:
            return parseInputStatement();
        case TokenType::OPEN:
            return parseOpenStatement();
        case TokenType::CLOSE:
            return parseCloseStatement();
        case TokenType::PRINT_STREAM:
            return parsePrintStreamStatement();
        case TokenType::INPUT_STREAM:
            return parseInputStreamStatement();
        case TokenType::WRITE_STREAM:
            return parseWriteStreamStatement();
        case TokenType::LET:
            return parseLetStatement();
        case TokenType::GOTO:
            return parseGotoStatement();
        case TokenType::GOSUB:
            return parseGosubStatement();
        case TokenType::ON:
            return parseOnStatement();
        // ONEVENT removed - use AFTER/EVERY instead
        // case TokenType::ONEVENT:
        //     return parseOnEventStatement();
        case TokenType::CONSTANT:
            return parseConstantStatement();
        case TokenType::RETURN:
            return parseReturnStatement();
        case TokenType::EXIT:
            return parseExitStatement();
        case TokenType::IF:
            return parseIfStatement();
        case TokenType::CASE:
            // CASE is ambiguous - it could be:
            // 1. "CASE TRUE OF" (BBC BASIC style statement)
            // 2. A clause inside "SELECT CASE" (VB style)
            // Look ahead to disambiguate
            if (m_inSelectCase) {
                // Inside SELECT CASE - need to check if this is:
                // a) "CASE expression OF" (BBC BASIC nested statement)
                // b) "CASE condition" (SELECT CASE clause label)
                // Look ahead for OF token to distinguish
                size_t savedIndex = m_currentIndex;
                advance(); // consume CASE

                // Try to parse expression
                bool foundOf = false;
                int depth = 0;
                while (!isAtEnd() && depth < 20) {
                    if (current().type == TokenType::OF) {
                        foundOf = true;
                        break;
                    }
                    if (current().type == TokenType::END_OF_LINE ||
                        current().type == TokenType::COLON) {
                        break;
                    }
                    advance();
                    depth++;
                }

                // Restore position
                m_currentIndex = savedIndex;

                if (foundOf) {
                    // This is "CASE expression OF" - BBC BASIC statement (nested inside SELECT CASE)
                    std::cerr << "\n*** WARNING: Found nested 'CASE...OF' statement inside SELECT CASE at line "
                              << current().location.line << std::endl;
                    return parseCaseStatement();
                } else {
                    // This is a SELECT CASE clause label
                    // Return nullptr to let the SELECT CASE parser handle it
                    return nullptr;
                }
            } else {
                // Top-level CASE - must be BBC BASIC "CASE expression OF"
                // Peek ahead to verify OF is present
                size_t savedIndex = m_currentIndex;
                advance(); // consume CASE

                bool foundOf = false;
                int depth = 0;
                while (!isAtEnd() && depth < 20) {
                    if (current().type == TokenType::OF) {
                        foundOf = true;
                        break;
                    }
                    if (current().type == TokenType::END_OF_LINE ||
                        current().type == TokenType::COLON) {
                        break;
                    }
                    advance();
                    depth++;
                }

                // Restore position
                m_currentIndex = savedIndex;

                if (!foundOf) {
                    error("CASE statement requires 'OF' keyword (syntax: CASE expression OF). "
                          "Did you mean to use SELECT CASE instead?");
                    return nullptr;
                }

                return parseCaseStatement();
            }
        case TokenType::SELECT:
            return parseSelectCaseStatement();
        case TokenType::FOR:
            return parseForStatement();
        case TokenType::NEXT:
            return parseNextStatement();
        case TokenType::WHILE:
            return parseWhileStatement();
        case TokenType::WEND:
            return parseWendStatement();
        case TokenType::REPEAT:
            return parseRepeatStatement();
        case TokenType::UNTIL:
            return parseUntilStatement();
        case TokenType::DO:
            return parseDoStatement();
        case TokenType::LOOP:
            return parseLoopStatement();
        case TokenType::END:
            return parseEndStatement();
        case TokenType::DIM:
            return parseDimStatement();
        case TokenType::REDIM:
            return parseRedimStatement();
        case TokenType::ERASE:
            return parseEraseStatement();
        case TokenType::SWAP:
            return parseSwapStatement();
        case TokenType::INC:
            return parseIncStatement();
        case TokenType::DEC:
            return parseDecStatement();
        case TokenType::TYPE:
            return parseTypeDeclarationStatement();
        case TokenType::LOCAL:
            return parseLocalStatement();
        case TokenType::SHARED:
            return parseSharedStatement();
        case TokenType::DATA:
            return parseDataStatement();
        case TokenType::READ:
            return parseReadStatement();
        case TokenType::RESTORE:
            return parseRestoreStatement();
        case TokenType::REM:
            return parseRemStatement();
        case TokenType::OPTION:
            return parseOptionStatement();
        case TokenType::DEF:
            return parseDefStatement();
        case TokenType::FUNCTION:
            return parseFunctionStatement();
        case TokenType::SUB:
            return parseSubStatement();
        case TokenType::CALL:
            return parseCallStatement();
        case TokenType::CLS:
            return parseClsStatement();
        case TokenType::GCLS:
            return parseGclsStatement();
        case TokenType::COLOR:
            return parseColorStatement();
        case TokenType::WAIT:
            return parseWaitStatement();
        case TokenType::WAIT_MS:
            return parseWaitMsStatement();
        case TokenType::PLAY:
            return parsePlayStatement();

        case TokenType::PLAY_SOUND:
            return parsePlaySoundStatement();

        case TokenType::PSET:
            return parsePsetStatement();
        case TokenType::LINE:
            // Check if this is "LINE INPUT#" (file I/O) or "LINE" (graphics)
            if (peek().type == TokenType::INPUT_STREAM) {
                advance(); // consume LINE
                advance(); // consume INPUT_STREAM
                return parseLineInputStreamStatement();
            }
            // Also check for "LINE INPUT #" with space (INPUT followed by HASH)
            if (peek().type == TokenType::INPUT && peek(2).type == TokenType::HASH) {
                advance(); // consume LINE
                advance(); // consume INPUT
                advance(); // consume HASH
                return parseLineInputStreamStatement();
            }
            return parseLineStatement();
        case TokenType::RECT:
            return parseRectStatement();

        case TokenType::CIRCLE:
            return parseCircleStatement();
        case TokenType::CIRCLEF:
            return parseCirclefStatement();
        case TokenType::CLG:
            return parseClgStatement();
        case TokenType::HLINE:
            return parseHlineStatement();

        // Text Layer Commands
        case TokenType::AT:
        case TokenType::LOCATE:
            return parseAtStatement();
        case TokenType::TEXTPUT:
            return parseTextputStatement();
        case TokenType::PRINT_AT:  // Special command with PRINT-style syntax
            return parsePrintAtStatement();
        case TokenType::INPUT_AT:  // Special command with INPUT-style syntax
            return parseInputAtStatement();
        case TokenType::REGISTRY_COMMAND:
            return parseRegistryCommandStatement();
        case TokenType::TCHAR:
            return parseTcharStatement();
        case TokenType::TGRID:
            return parseTgridStatement();
        case TokenType::TSCROLL:
            return parseTscrollStatement();
        case TokenType::TCLEAR:
            return parseTclearStatement();

        // Sprite Commands
        case TokenType::SPRLOAD:
            return parseSprloadStatement();
        case TokenType::SPRFREE:
            return parseSprfreeStatement();
        case TokenType::SPRSHOW:
            return parseSprshowStatement();
        case TokenType::SPRHIDE:
            return parsSprhideStatement();
        case TokenType::SPRMOVE:
            return parseSprmoveStatement();
        case TokenType::SPRPOS:
            return parseSprposStatement();
        case TokenType::SPRTINT:
            return parseSprtintStatement();
        case TokenType::SPRSCALE:
            return parseSprscaleStatement();
        case TokenType::SPRROT:
            return parseSprrotStatement();
        case TokenType::SPREXPLODE:
            return parseSprexplodeStatement();



        // Timing Commands
        case TokenType::VSYNC:
            return parseVsyncStatement();
        
        case TokenType::AFTER:
            return parseAfterStatement();
        
        case TokenType::EVERY:
            return parseEveryStatement();
        
        case TokenType::AFTERFRAMES:
            return parseAfterFramesStatement();
        
        case TokenType::EVERYFRAME:
            return parseEveryFrameStatement();
        
        case TokenType::RUN:
            return parseRunStatement();
        
        case TokenType::TIMER:
            return parseTimerStatement();

        case TokenType::IDENTIFIER:
            // Check for MID$ assignment: MID$(var$, pos, len) = value
            if (current().value == "MID$") {
                return parseLetStatement();
            }
            // Check if this is a known user-defined SUB (implicit CALL)
            if (m_userDefinedSubs.find(current().value) != m_userDefinedSubs.end()) {
                // Implicit CALL to user-defined SUB
                std::string subName = current().value;
                advance();

                auto stmt = std::make_unique<CallStatement>(subName);

                // Parse arguments if present
                if (match(TokenType::LPAREN)) {
                    if (current().type != TokenType::RPAREN) {
                        do {
                            stmt->addArgument(parseExpression());
                        } while (match(TokenType::COMMA));
                    }
                    consume(TokenType::RPAREN, "Expected ')' after subroutine arguments");
                }

                return stmt;
            }
            // Could be implicit LET (assignment without LET keyword)
            if (m_allowImplicitLet && isAssignment()) {
                return parseLetStatement();
            }
            // Check if this looks like a builtin function call as a statement
            // (IDENTIFIER followed by '(' - will be validated by semantic analyzer)
            // Only treat as function call if NOT an assignment (checked above)
            if (peek().type == TokenType::LPAREN) {
                std::string funcName = current().value;
                advance(); // consume identifier
                advance(); // consume LPAREN

                auto stmt = std::make_unique<CallStatement>(funcName);

                // Parse arguments (may be empty for 0-argument functions)
                if (current().type != TokenType::RPAREN) {
                    do {
                        stmt->addArgument(parseExpression());
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RPAREN, "Expected ')' after function arguments");

                return stmt;
            }
            // Fall through to error
            [[fallthrough]];

        case TokenType::REGISTRY_FUNCTION:
            // Check for MID$ assignment: MID$(var$, pos, len) = value
            if (current().value == "MID$") {
                return parseLetStatement();
            }
            // For other registry functions, treat as function call statement
            if (peek().type == TokenType::LPAREN) {
                std::string funcName = current().value;
                advance(); // consume function name
                advance(); // consume LPAREN

                auto stmt = std::make_unique<CallStatement>(funcName);

                // Parse arguments (may be empty for 0-argument functions)
                if (current().type != TokenType::RPAREN) {
                    do {
                        stmt->addArgument(parseExpression());
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RPAREN, "Expected ')' after function arguments");

                return stmt;
            }
            // Fall through to error
            [[fallthrough]];

        default:
            error("Unexpected token: " + current().toString());
            advance();
            return nullptr;
    }
}

StatementPtr Parser::parsePrintStatement() {
    auto stmt = std::make_unique<PrintStatement>();
    advance(); // consume PRINT or ?

    // Reserve capacity for common case (most PRINT statements have 1-3 items)
    stmt->items.reserve(3);

    // Check for file number: PRINT #n, ...
    if (current().type == TokenType::HASH) {
        advance(); // consume #
        if (current().type != TokenType::NUMBER) {
            error("Expected file number after #");
            return stmt;
        }
        stmt->fileNumber = std::stoi(current().value);
        advance();

        // Require comma or semicolon after file number
        if (!match(TokenType::COMMA) && !match(TokenType::SEMICOLON)) {
            error("Expected , or ; after file number");
            return stmt;
        }
    }

    // Empty PRINT
    if (current().type == TokenType::END_OF_LINE ||
        current().type == TokenType::COLON) {
        return stmt;
    }

    // Check for PRINT USING
    if (match(TokenType::USING)) {
        stmt->hasUsing = true;

        // Parse format string expression
        stmt->formatExpr = parseExpression();

        // Require semicolon or comma separator after format string
        if (!match(TokenType::SEMICOLON) && !match(TokenType::COMMA)) {
            error("Expected ; or , after PRINT USING format string");
            return stmt;
        }

        // Parse values to format
        stmt->usingValues.reserve(4);
        while (!isAtEnd() &&
               current().type != TokenType::END_OF_LINE &&
               current().type != TokenType::COLON) {
            stmt->usingValues.push_back(parseExpression());

            // Check for separator
            if (!match(TokenType::SEMICOLON) && !match(TokenType::COMMA)) {
                break;
            }
        }

        return stmt;
    }

    // Regular PRINT (no USING)
    // Parse print items
    while (!isAtEnd() &&
           current().type != TokenType::END_OF_LINE &&
           current().type != TokenType::COLON) {

        auto expr = parseExpression();
        bool hasSemicolon = match(TokenType::SEMICOLON);
        bool hasComma = match(TokenType::COMMA);

        stmt->addItem(std::move(expr), hasSemicolon, hasComma);

        if (!hasSemicolon && !hasComma) {
            break;
        }
    }

    // Check if we should suppress newline
    if (!stmt->items.empty()) {
        const auto& lastItem = stmt->items.back();
        if (lastItem.semicolon || lastItem.comma) {
            stmt->trailingNewline = false;
        }
    }

    return stmt;
}

StatementPtr Parser::parseConsoleStatement() {
    auto stmt = std::make_unique<ConsoleStatement>();
    advance(); // consume CONSOLE

    // Reserve capacity for common case (most CONSOLE statements have 1-3 items)
    stmt->items.reserve(3);

    // Empty CONSOLE
    if (current().type == TokenType::END_OF_LINE ||
        current().type == TokenType::COLON) {
        return stmt;
    }

    // Parse console items
    while (!isAtEnd() &&
           current().type != TokenType::END_OF_LINE &&
           current().type != TokenType::COLON) {

        auto expr = parseExpression();
        bool hasSemicolon = match(TokenType::SEMICOLON);
        bool hasComma = match(TokenType::COMMA);

        stmt->addItem(std::move(expr), hasSemicolon, hasComma);

        if (!hasSemicolon && !hasComma) {
            break;
        }
    }

    // Check if we should suppress newline
    if (!stmt->items.empty()) {
        const auto& lastItem = stmt->items.back();
        if (lastItem.semicolon || lastItem.comma) {
            stmt->trailingNewline = false;
        }
    }

    return stmt;
}

StatementPtr Parser::parseInputStatement() {
    auto stmt = std::make_unique<InputStatement>();
    advance(); // consume INPUT

    // Optional prompt string
    if (current().type == TokenType::STRING) {
        stmt->prompt = current().value;
        advance();

        // Require semicolon or comma after prompt
        if (!match(TokenType::SEMICOLON) && !match(TokenType::COMMA)) {
            error("Expected ; or , after INPUT prompt");
        }
    }

    // Reserve capacity for common case (1-3 variables)
    stmt->variables.reserve(3);

    // Parse variable list
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in INPUT statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);
        stmt->addVariable(varName);

    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseLetStatement() {
    bool hasLet = match(TokenType::LET);

    // Check for MID$ assignment: MID$(var$, pos, len) = replacement$
    if ((current().type == TokenType::IDENTIFIER || current().type == TokenType::REGISTRY_FUNCTION) && current().value == "MID$") {
        advance(); // consume MID$

        if (!match(TokenType::LPAREN)) {
            error("Expected '(' after MID$");
            return nullptr;
        }

        // Parse the variable name
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in MID$ assignment");
            return nullptr;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);

        auto midStmt = std::make_unique<MidAssignStatement>(varName);

        if (!match(TokenType::COMMA)) {
            error("Expected ',' after variable in MID$ assignment");
            return nullptr;
        }

        // Parse position
        midStmt->position = parseExpression();

        if (!match(TokenType::COMMA)) {
            error("Expected ',' after position in MID$ assignment");
            return nullptr;
        }

        // Parse length
        midStmt->length = parseExpression();

        if (!match(TokenType::RPAREN)) {
            error("Expected ')' after MID$ parameters");
            return nullptr;
        }

        if (!match(TokenType::EQUAL)) {
            error("Expected '=' in MID$ assignment");
            return nullptr;
        }

        // Parse replacement expression
        midStmt->replacement = parseExpression();

        return midStmt;
    }

    if (current().type != TokenType::IDENTIFIER) {
        error("Expected variable name in assignment");
        return nullptr;
    }

    TokenType suffix = TokenType::UNKNOWN;
    std::string varName = parseVariableName(suffix);

    auto stmt = std::make_unique<LetStatement>(varName, suffix);

    // Check for array indices
    if (match(TokenType::LPAREN)) {
        // Support whole-array syntax: A() = ...
        // Empty parentheses means operate on entire array
        if (current().type != TokenType::RPAREN) {
            do {
                stmt->addIndex(parseExpression());
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RPAREN, "Expected ')' after array indices");
    }

    // Check for member access (e.g., P.X or P.Position.X)
    while (match(TokenType::DOT)) {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected member name after '.'");
            break;
        }
        stmt->addMember(current().value);
        advance();
    }

    consume(TokenType::EQUAL, "Expected '=' in assignment");

    stmt->value = parseExpression();

    // Check for AS type declaration (e.g., LET x = 10 AS INTEGER)
    TokenType asType = TokenType::UNKNOWN;
    if (current().type == TokenType::AS) {
        asType = parseAsType();
    }

    // Validate and merge types
    stmt->typeSuffix = mergeTypes(suffix, asType, varName);

    return stmt;
}

StatementPtr Parser::parseGotoStatement() {
    advance(); // consume GOTO

    // Check if it's a line number or symbolic label
    if (current().type == TokenType::NUMBER) {
        // GOTO line_number
        int line = parseLineNumber();
        return std::make_unique<GotoStatement>(line);
    } else {
        // GOTO label - allow identifiers or keywords as label names
        std::string label = current().value;
        advance();
        return std::make_unique<GotoStatement>(label);
    }
}

StatementPtr Parser::parseGosubStatement() {
    advance(); // consume GOSUB

    // Check if it's a line number or symbolic label
    if (current().type == TokenType::NUMBER) {
        // GOSUB line_number
        int line = parseLineNumber();
        return std::make_unique<GosubStatement>(line);
    } else {
        // GOSUB label - allow identifiers or keywords as label names
        std::string label = current().value;
        advance();
        return std::make_unique<GosubStatement>(label);
    }
}

StatementPtr Parser::parseOnStatement() {
    advance(); // consume ON

    // ON statement is now unambiguous - it's always indexed (ON expr GOTO/GOSUB)
    // Event-driven statements use ONEVENT keyword instead
    
    // Parse the selector expression for indexed ON statements
    auto selector = parseExpression();

    // Check for GOTO, GOSUB, or CALL
    if (current().type == TokenType::GOTO) {
        advance(); // consume GOTO
        auto stmt = std::make_unique<OnGotoStatement>();
        stmt->selector = std::move(selector);

        // Parse comma-separated list of labels/line numbers
        do {
            if (current().type == TokenType::COMMA) {
                advance(); // consume comma
            }

            if (current().type == TokenType::NUMBER) {
                // Line number
                int lineNum = parseLineNumber();
                stmt->addTarget(lineNum);
            } else if (current().type == TokenType::IDENTIFIER ||
                       current().isKeyword()) {
                // Label
                std::string label = current().value;
                advance();
                stmt->addTarget(label);
            } else {
                error("Expected line number or label in ON GOTO statement");
                break;
            }
        } while (current().type == TokenType::COMMA);

        return stmt;
    } else if (current().type == TokenType::GOSUB) {
        advance(); // consume GOSUB
        auto stmt = std::make_unique<OnGosubStatement>();
        stmt->selector = std::move(selector);

        // Parse comma-separated list of labels/line numbers
        do {
            if (current().type == TokenType::COMMA) {
                advance(); // consume comma
            }

            if (current().type == TokenType::NUMBER) {
                // Line number
                int lineNum = parseLineNumber();
                stmt->addTarget(lineNum);
            } else if (current().type == TokenType::IDENTIFIER ||
                       current().isKeyword()) {
                // Label
                std::string label = current().value;
                advance();
                stmt->addTarget(label);
            } else {
                error("Expected line number or label in ON GOSUB statement");
                break;
            }
        } while (current().type == TokenType::COMMA);

        return stmt;
    } else if (current().type == TokenType::CALL) {
        advance(); // consume CALL
        auto stmt = std::make_unique<OnCallStatement>();
        stmt->selector = std::move(selector);

        // Parse comma-separated list of function/sub names
        do {
            if (current().type == TokenType::COMMA) {
                advance(); // consume comma
            }

            if (current().type == TokenType::IDENTIFIER ||
                current().isKeyword()) {
                // Function/sub name
                std::string name = current().value;
                advance();
                stmt->addTarget(name);
            } else {
                error("Expected function or subroutine name in ON CALL statement");
                break;
            }
        } while (current().type == TokenType::COMMA);

        return stmt;
    } else {
        error("Expected GOTO, GOSUB, or CALL after ON expression");
        return nullptr;
    }
}

// DEPRECATED: ONEVENT removed in favor of AFTER/EVERY timer commands
// This function is no longer called and will be removed in future cleanup
/*
StatementPtr Parser::parseOnEventStatement() {
    advance(); // consume ONEVENT
    
    // Parse event name
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected event name after ONEVENT");
        return nullptr;
    }

    std::string eventName = current().value;
    advance();

    // Parse handler type (CALL, GOTO, GOSUB)
    EventHandlerType handlerType;
    if (current().type == TokenType::CALL) {
        handlerType = EventHandlerType::CALL;
        advance();
    } else if (current().type == TokenType::GOTO) {
        handlerType = EventHandlerType::GOTO;
        advance();
    } else if (current().type == TokenType::GOSUB) {
        handlerType = EventHandlerType::GOSUB;
        advance();
    } else {
        error("Expected CALL, GOTO, or GOSUB after event name");
        return nullptr;
    }

    // Parse target (function name, label, or line number)
    std::string target;
    bool isLineNumber = false;

    if (current().type == TokenType::NUMBER) {
        // Line number
        target = std::to_string(parseLineNumber());
        isLineNumber = true;
    } else if (current().type == TokenType::IDENTIFIER || current().isKeyword()) {
        // Function name or label
        target = current().value;
        advance();
        isLineNumber = false;
    } else {
        std::string handlerName = (handlerType == EventHandlerType::CALL ? "CALL" :
                                  handlerType == EventHandlerType::GOTO ? "GOTO" : "GOSUB");
        error("Expected function name, label, or line number after " + handlerName);
        return nullptr;
    }

    return std::make_unique<OnEventStatement>(eventName, handlerType, target, isLineNumber);
}
*/

StatementPtr Parser::parseConstantStatement() {
    advance(); // consume CONSTANT

    // Parse constant name
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected identifier after CONSTANT");
        return nullptr;
    }

    std::string name = current().value;
    advance();

    // Expect equals sign
    if (current().type != TokenType::EQUAL) {
        error("Expected '=' after constant name");
        return nullptr;
    }
    advance(); // consume =

    // Parse constant value expression
    auto value = parseExpression();

    return std::make_unique<ConstantStatement>(name, std::move(value));
}

StatementPtr Parser::parseReturnStatement() {
    advance(); // consume RETURN

    // Check if there's a return value expression
    if (current().type != TokenType::END_OF_LINE &&
        current().type != TokenType::END_OF_FILE &&
        current().type != TokenType::COLON) {
        // Parse return value expression
        auto returnValue = parseExpression();
        return std::make_unique<ReturnStatement>(std::move(returnValue));
    }

    return std::make_unique<ReturnStatement>();
}

StatementPtr Parser::parseExitStatement() {
    advance(); // consume EXIT

    // Determine what we're exiting
    if (current().type == TokenType::FOR) {
        advance(); // consume FOR
        return std::make_unique<ExitStatement>(ExitStatement::ExitType::FOR_LOOP);
    } else if (current().type == TokenType::DO) {
        advance(); // consume DO
        return std::make_unique<ExitStatement>(ExitStatement::ExitType::DO_LOOP);
    } else if (current().type == TokenType::WHILE) {
        advance(); // consume WHILE
        return std::make_unique<ExitStatement>(ExitStatement::ExitType::WHILE_LOOP);
    } else if (current().type == TokenType::REPEAT) {
        advance(); // consume REPEAT
        return std::make_unique<ExitStatement>(ExitStatement::ExitType::REPEAT_LOOP);
    } else if (current().type == TokenType::FUNCTION) {
        advance(); // consume FUNCTION
        return std::make_unique<ExitStatement>(ExitStatement::ExitType::FUNCTION);
    } else if (current().type == TokenType::SUB) {
        advance(); // consume SUB
        return std::make_unique<ExitStatement>(ExitStatement::ExitType::SUB);
    } else {
        error("Expected FOR, DO, WHILE, REPEAT, FUNCTION, or SUB after EXIT");
        return nullptr;
    }
}

StatementPtr Parser::parseIfStatement() {
    auto stmt = std::make_unique<IfStatement>();
    advance(); // consume IF

    stmt->condition = parseExpression();

    consume(TokenType::THEN, "Expected THEN after IF condition");

    // Check if this is a multi-line IF (THEN followed by EOL)
    // NOTE: COLON does NOT trigger multi-line mode - it's for single-line with multiple statements
    if (current().type == TokenType::END_OF_LINE) {
        // Multi-line IF...ENDIF block
        stmt->isMultiLine = true;

        // Skip to next line
        advance();

        // Parse THEN block until ELSEIF, ELSE, or ENDIF
        while (!isAtEnd()) {
            skipBlankLines();

            if (isAtEnd()) break;

            // Skip optional line number at start of line
            skipOptionalLineNumber();

            // Check for end of THEN block
            if (current().type == TokenType::ELSEIF ||
                current().type == TokenType::ELSE ||
                current().type == TokenType::ENDIF) {
                break;
            }

            // Check for END IF (two tokens)
            if (current().type == TokenType::END) {
                if (peek().type == TokenType::IF) {
                    advance(); // consume END
                    advance(); // consume IF
                    return stmt;
                }
            }

            // Parse statements on this line (may be separated by colons)
            while (!isAtEnd() &&
                   current().type != TokenType::END_OF_LINE &&
                   current().type != TokenType::ELSEIF &&
                   current().type != TokenType::ELSE &&
                   current().type != TokenType::ENDIF) {

                // Check for END IF (two tokens)
                if (current().type == TokenType::END && peek().type == TokenType::IF) {
                    break;
                }

                auto thenStmt = parseStatement();
                if (thenStmt) {
                    stmt->addThenStatement(std::move(thenStmt));
                }

                // If there's a colon, continue parsing more statements on this line
                if (current().type == TokenType::COLON) {
                    advance(); // consume colon
                } else {
                    // No more statements on this line
                    break;
                }
            }

            // Skip EOL after statement(s)
            if (current().type == TokenType::END_OF_LINE) {
                advance();
            }
        }

        // Parse ELSEIF clauses (including "ELSE IF" as two tokens)
        while (current().type == TokenType::ELSEIF ||
               (current().type == TokenType::ELSE && peek().type == TokenType::IF)) {
            if (current().type == TokenType::ELSEIF) {
                advance(); // consume ELSEIF
            } else {
                advance(); // consume ELSE
                advance(); // consume IF
            }

            auto elseifCondition = parseExpression();
            consume(TokenType::THEN, "Expected THEN after ELSEIF condition");

            stmt->addElseIfClause(std::move(elseifCondition));

            // Skip to next line
            if (current().type == TokenType::END_OF_LINE) {
                advance();
            }

            // Parse ELSEIF block
            while (!isAtEnd()) {
                skipBlankLines();

                if (isAtEnd()) break;

                // Skip optional line number at start of line
                skipOptionalLineNumber();

                if (current().type == TokenType::ELSEIF ||
                    current().type == TokenType::ELSE ||
                    current().type == TokenType::ENDIF) {
                    break;
                }

                if (current().type == TokenType::END && peek().type == TokenType::IF) {
                    break;
                }

                // Parse statements on this line (may be separated by colons)
                while (!isAtEnd() &&
                       current().type != TokenType::END_OF_LINE &&
                       current().type != TokenType::ELSEIF &&
                       current().type != TokenType::ELSE &&
                       current().type != TokenType::ENDIF) {

                    // Check for END IF (two tokens)
                    if (current().type == TokenType::END && peek().type == TokenType::IF) {
                        break;
                    }

                    auto elseifStmt = parseStatement();
                    if (elseifStmt) {
                        stmt->addElseIfStatement(std::move(elseifStmt));
                    }

                    // If there's a colon, continue parsing more statements on this line
                    if (current().type == TokenType::COLON) {
                        advance(); // consume colon
                    } else {
                        // No more statements on this line
                        break;
                    }
                }

                // Skip EOL after statement(s)
                if (current().type == TokenType::END_OF_LINE) {
                    advance();
                }
            }
        }

        // Parse ELSE clause (but not "ELSE IF" which is handled above)
        if (current().type == TokenType::ELSE && peek().type != TokenType::IF) {
            advance(); // consume ELSE

            // Skip to next line
            if (current().type == TokenType::END_OF_LINE) {
                advance();
            }

            // Parse ELSE block
            while (!isAtEnd()) {
                skipBlankLines();

                if (isAtEnd()) break;

                // Skip optional line number at start of line
                skipOptionalLineNumber();

                if (current().type == TokenType::ENDIF) {
                    break;
                }

                if (current().type == TokenType::END && peek().type == TokenType::IF) {
                    break;
                }

                // Parse statements on this line (may be separated by colons)
                while (!isAtEnd() &&
                       current().type != TokenType::END_OF_LINE &&
                       current().type != TokenType::ENDIF) {

                    // Check for END IF (two tokens)
                    if (current().type == TokenType::END && peek().type == TokenType::IF) {
                        break;
                    }

                    auto elseStmt = parseStatement();
                    if (elseStmt) {
                        stmt->addElseStatement(std::move(elseStmt));
                    }

                    // If there's a colon, continue parsing more statements on this line
                    if (current().type == TokenType::COLON) {
                        advance(); // consume colon
                    } else {
                        // No more statements on this line
                        break;
                    }
                }

                if (current().type == TokenType::END_OF_LINE) {
                    advance();
                }
            }
        }

        // Consume ENDIF or END IF
        if (current().type == TokenType::ENDIF) {
            advance();
        } else if (current().type == TokenType::END && peek().type == TokenType::IF) {
            advance(); // consume END
            advance(); // consume IF
        } else {
            error("Expected ENDIF or END IF to close multi-line IF statement");
        }

    } else {
        // Single-line IF
        stmt->isMultiLine = false;

        // Check for GOTO form: IF condition THEN lineNumber or IF condition THEN label
        if (current().type == TokenType::NUMBER) {
            stmt->hasGoto = true;
            stmt->gotoLine = static_cast<int>(current().numberValue);
            advance();
        } else if (current().type == TokenType::IDENTIFIER && peek().type != TokenType::EQUAL) {
            // IF condition THEN label (converted from line number by preprocessor)
            // Only treat as label if NOT followed by = (which would be an assignment)
            // Labels from preprocessor start with 'L' followed by digits
            std::string label = current().value;
            if (label.length() > 1 && label[0] == 'L' && std::isdigit(label[1])) {
                // This is a preprocessed label like L100, L200, etc.
                advance();
                auto gotoStmt = std::make_unique<GotoStatement>(label);
                stmt->addThenStatement(std::move(gotoStmt));
            } else {
                // Not a preprocessed label - parse as statement
                goto parse_then_statements;
            }
        } else if (current().type == TokenType::GOTO) {
            // IF condition THEN GOTO lineNumber or label
            advance(); // consume GOTO
            if (current().type == TokenType::NUMBER) {
                // GOTO line number
                stmt->hasGoto = true;
                stmt->gotoLine = parseLineNumber();
            } else {
                // GOTO label - allow identifiers or keywords as label names
                std::string label = current().value;
                advance();
                auto gotoStmt = std::make_unique<GotoStatement>(label);
                stmt->addThenStatement(std::move(gotoStmt));
            }
        } else {
parse_then_statements:
            // IF condition THEN statement(s)
            // Parse statements until ELSEIF, ELSE or end of line
            // NOTE: Colon separates statements WITHIN the THEN clause, not after it
            while (!isAtEnd() &&
                   current().type != TokenType::END_OF_LINE &&
                   current().type != TokenType::ELSEIF &&
                   current().type != TokenType::ELSE) {
                auto thenStmt = parseStatement();
                if (thenStmt) {
                    stmt->addThenStatement(std::move(thenStmt));
                }

                // If there's a colon, continue parsing more THEN statements
                if (current().type == TokenType::COLON) {
                    advance(); // consume colon
                } else {
                    // No more statements in THEN clause
                    break;
                }
            }
        }

        // Optional ELSEIF clauses (single-line)
        while (current().type == TokenType::ELSEIF) {
            advance(); // consume ELSEIF

            auto elseifCondition = parseExpression();
            consume(TokenType::THEN, "Expected THEN after ELSEIF condition");

            stmt->addElseIfClause(std::move(elseifCondition));

            // Parse ELSEIF statements until next ELSEIF, ELSE, or end of line
            while (!isAtEnd() &&
                   current().type != TokenType::END_OF_LINE &&
                   current().type != TokenType::ELSEIF &&
                   current().type != TokenType::ELSE) {
                auto elseifStmt = parseStatement();
                if (elseifStmt) {
                    stmt->addElseIfStatement(std::move(elseifStmt));
                }

                // If there's a colon, continue parsing more ELSEIF statements
                if (current().type == TokenType::COLON) {
                    advance(); // consume colon
                } else {
                    break;
                }
            }
        }

        // Optional ELSE clause (single-line only)
        if (match(TokenType::ELSE)) {
            if (current().type == TokenType::NUMBER) {
                // ELSE lineNumber (implicit GOTO)
                auto gotoStmt = std::make_unique<GotoStatement>(static_cast<int>(current().numberValue));
                advance();
                stmt->addElseStatement(std::move(gotoStmt));
            } else {
                // ELSE statement(s)
                while (!isAtEnd() &&
                       current().type != TokenType::END_OF_LINE &&
                       current().type != TokenType::COLON) {
                    auto elseStmt = parseStatement();
                    if (elseStmt) {
                        stmt->addElseStatement(std::move(elseStmt));
                    }
                }
            }
        }
    }

    return stmt;
}

// Parse CASE statement (BBC BASIC style: CASE expression OF ... ENDCASE)
StatementPtr Parser::parseCaseStatement() {
    auto stmt = std::make_unique<CaseStatement>();
    advance(); // consume CASE

    // Parse the CASE expression (e.g., TRUE)
    stmt->caseExpression = parseExpression();

    // Expect OF
    consume(TokenType::OF, "Expected OF after CASE expression");

    // Consume optional newline/colon after OF
    if (current().type == TokenType::END_OF_LINE || current().type == TokenType::COLON) {
        advance();
    }

    // Parse WHEN clauses
    while (!isAtEnd() && current().type != TokenType::ENDCASE &&
           current().type != TokenType::OTHERWISE && current().type != TokenType::END_OF_FILE) {
        
        // Check for END CASE (two tokens)
        if (current().type == TokenType::END && peek().type == TokenType::CASE) {
            break;
        }

        skipBlankLines();

        if (isAtEnd()) break;

        // Skip optional line number at start of line
        skipOptionalLineNumber();

        if (current().type == TokenType::WHEN) {
            advance(); // consume WHEN

            // Parse comma-separated values for WHEN
            std::vector<ExpressionPtr> values;
            do {
                ExpressionPtr value = parseExpression();
                values.push_back(std::move(value));
            } while (match(TokenType::COMMA));

            stmt->addWhenClause(std::move(values));

            // Expect colon after condition
            if (current().type == TokenType::COLON) {
                advance();
            }

            // Parse statements on the same line or next lines until next WHEN/OTHERWISE/ENDCASE
            while (!isAtEnd() && current().type != TokenType::WHEN &&
                   current().type != TokenType::OTHERWISE && current().type != TokenType::ENDCASE) {
                
                // Check for END CASE (two tokens)
                if (current().type == TokenType::END && peek().type == TokenType::CASE) {
                    break;
                }

                skipBlankLines();

                if (isAtEnd() || current().type == TokenType::WHEN ||
                    current().type == TokenType::OTHERWISE || current().type == TokenType::ENDCASE) {
                    break;
                }

                // Skip optional line number at start of line
                skipOptionalLineNumber();

                // Check if we've reached the next WHEN/OTHERWISE/ENDCASE after skipping line number
                if (current().type == TokenType::WHEN ||
                    current().type == TokenType::OTHERWISE ||
                    current().type == TokenType::ENDCASE) {
                    break;
                }
                
                // Check for END CASE (two tokens)
                if (current().type == TokenType::END && peek().type == TokenType::CASE) {
                    break;
                }

                auto whenStmt = parseStatement();
                if (whenStmt) {
                    stmt->addWhenStatement(std::move(whenStmt));
                }

                // Stop if errors occurred
                if (hasErrors()) {
                    return nullptr;
                }

                // Check for multiple statements on same line or continue to next line
                if (current().type == TokenType::COLON) {
                    advance();
                    continue;
                } else if (current().type == TokenType::END_OF_LINE) {
                    advance();
                    // Don't break - continue parsing more statements on next lines
                    continue;
                } else {
                    break;
                }
            }
        } else if (current().type == TokenType::OTHERWISE) {
            break; // Handle OTHERWISE outside the loop
        } else {
            // Unexpected token - provide context about CASE statement
            error("Expected WHEN, OTHERWISE, or ENDCASE in CASE statement. Found: " +
                  current().toString() + ". (Note: CASE statement syntax is 'CASE expression OF')");
            return nullptr;
        }
    }

    // Parse optional OTHERWISE clause
    if (current().type == TokenType::OTHERWISE) {
        advance(); // consume OTHERWISE

        // Expect colon after OTHERWISE
        if (current().type == TokenType::COLON) {
            advance();
        }

        // Parse statements until ENDCASE
        while (!isAtEnd() && current().type != TokenType::ENDCASE) {
            
            // Check for END CASE (two tokens)
            if (current().type == TokenType::END && peek().type == TokenType::CASE) {
                break;
            }
            skipBlankLines();

            if (isAtEnd() || current().type == TokenType::ENDCASE) {
                break;
            }

            // Skip optional line number at start of line
            skipOptionalLineNumber();

            // Check if we've reached ENDCASE after skipping line number
            if (current().type == TokenType::ENDCASE) {
                break;
            }
            
            // Check for END CASE (two tokens)
            if (current().type == TokenType::END && peek().type == TokenType::CASE) {
                break;
            }

            auto otherwiseStmt = parseStatement();
            if (otherwiseStmt) {
                stmt->addOtherwiseStatement(std::move(otherwiseStmt));
            }

            // Check for multiple statements on same line
            if (current().type == TokenType::COLON) {
                advance();
                continue;
            } else if (current().type == TokenType::END_OF_LINE) {
                advance();
                // Continue to check for ENDCASE on next line
                continue;
            } else {
                break;
            }
        }
    }

    // Expect ENDCASE or END CASE
    if (current().type == TokenType::ENDCASE) {
        advance();
    } else if (current().type == TokenType::END && peek().type == TokenType::CASE) {
        advance(); // consume END
        advance(); // consume CASE
    } else {
        error("Expected ENDCASE or END CASE to close CASE statement");
    }

    return stmt;
}

// Parse SELECT CASE statement (Visual Basic style: SELECT CASE expression ... END SELECT)
StatementPtr Parser::parseSelectCaseStatement() {
    auto stmt = std::make_unique<CaseStatement>();
    advance(); // consume SELECT

    // Expect CASE after SELECT
    consume(TokenType::CASE, "Expected CASE after SELECT");

    // Enter SELECT CASE mode - CASE tokens are now clause labels, not statements
    bool savedSelectCaseState = m_inSelectCase;
    m_inSelectCase = true;

    // Parse the SELECT CASE expression (e.g., TRUE)
    stmt->caseExpression = parseExpression();

    // Consume optional newline/colon after expression
    if (current().type == TokenType::END_OF_LINE || current().type == TokenType::COLON) {
        advance();
    }

    // Parse CASE clauses (note: in SELECT CASE, we use CASE not WHEN)
    while (!isAtEnd() && current().type != TokenType::END &&
           current().type != TokenType::ELSE && current().type != TokenType::END_OF_FILE) {

        skipBlankLines();

        if (isAtEnd() || current().type == TokenType::ELSE || current().type == TokenType::END) {
            break;
        }

        // Skip optional line number at start of line
        skipOptionalLineNumber();

        if (current().type == TokenType::CASE) {
            advance(); // consume CASE

            // Parse comma-separated values for CASE
            std::vector<ExpressionPtr> values;
            do {
                ExpressionPtr value = parseExpression();
                values.push_back(std::move(value));
            } while (match(TokenType::COMMA));

            stmt->addWhenClause(std::move(values));

            // Optional colon or newline after condition
            if (current().type == TokenType::COLON) {
                advance();
            }
            if (current().type == TokenType::END_OF_LINE) {
                advance();
            }

            // Parse statements until next CASE/ELSE/END
            while (!isAtEnd() && current().type != TokenType::ELSE && current().type != TokenType::END) {

                skipBlankLines();

                if (isAtEnd() || current().type == TokenType::CASE ||
                    current().type == TokenType::ELSE || current().type == TokenType::END) {
                    break;
                }

                // Skip optional line number at start of line
                skipOptionalLineNumber();

                // Check if we've reached the next CASE/ELSE/END after skipping line number
                if (current().type == TokenType::CASE ||
                    current().type == TokenType::ELSE ||
                    current().type == TokenType::END) {
                    break;
                }

                // Check if CASE is actually a new clause or a nested BBC BASIC statement
                if (current().type == TokenType::CASE) {
                    // Lookahead to check for OF token
                    size_t savedIndex = m_currentIndex;
                    advance(); // consume CASE

                    bool foundOf = false;
                    int depth = 0;
                    while (!isAtEnd() && depth < 20) {
                        if (current().type == TokenType::OF) {
                            foundOf = true;
                            break;
                        }
                        if (current().type == TokenType::END_OF_LINE ||
                            current().type == TokenType::COLON) {
                            break;
                        }
                        advance();
                        depth++;
                    }

                    // Restore position
                    m_currentIndex = savedIndex;

                    if (!foundOf) {
                        // This is a new SELECT CASE clause, not a nested BBC BASIC statement
                        std::cerr << "\n*** Found new CASE clause at line " << current().location.line << std::endl;
                        break;
                    } else {
                        // Found nested BBC BASIC CASE...OF statement
                        std::cerr << "\n*** WARNING: Found nested BBC BASIC 'CASE...OF' inside SELECT CASE at line "
                                  << current().location.line << std::endl;
                    }
                    // If foundOf is true, fall through to parseStatement() which will handle it
                }

                auto caseStmt = parseStatement();
                if (caseStmt) {
                    stmt->addWhenStatement(std::move(caseStmt));
                }

                // Stop if errors occurred
                if (hasErrors()) {
                    return nullptr;
                }

                // Check for multiple statements on same line
                if (current().type == TokenType::COLON) {
                    advance();
                    continue;
                } else if (current().type == TokenType::END_OF_LINE) {
                    advance();
                    // Check if next line starts a new CASE/ELSE/END
                    if (current().type == TokenType::ELSE ||
                        current().type == TokenType::END) {
                        break;
                    }
                    // For CASE, we need to check if it's a clause or nested statement
                    if (current().type == TokenType::CASE) {
                        // Will be checked in next iteration
                        continue;
                    }
                } else {
                    break;
                }
            }
        } else if (current().type == TokenType::ELSE) {
            break; // Handle ELSE outside the loop
        } else if (current().type == TokenType::END) {
            break; // END SELECT
        } else {
            // Unexpected token - provide helpful context
            error("Expected CASE clause, ELSE, or END SELECT in SELECT CASE statement. Found: " +
                  current().toString() + ". (Note: In SELECT CASE, use 'CASE condition', not 'WHEN')");
            return nullptr;
        }
    }

    // Parse optional ELSE clause (equivalent to OTHERWISE)
    if (current().type == TokenType::ELSE) {
        advance(); // consume ELSE

        // Optional colon or newline after ELSE
        if (current().type == TokenType::COLON) {
            advance();
        }
        if (current().type == TokenType::END_OF_LINE) {
            advance();
        }

        // Parse statements until END
        while (!isAtEnd() && current().type != TokenType::END) {
            skipBlankLines();

            if (isAtEnd() || current().type == TokenType::END) {
                break;
            }

            // Skip optional line number at start of line
            skipOptionalLineNumber();

            // Check if we've reached END after skipping line number
            if (current().type == TokenType::END) {
                break;
            }

            auto elseStmt = parseStatement();
            if (elseStmt) {
                stmt->addOtherwiseStatement(std::move(elseStmt));
            }

            // Check for multiple statements on same line
            if (current().type == TokenType::COLON) {
                advance();
                continue;
            } else if (current().type == TokenType::END_OF_LINE) {
                advance();
                // Continue to check for END on next line
                continue;
            } else {
                break;
            }
        }
    }

    // Expect END SELECT
    consume(TokenType::END, "Expected END to close SELECT CASE statement");
    consume(TokenType::SELECT, "Expected SELECT after END");

    // Restore previous SELECT CASE state
    m_inSelectCase = savedSelectCaseState;

    return stmt;
}

StatementPtr Parser::parseForStatement() {
    advance(); // consume FOR

    if (current().type != TokenType::IDENTIFIER) {
        error("Expected variable name in FOR statement");
        return nullptr;
    }

    TokenType suffix = TokenType::UNKNOWN;
    std::string varName = parseVariableName(suffix);

    // Check if this is FOR...IN or traditional FOR...TO
    if (current().type == TokenType::IN) {
        // FOR...IN syntax: FOR var IN array
        advance(); // consume IN

        auto stmt = std::make_unique<ForInStatement>(varName);
        stmt->array = parseExpression();

        return stmt;
    } else if (current().type == TokenType::COMMA) {
        // FOR...IN with index: FOR var, index IN array
        advance(); // consume comma

        if (current().type != TokenType::IDENTIFIER) {
            error("Expected index variable name after comma in FOR...IN statement");
            return nullptr;
        }

        TokenType indexSuffix = TokenType::UNKNOWN;
        std::string indexVarName = parseVariableName(indexSuffix);

        consume(TokenType::IN, "Expected IN after index variable in FOR statement");

        auto stmt = std::make_unique<ForInStatement>(varName, indexVarName);
        stmt->array = parseExpression();

        return stmt;
    } else {
        // Traditional FOR...TO syntax
        auto stmt = std::make_unique<ForStatement>(varName);

        consume(TokenType::EQUAL, "Expected '=' in FOR statement");

        stmt->start = parseExpression();

        consume(TokenType::TO, "Expected TO in FOR statement");

        stmt->end = parseExpression();

        // Optional STEP
        if (match(TokenType::STEP)) {
            stmt->step = parseExpression();
        }

        return stmt;
    }
}

StatementPtr Parser::parseNextStatement() {
    advance(); // consume NEXT

    auto stmt = std::make_unique<NextStatement>();

    // Optional variable name
    if (current().type == TokenType::IDENTIFIER) {
        TokenType suffix = TokenType::UNKNOWN;
        stmt->variable = parseVariableName(suffix);
    }

    return stmt;
}

StatementPtr Parser::parseWhileStatement() {
    auto stmt = std::make_unique<WhileStatement>();
    SourceLocation whileLocation = current().location;
    advance(); // consume WHILE

    stmt->condition = parseExpression();

    // Push WHILE onto loop stack to track nesting
    m_loopStack.push_back({LoopType::WHILE_WEND, whileLocation});

    return stmt;
}

StatementPtr Parser::parseWendStatement() {
    SourceLocation wendLocation = current().location;
    
    // Handle both WEND and END WHILE
    bool isEndWhile = false;
    if (current().type == TokenType::END && peek().type == TokenType::WHILE) {
        isEndWhile = true;
    }

    // Check if we have a matching WHILE
    if (m_loopStack.empty()) {
        if (isEndWhile) {
            error("END WHILE without matching WHILE", wendLocation);
        } else {
            error("WEND without matching WHILE", wendLocation);
        }
        return nullptr;
    }

    // Check if the top of stack is WHILE_WEND
    if (m_loopStack.back().first != LoopType::WHILE_WEND) {
        std::string loopTypeName;
        switch (m_loopStack.back().first) {
            case LoopType::REPEAT_UNTIL:
                loopTypeName = "REPEAT (expected UNTIL)";
                break;
            case LoopType::DO_LOOP:
                loopTypeName = "DO (expected LOOP)";
                break;
            default:
                loopTypeName = "unknown loop";
        }
        error("WEND found but current loop is " + loopTypeName +
              " started at line " + std::to_string(m_loopStack.back().second.line), wendLocation);
        return nullptr;
    }

    // Pop the WHILE from stack
    // Pop WHILE from loop stack
    m_loopStack.pop_back();

    if (isEndWhile) {
        advance(); // consume END
        advance(); // consume WHILE
    } else {
        advance(); // consume WEND
    }
    return std::make_unique<WendStatement>();
}

StatementPtr Parser::parseRepeatStatement() {
    SourceLocation repeatLocation = current().location;
    advance(); // consume REPEAT

    // Push REPEAT onto loop stack to track nesting
    m_loopStack.push_back({LoopType::REPEAT_UNTIL, repeatLocation});

    return std::make_unique<RepeatStatement>();
}

StatementPtr Parser::parseUntilStatement() {
    auto stmt = std::make_unique<UntilStatement>();
    SourceLocation untilLocation = current().location;

    // Check if we have a matching REPEAT
    if (m_loopStack.empty()) {
        error("UNTIL without matching REPEAT", untilLocation);
        return nullptr;
    }

    // Check if the top of stack is REPEAT_UNTIL
    if (m_loopStack.back().first != LoopType::REPEAT_UNTIL) {
        std::string loopTypeName;
        switch (m_loopStack.back().first) {
            case LoopType::WHILE_WEND:
                loopTypeName = "WHILE (expected WEND)";
                break;
            case LoopType::DO_LOOP:
                loopTypeName = "DO (expected LOOP)";
                break;
            default:
                loopTypeName = "unknown loop";
        }
        error("UNTIL found but current loop is " + loopTypeName +
              " started at line " + std::to_string(m_loopStack.back().second.line), untilLocation);
        return nullptr;
    }

    // Pop the REPEAT from stack
    m_loopStack.pop_back();

    advance(); // consume UNTIL
    stmt->condition = parseExpression();

    return stmt;
}

StatementPtr Parser::parseDoStatement() {
    auto stmt = std::make_unique<DoStatement>();
    SourceLocation doLocation = current().location;
    advance(); // consume DO

    // Check for WHILE or UNTIL condition
    if (current().type == TokenType::WHILE) {
        advance(); // consume WHILE
        stmt->conditionType = DoStatement::ConditionType::WHILE;
        stmt->condition = parseExpression();
    } else if (current().type == TokenType::UNTIL) {
        advance(); // consume UNTIL
        stmt->conditionType = DoStatement::ConditionType::UNTIL;
        stmt->condition = parseExpression();
    } else {
        // Plain DO (infinite loop)
        stmt->conditionType = DoStatement::ConditionType::NONE;
    }

    // Push DO onto loop stack to track nesting
    m_loopStack.push_back({LoopType::DO_LOOP, doLocation});

    return stmt;
}

StatementPtr Parser::parseLoopStatement() {
    auto stmt = std::make_unique<LoopStatement>();
    SourceLocation loopLocation = current().location;

    // Check if we have a matching DO
    if (m_loopStack.empty()) {
        error("LOOP without matching DO", loopLocation);
        return nullptr;
    }

    // Check if the top of stack is DO_LOOP
    if (m_loopStack.back().first != LoopType::DO_LOOP) {
        std::string loopTypeName;
        switch (m_loopStack.back().first) {
            case LoopType::WHILE_WEND:
                loopTypeName = "WHILE (expected WEND)";
                break;
            case LoopType::REPEAT_UNTIL:
                loopTypeName = "REPEAT (expected UNTIL)";
                break;
            default:
                loopTypeName = "unknown loop";
        }
        error("LOOP found but current loop is " + loopTypeName +
              " started at line " + std::to_string(m_loopStack.back().second.line), loopLocation);
        return nullptr;
    }

    // Pop the DO from stack
    m_loopStack.pop_back();

    advance(); // consume LOOP

    // Check for WHILE or UNTIL condition (post-test)
    if (current().type == TokenType::WHILE) {
        advance(); // consume WHILE
        stmt->conditionType = LoopStatement::ConditionType::WHILE;
        stmt->condition = parseExpression();
    } else if (current().type == TokenType::UNTIL) {
        advance(); // consume UNTIL
        stmt->conditionType = LoopStatement::ConditionType::UNTIL;
        stmt->condition = parseExpression();
    } else {
        // Plain LOOP
        stmt->conditionType = LoopStatement::ConditionType::NONE;
    }

    return stmt;
}

StatementPtr Parser::parseEndStatement() {
    // Check if this is a compound END keyword (END WHILE, END CASE, etc.)
    if (peek().type == TokenType::WHILE) {
        return parseWendStatement();
    }
    
    // Note: END SUB, END FUNCTION, END IF, END TYPE handled in their respective parsers
    // These should not appear as standalone statements
    
    advance(); // consume END
    return std::make_unique<EndStatement>();
}

StatementPtr Parser::parseDimStatement() {
    auto stmt = std::make_unique<DimStatement>();
    advance(); // consume DIM

    // Reserve capacity for common case (1-4 arrays)
    stmt->arrays.reserve(4);

    // Parse array or variable declarations
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable or array name in DIM statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);

        stmt->addArray(varName, suffix);

        // Check if this is an array (has dimensions)
        bool hasIndices = false;
        if (match(TokenType::LPAREN)) {
            hasIndices = true;
            // Parse dimensions
            do {
                stmt->addDimension(parseExpression());
            } while (match(TokenType::COMMA));

            consume(TokenType::RPAREN, "Expected ')' after array dimensions");
        }
        // Otherwise it's a scalar variable (no dimensions)

        // Check for AS type declaration
        if (current().type == TokenType::AS) {
            advance(); // consume AS
            
            // Check if it's a built-in type or user-defined type
            if (isTypeKeyword(current().type)) {
                // Built-in type keyword (INT, FLOAT, DOUBLE, STRING)
                TokenType asType = current().type;
                advance();
                
                // Convert AS type keyword to type suffix
                TokenType convertedType = asTypeToSuffix(asType);
                
                // For built-in types, merge with any explicit suffix
                if (!stmt->arrays.empty()) {
                    stmt->arrays.back().typeSuffix = mergeTypes(suffix, convertedType, varName);
                }
            } else if (current().type == TokenType::IDENTIFIER) {
                // User-defined type
                std::string userTypeName = current().value;
                advance();
                
                // Set user-defined type
                if (!stmt->arrays.empty()) {
                    stmt->setAsType(userTypeName);
                    
                    // Validate: if explicit suffix was given, it conflicts with user type
                    if (suffix != TokenType::UNKNOWN) {
                        error("Cannot use type suffix with user-defined type AS " + userTypeName);
                    }
                }
            } else {
                error("Expected type name after AS");
            }
        }

    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseRedimStatement() {
    auto stmt = std::make_unique<RedimStatement>();
    advance(); // consume REDIM

    // Check for PRESERVE keyword
    if (current().type == TokenType::PRESERVE) {
        stmt->preserve = true;
        advance();
    }

    // Parse array declarations (similar to DIM)
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected array name in REDIM statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string arrayName = parseVariableName(suffix);

        stmt->addArray(arrayName);

        // Array must have dimensions
        if (!match(TokenType::LPAREN)) {
            error("REDIM requires array dimensions");
            break;
        }

        // Parse dimensions
        do {
            stmt->addDimension(parseExpression());
        } while (match(TokenType::COMMA));

        consume(TokenType::RPAREN, "Expected ')' after array dimensions");

    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseEraseStatement() {
    auto stmt = std::make_unique<EraseStatement>();
    advance(); // consume ERASE

    // Parse array names
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected array name in ERASE statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string arrayName = parseVariableName(suffix);
        stmt->addArray(arrayName);

    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseSwapStatement() {
    advance(); // consume SWAP

    // Parse first variable
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected variable name after SWAP");
        return std::make_unique<RemStatement>("");
    }

    TokenType suffix1 = TokenType::UNKNOWN;
    std::string var1 = parseVariableName(suffix1);

    // Expect comma
    if (!match(TokenType::COMMA)) {
        error("Expected comma between variables in SWAP");
        return std::make_unique<RemStatement>("");
    }

    // Parse second variable
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected second variable name in SWAP");
        return std::make_unique<RemStatement>("");
    }

    TokenType suffix2 = TokenType::UNKNOWN;
    std::string var2 = parseVariableName(suffix2);

    return std::make_unique<SwapStatement>(var1, var2);
}

StatementPtr Parser::parseIncStatement() {
    advance(); // consume INC

    // Parse variable name
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected variable name after INC");
        return std::make_unique<RemStatement>("");
    }

    TokenType suffix = TokenType::UNKNOWN;
    std::string varName = parseVariableName(suffix);

    auto stmt = std::make_unique<IncStatement>(varName);

    // Check for array indices
    if (match(TokenType::LPAREN)) {
        do {
            stmt->addIndex(parseExpression());
        } while (match(TokenType::COMMA));

        consume(TokenType::RPAREN, "Expected ')' after array indices");
    }

    // Check for member access (e.g., P.X or P.Position.X)
    while (match(TokenType::DOT)) {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected member name after '.'");
            break;
        }
        stmt->addMember(current().value);
        advance();
    }

    // Check for optional increment value (comma-separated)
    if (match(TokenType::COMMA)) {
        stmt->incrementExpr = parseExpression();
    }

    return stmt;
}

StatementPtr Parser::parseDecStatement() {
    advance(); // consume DEC

    // Parse variable name
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected variable name after DEC");
        return std::make_unique<RemStatement>("");
    }

    TokenType suffix = TokenType::UNKNOWN;
    std::string varName = parseVariableName(suffix);

    auto stmt = std::make_unique<DecStatement>(varName);

    // Check for array indices
    if (match(TokenType::LPAREN)) {
        do {
            stmt->addIndex(parseExpression());
        } while (match(TokenType::COMMA));

        consume(TokenType::RPAREN, "Expected ')' after array indices");
    }

    // Check for member access (e.g., P.X or P.Position.X)
    while (match(TokenType::DOT)) {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected member name after '.'");
            break;
        }
        stmt->addMember(current().value);
        advance();
    }

    // Check for optional decrement value (comma-separated)
    if (match(TokenType::COMMA)) {
        stmt->decrementExpr = parseExpression();
    }

    return stmt;
}

StatementPtr Parser::parseTypeDeclarationStatement() {
    advance(); // consume TYPE
    
    // Expect type name
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected type name after TYPE");
        return std::make_unique<RemStatement>(""); // Return dummy statement
    }
    
    std::string typeName = current().value;
    advance(); // consume type name
    
    auto stmt = std::make_unique<TypeDeclarationStatement>(typeName);
    
    // Expect end of line
    if (current().type != TokenType::END_OF_LINE && current().type != TokenType::COLON) {
        error("Expected end of line after TYPE name");
    }
    skipToEndOfLine();
    
    // Parse fields until END TYPE or ENDTYPE
    while (current().type != TokenType::END_OF_FILE) {
        // Check for END TYPE or ENDTYPE
        if (current().type == TokenType::ENDTYPE) {
            advance(); // consume ENDTYPE
            break;
        }
        
        if (current().type == TokenType::END) {
            advance(); // consume END
            if (current().type == TokenType::TYPE) {
                advance(); // consume TYPE
                break;
            } else {
                error("Expected TYPE after END in type declaration");
                break;
            }
        }
        
        // Skip blank lines
        if (current().type == TokenType::END_OF_LINE) {
            advance();
            continue;
        }
        
        // Parse field: FieldName AS TypeName
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected field name in type declaration");
            skipToEndOfLine();
            continue;
        }
        
        std::string fieldName = current().value;
        advance(); // consume field name
        
        // Expect AS keyword
        if (current().type != TokenType::AS) {
            error("Expected AS after field name in type declaration");
            skipToEndOfLine();
            continue;
        }
        advance(); // consume AS
        
        // Parse type - can be built-in (INTEGER, DOUBLE, etc.) or user-defined (identifier)
        std::string fieldTypeName;
        TokenType builtInType = TokenType::UNKNOWN;
        bool isBuiltIn = false;
        
        if (isTypeKeyword(current().type)) {
            // Built-in type
            isBuiltIn = true;
            builtInType = current().type;
            fieldTypeName = current().value;
            advance();
        } else if (current().type == TokenType::IDENTIFIER) {
            // User-defined type
            isBuiltIn = false;
            fieldTypeName = current().value;
            advance();
        } else {
            error("Expected type name after AS in type declaration");
            skipToEndOfLine();
            continue;
        }
        
        // Add field to type declaration
        stmt->addField(fieldName, fieldTypeName, builtInType, isBuiltIn);
        
        // Expect end of line
        skipToEndOfLine();
    }
    
    return stmt;
}

StatementPtr Parser::parseLocalStatement() {
    auto stmt = std::make_unique<LocalStatement>();
    advance(); // consume LOCAL

    // Reserve capacity for common case (1-4 local variables)
    stmt->variables.reserve(4);

    // Parse local variable declarations (similar to DIM but for locals)
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in LOCAL statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);

        stmt->addVariable(varName, suffix);

        // Check for optional initialization (LOCAL x = 10)
        if (match(TokenType::EQUAL)) {
            stmt->setInitialValue(parseExpression());
        }

        // Check for AS type declaration
        if (current().type == TokenType::AS) {
            advance(); // consume AS
            
            // Check if it's a built-in type or user-defined type
            if (isTypeKeyword(current().type)) {
                // Built-in type keyword (INT, FLOAT, DOUBLE, STRING)
                TokenType asType = current().type;
                advance();
                
                // Convert AS type keyword to type suffix
                TokenType convertedType = asTypeToSuffix(asType);
                
                // Validate and merge types
                if (!stmt->variables.empty()) {
                    stmt->variables.back().typeSuffix = mergeTypes(suffix, convertedType, varName);
                }
            } else if (current().type == TokenType::IDENTIFIER) {
                // User-defined type
                std::string userTypeName = current().value;
                advance();
                
                // Set user-defined type
                if (!stmt->variables.empty()) {
                    stmt->variables.back().asTypeName = userTypeName;
                    stmt->variables.back().hasAsType = true;
                    
                    // Validate: if explicit suffix was given, it conflicts with user type
                    if (suffix != TokenType::UNKNOWN) {
                        error("Cannot use type suffix with user-defined type AS " + userTypeName);
                    }
                }
            } else {
                error("Expected type name after AS");
            }
        }

    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseSharedStatement() {
    auto stmt = std::make_unique<SharedStatement>();
    advance(); // consume SHARED

    // Reserve capacity for common case (1-4 shared variables)
    stmt->variables.reserve(4);

    // Parse shared variable list (similar to LOCAL but for module-level access)
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in SHARED statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);

        stmt->addVariable(varName, suffix);

        // Check for AS type declaration
        if (current().type == TokenType::AS) {
            advance(); // consume AS
            
            // Check if it's a built-in type or user-defined type
            if (isTypeKeyword(current().type)) {
                // Built-in type keyword (INT, FLOAT, DOUBLE, STRING)
                TokenType asType = current().type;
                advance();
                
                // Convert AS type keyword to type suffix
                TokenType convertedType = asTypeToSuffix(asType);
                
                // Validate and merge types
                if (!stmt->variables.empty()) {
                    stmt->variables.back().typeSuffix = mergeTypes(suffix, convertedType, varName);
                }
            } else if (current().type == TokenType::IDENTIFIER) {
                // User-defined type
                std::string userTypeName = current().value;
                advance();
                
                // Set user-defined type
                if (!stmt->variables.empty()) {
                    stmt->variables.back().asTypeName = userTypeName;
                    stmt->variables.back().hasAsType = true;
                    
                    // Validate: if explicit suffix was given, it conflicts with user type
                    if (suffix != TokenType::UNKNOWN) {
                        error("Cannot use type suffix with user-defined type AS " + userTypeName);
                    }
                }
            } else {
                error("Expected type name after AS");
            }
        }

    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseDataStatement() {
    auto stmt = std::make_unique<DataStatement>();
    advance(); // consume DATA

    // Reserve capacity for common case (5-10 data values)
    stmt->values.reserve(8);

    // Parse data values (numbers or strings)
    do {
        if (current().type == TokenType::STRING) {
            stmt->addValue(current().value);
            advance();
        } else if (current().type == TokenType::NUMBER) {
            stmt->addValue(current().value);
            advance();
        } else if (current().type == TokenType::IDENTIFIER) {
            // Unquoted string data
            stmt->addValue(current().value);
            advance();
        } else if (current().type == TokenType::MINUS) {
            // Negative number
            advance();
            if (current().type == TokenType::NUMBER) {
                stmt->addValue("-" + current().value);
                advance();
            } else {
                error("Expected number after '-' in DATA statement");
            }
        } else {
            error("Expected value in DATA statement");
            break;
        }
    } while (match(TokenType::COMMA));

    return stmt;
}

StatementPtr Parser::parseReadStatement() {
    auto stmt = std::make_unique<ReadStatement>();
    advance(); // consume READ

    // Reserve capacity for common case (1-4 variables)
    stmt->variables.reserve(4);

    // Parse variable list
    do {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in READ statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);
        stmt->addVariable(varName);

    } while (match(TokenType::COMMA));

    return stmt;
}



StatementPtr Parser::parseRestoreStatement() {
    advance(); // consume RESTORE

    // Check if it's a line number or symbolic label
    if (current().type == TokenType::NUMBER) {
        // RESTORE line_number
        int line = static_cast<int>(current().numberValue);
        advance();
        return std::make_unique<RestoreStatement>(line);
    } else if (current().type == TokenType::IDENTIFIER ||
               current().type == TokenType::COLON) {
        // RESTORE label or RESTORE :label
        if (current().type == TokenType::COLON) {
            advance(); // consume optional colon prefix
        }
        // Allow identifiers or keywords as label names
        std::string label = current().value;
        advance();
        return std::make_unique<RestoreStatement>(label);
    }

    // No line number or label - restore to beginning
    return std::make_unique<RestoreStatement>();
}

StatementPtr Parser::parseRemStatement() {
    advance(); // consume REM

    // Rest of line is comment - collect it into the comment map
    // This handles inline REM (REM after other statements on the same line)
    std::string comment;
    while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
        if (!comment.empty()) comment += " ";
        comment += current().value;
        advance();
    }

    // Store or append comment by line number
    // If there's already a comment for this line, append with a separator
    if (m_comments.find(m_currentLineNumber) != m_comments.end()) {
        m_comments[m_currentLineNumber] += " | " + comment;
    } else {
        m_comments[m_currentLineNumber] = comment;
    }

    // Return nullptr to indicate no statement should be added
    return nullptr;
}

// =============================================================================
// File I/O Statement Parsing
// =============================================================================

StatementPtr Parser::parseOpenStatement() {
    auto stmt = std::make_unique<OpenStatement>();
    advance(); // consume OPEN

    // Parse filename (must be a string expression)
    auto filenameExpr = parseExpression();
    // TODO: Extract string value from expression (for now, assume STRING literal)
    if (auto* strLit = dynamic_cast<StringExpression*>(filenameExpr.get())) {
        stmt->filename = strLit->value;
    } else {
        error("OPEN requires a string filename");
        return stmt;
    }

    // Expect FOR keyword
    if (!match(TokenType::FOR)) {
        error("Expected FOR after filename in OPEN statement");
        return stmt;
    }

    // Parse mode (INPUT, OUTPUT, APPEND, RANDOM)
    // INPUT is a keyword token, others are identifiers
    if (current().type == TokenType::INPUT) {
        stmt->mode = "INPUT";
        advance();
    } else if (current().type == TokenType::IDENTIFIER) {
        stmt->mode = current().value;
        advance();
    } else {
        error("Expected file mode (INPUT, OUTPUT, APPEND, RANDOM) after FOR");
        return stmt;
    }

    // Expect AS keyword
    if (!match(TokenType::AS)) {
        error("Expected AS after file mode in OPEN statement");
        return stmt;
    }

    // Expect # (HASH token)
    if (!match(TokenType::HASH)) {
        error("Expected # after AS in OPEN statement");
        return stmt;
    }

    // Parse file number
    if (current().type != TokenType::NUMBER) {
        error("Expected file number after # in OPEN statement");
        return stmt;
    }
    stmt->fileNumber = static_cast<int>(current().numberValue);
    advance();

    return stmt;
}

StatementPtr Parser::parseCloseStatement() {
    auto stmt = std::make_unique<CloseStatement>();
    advance(); // consume CLOSE

    // Check if we have a file number or close all
    if (current().type == TokenType::END_OF_LINE || current().type == TokenType::COLON) {
        // CLOSE with no arguments - close all files
        stmt->closeAll = true;
        return stmt;
    }

    // Expect # (HASH token)
    if (match(TokenType::HASH)) {
        // Parse file number
        if (current().type != TokenType::NUMBER) {
            error("Expected file number after # in CLOSE statement");
            return stmt;
        }
        stmt->fileNumber = static_cast<int>(current().numberValue);
        stmt->closeAll = false;
        advance();
    } else {
        error("Expected # and file number in CLOSE statement");
    }

    return stmt;
}

StatementPtr Parser::parsePrintStreamStatement() {
    auto stmt = std::make_unique<PrintStatement>();
    advance(); // consume PRINT#

    // Parse file number
    if (current().type != TokenType::NUMBER) {
        error("Expected file number after PRINT#");
        return stmt;
    }
    stmt->fileNumber = static_cast<int>(current().numberValue);
    advance();

    // Expect comma or semicolon separator
    if (!match(TokenType::COMMA) && !match(TokenType::SEMICOLON)) {
        error("Expected , or ; after file number in PRINT#");
        return stmt;
    }

    // Parse print items (same as regular PRINT)
    while (!isAtEnd() &&
           current().type != TokenType::END_OF_LINE &&
           current().type != TokenType::COLON) {

        auto expr = parseExpression();
        bool hasSemicolon = match(TokenType::SEMICOLON);
        bool hasComma = match(TokenType::COMMA);

        stmt->addItem(std::move(expr), hasSemicolon, hasComma);

        if (!hasSemicolon && !hasComma) {
            break;
        }
    }

    // Check if we should suppress newline
    if (!stmt->items.empty()) {
        const auto& lastItem = stmt->items.back();
        if (lastItem.semicolon || lastItem.comma) {
            stmt->trailingNewline = false;
        }
    }

    return stmt;
}

StatementPtr Parser::parseInputStreamStatement() {
    auto stmt = std::make_unique<InputStatement>();
    advance(); // consume INPUT#

    stmt->fileNumber = 1; // Will be set below

    // Parse file number
    if (current().type != TokenType::NUMBER) {
        error("Expected file number after INPUT#");
        return stmt;
    }
    stmt->fileNumber = static_cast<int>(current().numberValue);
    advance();

    // Expect comma separator
    if (!match(TokenType::COMMA)) {
        error("Expected , after file number in INPUT#");
        return stmt;
    }

    // Parse variable list
    while (!isAtEnd() &&
           current().type != TokenType::END_OF_LINE &&
           current().type != TokenType::COLON) {

        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in INPUT# statement");
            break;
        }

        TokenType suffix = TokenType::UNKNOWN;
        std::string varName = parseVariableName(suffix);
        stmt->addVariable(varName);

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    return stmt;
}

StatementPtr Parser::parseWriteStreamStatement() {
    auto stmt = std::make_unique<PrintStatement>();
    advance(); // consume WRITE#

    // Parse file number
    if (current().type != TokenType::NUMBER) {
        error("Expected file number after WRITE#");
        return stmt;
    }
    stmt->fileNumber = static_cast<int>(current().numberValue);
    advance();

    // Expect comma separator
    if (!match(TokenType::COMMA)) {
        error("Expected , after file number in WRITE#");
        return stmt;
    }

    // Parse write items (like PRINT but with different formatting)
    while (!isAtEnd() &&
           current().type != TokenType::END_OF_LINE &&
           current().type != TokenType::COLON) {

        auto expr = parseExpression();
        bool hasComma = match(TokenType::COMMA);

        stmt->addItem(std::move(expr), false, hasComma);

        if (!hasComma) {
            break;
        }
    }

    stmt->trailingNewline = true; // WRITE# always adds newline

    return stmt;
}

StatementPtr Parser::parseLineInputStreamStatement() {
    // LINE INPUT# has already been consumed (LINE and INPUT_STREAM tokens)
    auto stmt = std::make_unique<InputStatement>();
    stmt->isLineInput = true;

    // Parse file number
    if (current().type != TokenType::NUMBER) {
        error("Expected file number after LINE INPUT#");
        return stmt;
    }
    stmt->fileNumber = static_cast<int>(current().numberValue);
    advance();

    // Expect comma separator
    if (!match(TokenType::COMMA)) {
        error("Expected , after file number in LINE INPUT#");
        return stmt;
    }

    // Parse variable name
    if (current().type != TokenType::IDENTIFIER) {
        error("Expected variable name in LINE INPUT# statement");
        return stmt;
    }

    TokenType suffix = TokenType::UNKNOWN;
    std::string varName = parseVariableName(suffix);
    stmt->addVariable(varName);

    return stmt;
}

StatementPtr Parser::parseOptionStatement() {
    advance(); // consume OPTION

    if (match(TokenType::BITWISE)) {
        return std::make_unique<OptionStatement>(OptionStatement::OptionType::BITWISE);
    } else if (match(TokenType::LOGICAL)) {
        return std::make_unique<OptionStatement>(OptionStatement::OptionType::LOGICAL);
    } else if (match(TokenType::BASE)) {
        if (current().type != TokenType::NUMBER) {
            error("Expected number after OPTION BASE");
            return nullptr;
        }
        int base = static_cast<int>(current().numberValue);
        advance();
        if (base != 0 && base != 1) {
            error("OPTION BASE must be 0 or 1");
            return nullptr;
        }
        return std::make_unique<OptionStatement>(OptionStatement::OptionType::BASE, base);
    } else if (match(TokenType::EXPLICIT)) {
        return std::make_unique<OptionStatement>(OptionStatement::OptionType::EXPLICIT);
    } else if (match(TokenType::UNICODE)) {
        return std::make_unique<OptionStatement>(OptionStatement::OptionType::UNICODE);
    } else if (match(TokenType::ERROR)) {
        return std::make_unique<OptionStatement>(OptionStatement::OptionType::ERROR);
    } else if (match(TokenType::CANCELLABLE)) {
        // Parse ON/OFF for OPTION CANCELLABLE
        if (match(TokenType::ON)) {
            return std::make_unique<OptionStatement>(OptionStatement::OptionType::CANCELLABLE, 1);
        } else if (match(TokenType::OFF)) {
            return std::make_unique<OptionStatement>(OptionStatement::OptionType::CANCELLABLE, 0);
        } else {
            error("Expected ON or OFF after OPTION CANCELLABLE");
            return nullptr;
        }
    } else {
        error("Unknown OPTION type. Expected BITWISE, LOGICAL, BASE, EXPLICIT, UNICODE, ERROR, or CANCELLABLE");
        return nullptr;
    }
}

StatementPtr Parser::parseDefStatement() {
    advance(); // consume DEF

    consume(TokenType::FN, "Expected FN after DEF");

    if (current().type != TokenType::IDENTIFIER) {
        error("Expected function name after DEF FN");
        return nullptr;
    }

    std::string funcName = current().value;
    advance();

    auto stmt = std::make_unique<DefStatement>(funcName);

    consume(TokenType::LPAREN, "Expected '(' in DEF FN");

    // Parse parameter list
    if (current().type != TokenType::RPAREN) {
        do {
            if (current().type != TokenType::IDENTIFIER) {
                error("Expected parameter name in DEF FN");
                break;
            }
            stmt->addParameter(current().value);
            advance();
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RPAREN, "Expected ')' in DEF FN");

    consume(TokenType::EQUAL, "Expected '=' in DEF FN");

    stmt->body = parseExpression();

    return stmt;
}

StatementPtr Parser::parseFunctionStatement() {
    advance(); // consume FUNCTION

    // Allow keywords as function names (e.g., FUNCTION double(x))
    if (current().type != TokenType::IDENTIFIER &&
        current().type != TokenType::KEYWORD_DOUBLE &&
        current().type != TokenType::KEYWORD_INTEGER &&
        current().type != TokenType::KEYWORD_SINGLE &&
        current().type != TokenType::KEYWORD_STRING) {
        error("Expected function name after FUNCTION");
        return nullptr;
    }

    std::string funcName = current().value;
    TokenType returnType = TokenType::UNKNOWN;

    // Extract and mangle type suffix from function name
    if (!funcName.empty()) {
        char lastChar = funcName.back();
        switch (lastChar) {
            case '$':
                returnType = TokenType::TYPE_STRING;
                funcName.pop_back();
                funcName += "_STRING";
                break;
            case '%':
                returnType = TokenType::TYPE_INT;
                funcName.pop_back();
                funcName += "_INT";
                break;
            case '#':
                returnType = TokenType::TYPE_DOUBLE;
                funcName.pop_back();
                funcName += "_DOUBLE";
                break;
            case '!':
                returnType = TokenType::TYPE_FLOAT;
                funcName.pop_back();
                funcName += "_FLOAT";
                break;
            case '&':
                returnType = TokenType::TYPE_INT;
                funcName.pop_back();
                funcName += "_LONG";
                break;
        }
    }
    advance();

    auto stmt = std::make_unique<FunctionStatement>(funcName, returnType);

    consume(TokenType::LPAREN, "Expected '(' after function name");

    // Parse parameter list
    if (current().type != TokenType::RPAREN) {
        do {
            // Check for BYREF or BYVAL keyword
            bool isByRef = false;
            if (current().type == TokenType::BYREF) {
                isByRef = true;
                advance();
            } else if (current().type == TokenType::BYVAL) {
                isByRef = false;
                advance();
            }
            
            if (current().type != TokenType::IDENTIFIER) {
                error("Expected parameter name in FUNCTION");
                break;
            }
            std::string paramName = current().value;
            TokenType paramType = TokenType::UNKNOWN;
            std::string paramAsType = "";

            // Extract and mangle type suffix from parameter name
            if (!paramName.empty()) {
                char lastChar = paramName.back();
                switch (lastChar) {
                    case '$':
                        paramType = TokenType::TYPE_STRING;
                        paramName.pop_back();
                        paramName += "_STRING";
                        break;
                    case '%':
                        paramType = TokenType::TYPE_INT;
                        paramName.pop_back();
                        paramName += "_INT";
                        break;
                    case '#':
                        paramType = TokenType::TYPE_DOUBLE;
                        paramName.pop_back();
                        paramName += "_DOUBLE";
                        break;
                    case '!':
                        paramType = TokenType::TYPE_FLOAT;
                        paramName.pop_back();
                        paramName += "_FLOAT";
                        break;
                    case '&':
                        paramType = TokenType::TYPE_INT;
                        paramName.pop_back();
                        paramName += "_LONG";
                        break;
                }
            }
            advance();

            // Check for AS TypeName syntax
            if (current().type == TokenType::AS) {
                advance(); // consume AS
                
                if (isTypeKeyword(current().type)) {
                    // Built-in type keyword (INT, FLOAT, DOUBLE, STRING)
                    TokenType asType = current().type;
                    paramAsType = current().value;  // Store the keyword name
                    advance();
                    
                    // Convert AS type keyword to type suffix
                    TokenType convertedType = asTypeToSuffix(asType);
                    
                    // Validate: if explicit suffix was given, it should match
                    if (paramType != TokenType::UNKNOWN && paramType != convertedType) {
                        error("Type suffix conflicts with AS type declaration for parameter " + paramName);
                    }
                    paramType = convertedType;
                    
                } else if (current().type == TokenType::IDENTIFIER) {
                    // User-defined type
                    paramAsType = current().value;
                    advance();
                    
                    // Validate: user-defined types can't have type suffixes
                    if (paramType != TokenType::UNKNOWN) {
                        error("Cannot use type suffix with user-defined type AS " + paramAsType);
                    }
                } else {
                    error("Expected type name after AS in parameter declaration");
                }
            }

            stmt->addParameter(paramName, paramType, isByRef, paramAsType);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RPAREN, "Expected ')' after parameter list");

    // Check for AS TypeName for return type
    if (current().type == TokenType::AS) {
        advance(); // consume AS
        
        if (isTypeKeyword(current().type)) {
            // Built-in type keyword
            TokenType asType = current().type;
            stmt->returnTypeAsName = current().value;
            stmt->hasReturnAsType = true;
            advance();
            
            // Convert AS type keyword to type suffix
            TokenType convertedType = asTypeToSuffix(asType);
            
            // Validate: if explicit suffix was given, it should match
            if (returnType != TokenType::UNKNOWN && returnType != convertedType) {
                error("Type suffix conflicts with AS type declaration for function " + funcName);
            }
            stmt->returnTypeSuffix = convertedType;
            
        } else if (current().type == TokenType::IDENTIFIER) {
            // User-defined type as return type
            stmt->returnTypeAsName = current().value;
            stmt->hasReturnAsType = true;
            advance();
            
            // Validate: user-defined types can't have type suffixes
            if (returnType != TokenType::UNKNOWN) {
                error("Cannot use type suffix with user-defined return type AS " + stmt->returnTypeAsName);
            }
        } else {
            error("Expected type name after AS in function declaration");
        }
    }

    // Expect end of line after FUNCTION declaration
    if (current().type != TokenType::END_OF_LINE && current().type != TokenType::END_OF_FILE) {
        skipToEndOfLine();
    }

    // Parse function body until END FUNCTION or ENDFUNCTION
    while (!isAtEnd()) {
        skipBlankLines();

        if (isAtEnd()) break;

        // Skip optional line number at start of line
        skipOptionalLineNumber();

        // Check for END FUNCTION or ENDFUNCTION
        if (current().type == TokenType::ENDFUNCTION) {
            advance();
            break;
        }

        if (current().type == TokenType::END) {
            advance();
            if (current().type == TokenType::FUNCTION) {
                advance();
                break;
            }
            // Put END back and let it be parsed as a statement
            m_currentIndex--;
        }

        auto bodyStmt = parseStatement();
        if (bodyStmt) {
            stmt->addStatement(std::move(bodyStmt));
        }

        // Skip end of line after statement
        if (current().type == TokenType::END_OF_LINE) {
            advance();
        }
    }

    return stmt;
}

StatementPtr Parser::parseSubStatement() {
    advance(); // consume SUB

    // Allow keywords as subroutine names
    if (current().type != TokenType::IDENTIFIER &&
        current().type != TokenType::KEYWORD_DOUBLE &&
        current().type != TokenType::KEYWORD_INTEGER &&
        current().type != TokenType::KEYWORD_SINGLE &&
        current().type != TokenType::KEYWORD_STRING) {
        error("Expected subroutine name after SUB");
        return nullptr;
    }

    std::string subName = current().value;
    advance();

    auto stmt = std::make_unique<SubStatement>(subName);

    consume(TokenType::LPAREN, "Expected '(' after subroutine name");

    // Parse parameter list
    if (current().type != TokenType::RPAREN) {
        do {
            // Check for BYREF or BYVAL keyword
            bool isByRef = false;
            if (current().type == TokenType::BYREF) {
                isByRef = true;
                advance();
            } else if (current().type == TokenType::BYVAL) {
                isByRef = false;
                advance();
            }
            
            if (current().type != TokenType::IDENTIFIER) {
                error("Expected parameter name in SUB");
                break;
            }
            std::string paramName = current().value;
            TokenType paramType = TokenType::UNKNOWN;
            std::string paramAsType = "";

            // Extract and mangle type suffix from parameter name
            if (!paramName.empty()) {
                char lastChar = paramName.back();
                switch (lastChar) {
                    case '$':
                        paramType = TokenType::TYPE_STRING;
                        paramName.pop_back();
                        paramName += "_STRING";
                        break;
                    case '%':
                        paramType = TokenType::TYPE_INT;
                        paramName.pop_back();
                        paramName += "_INT";
                        break;
                    case '#':
                        paramType = TokenType::TYPE_DOUBLE;
                        paramName.pop_back();
                        paramName += "_DOUBLE";
                        break;
                    case '!':
                        paramType = TokenType::TYPE_FLOAT;
                        paramName.pop_back();
                        paramName += "_FLOAT";
                        break;
                    case '&':
                        paramType = TokenType::TYPE_INT;
                        paramName.pop_back();
                        paramName += "_LONG";
                        break;
                }
            }
            advance();

            // Check for AS TypeName syntax
            if (current().type == TokenType::AS) {
                advance(); // consume AS
                
                if (isTypeKeyword(current().type)) {
                    // Built-in type keyword (INT, FLOAT, DOUBLE, STRING)
                    TokenType asType = current().type;
                    paramAsType = current().value;  // Store the keyword name
                    advance();
                    
                    // Convert AS type keyword to type suffix
                    TokenType convertedType = asTypeToSuffix(asType);
                    
                    // Validate: if explicit suffix was given, it should match
                    if (paramType != TokenType::UNKNOWN && paramType != convertedType) {
                        error("Type suffix conflicts with AS type declaration for parameter " + paramName);
                    }
                    paramType = convertedType;
                    
                } else if (current().type == TokenType::IDENTIFIER) {
                    // User-defined type
                    paramAsType = current().value;
                    advance();
                    
                    // Validate: user-defined types can't have type suffixes
                    if (paramType != TokenType::UNKNOWN) {
                        error("Cannot use type suffix with user-defined type AS " + paramAsType);
                    }
                } else {
                    error("Expected type name after AS in parameter declaration");
                }
            }

            stmt->addParameter(paramName, paramType, isByRef, paramAsType);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RPAREN, "Expected ')' after parameter list");

    // Expect end of line after SUB declaration
    if (current().type != TokenType::END_OF_LINE && current().type != TokenType::END_OF_FILE) {
        skipToEndOfLine();
    }

    // Parse subroutine body until END SUB or ENDSUB
    while (!isAtEnd()) {
        skipBlankLines();

        if (isAtEnd()) break;

        // Skip optional line number at start of line
        skipOptionalLineNumber();

        // Check for END SUB or ENDSUB
        if (current().type == TokenType::ENDSUB) {
            advance();
            break;
        }

        if (current().type == TokenType::END) {
            advance();
            if (current().type == TokenType::SUB) {
                advance();
                break;
            }
            // Put END back and let it be parsed as a statement
            m_currentIndex--;
        }

        auto bodyStmt = parseStatement();
        if (bodyStmt) {
            stmt->addStatement(std::move(bodyStmt));
        }

        // Skip end of line after statement
        if (current().type == TokenType::END_OF_LINE) {
            advance();
        }
    }

    return stmt;
}

StatementPtr Parser::parseCallStatement() {
    advance(); // consume CALL

    // Allow both identifiers and keywords (like RECTF, CIRCLE, etc.) as subroutine names
    if (current().type != TokenType::IDENTIFIER &&
        current().type != TokenType::RECT &&
        current().type != TokenType::CIRCLEF &&
        current().type != TokenType::CIRCLE &&
        current().type != TokenType::LINE &&
        current().type != TokenType::PSET &&
        current().type != TokenType::CLS) {
        error("Expected subroutine name after CALL");
        return nullptr;
    }

    std::string subName = current().value;
    advance();

    auto stmt = std::make_unique<CallStatement>(subName);

    // Optional parentheses for CALL
    if (current().type == TokenType::LPAREN) {
        advance();

        // Parse argument list
        if (current().type != TokenType::RPAREN) {
            do {
                auto arg = parseExpression();
                if (arg) {
                    stmt->addArgument(std::move(arg));
                }
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RPAREN, "Expected ')' after arguments");
    }

    return stmt;
}

// =============================================================================
// Graphics and Sound Statements
// =============================================================================

StatementPtr Parser::parseClsStatement() {
    advance(); // consume CLS
    return std::make_unique<SimpleStatement>(ASTNodeType::STMT_CLS, "CLS");
}

StatementPtr Parser::parseGclsStatement() {
    advance(); // consume GCLS
    return std::make_unique<SimpleStatement>(ASTNodeType::STMT_GCLS, "GCLS");
}

StatementPtr Parser::parseColorStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_COLOR, "COLOR");
    advance(); // consume COLOR

    // Parse color arguments (typically 1 or 2: foreground, background)
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());
    }

    return stmt;
}

StatementPtr Parser::parseWaitStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_WAIT, "WAIT");
    advance(); // consume WAIT

    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseWaitMsStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_WAIT_MS, "WAIT_MS");
    advance(); // consume WAIT_MS

    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parsePlayStatement() {
    advance(); // consume PLAY

    // Parse the filename expression
    auto filename = parseExpression();
    if (!filename) {
        error("Expected filename expression after PLAY");
        return nullptr;
    }

    auto stmt = std::make_unique<PlayStatement>(std::move(filename));

    // Check for optional AS clause
    if (match(TokenType::AS)) {
        // Parse format type (identifier or string)
        if (current().type == TokenType::IDENTIFIER || current().type == TokenType::STRING) {
            std::string format = current().value;
            // Convert to lowercase for consistency
            std::transform(format.begin(), format.end(), format.begin(), ::tolower);

            // Validate format
            if (format != "abc" && format != "sid" && format != "wav" && format != "voicescript") {
                error("Invalid format '" + format + "'. Valid formats: abc, sid, wav, voicescript");
                return nullptr;
            }

            stmt->format = format;
            stmt->hasFormat = true;
            advance(); // consume format
        } else {
            error("Expected format type after AS (abc, sid, wav, or voicescript)");
            return nullptr;
        }
    }

    // Check for optional INTO_WAV clause
    if (current().type == TokenType::IDENTIFIER && current().value == "INTO_WAV") {
        advance(); // consume INTO_WAV

        // Parse the output WAV filename expression
        auto wavOutput = parseExpression();
        if (!wavOutput) {
            error("Expected filename expression after INTO_WAV");
            return nullptr;
        }

        stmt->wavOutput = std::move(wavOutput);
        stmt->hasWavOutput = true;
    }

    // Check for optional INTO_SLOT clause
    if (current().type == TokenType::IDENTIFIER && current().value == "INTO_SLOT") {
        advance(); // consume INTO_SLOT

        // Parse the slot number expression
        auto slotNumber = parseExpression();
        if (!slotNumber) {
            error("Expected slot number expression after INTO_SLOT");
            return nullptr;
        }

        stmt->slotNumber = std::move(slotNumber);
        stmt->hasSlot = true;
    }

    // Check for optional FAST clause
    if (current().type == TokenType::IDENTIFIER && current().value == "FAST") {
        advance(); // consume FAST
        stmt->fastRender = true;
    }

    return stmt;
}

StatementPtr Parser::parsePlaySoundStatement() {
    advance(); // consume PLAY_SOUND

    // PLAY_SOUND sound_id, volume [, cap_duration]

    // Parse sound ID
    auto soundId = parseExpression();
    if (!soundId) {
        error("Expected sound ID after PLAY_SOUND");
        return nullptr;
    }

    // Expect comma
    if (!match(TokenType::COMMA)) {
        error("Expected comma after sound ID");
        return nullptr;
    }

    // Parse volume
    auto volume = parseExpression();
    if (!volume) {
        error("Expected volume after comma");
        return nullptr;
    }

    auto stmt = std::make_unique<PlaySoundStatement>(std::move(soundId), std::move(volume));

    // Optional: cap duration
    if (match(TokenType::COMMA)) {
        auto capDuration = parseExpression();
        if (!capDuration) {
            error("Expected cap duration after comma");
            return nullptr;
        }
        stmt->capDuration = std::move(capDuration);
        stmt->hasCapDuration = true;
    }

    return stmt;
}

StatementPtr Parser::parsePsetStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_PSET, "PSET");
    advance(); // consume PSET

    // PSET x, y [, color]
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());

        if (match(TokenType::COMMA)) {
            stmt->addArgument(parseExpression());
        }
    }

    return stmt;
}

StatementPtr Parser::parseLineStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_LINE, "LINE");
    advance(); // consume LINE

    // LINE x1, y1, x2, y2 [, color [, thickness]]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in LINE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in LINE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in LINE statement");
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());

        // Optional thickness parameter
        if (match(TokenType::COMMA)) {
            stmt->addArgument(parseExpression());
        }
    }

    return stmt;
}

StatementPtr Parser::parseRectStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_RECT, "RECT");
    advance(); // consume RECT

    // RECT x, y, width, height [, color [, thickness]]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in RECT statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in RECT statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in RECT statement");
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());

        // Optional thickness parameter
        if (match(TokenType::COMMA)) {
            stmt->addArgument(parseExpression());
        }
    }

    return stmt;
}



StatementPtr Parser::parseCircleStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_CIRCLE, "CIRCLE");
    advance(); // consume CIRCLE

    // CIRCLE x, y, radius [, color [, thickness]]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in CIRCLE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in CIRCLE statement");
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());

        // Optional thickness parameter
        if (match(TokenType::COMMA)) {
            stmt->addArgument(parseExpression());
        }
    }

    return stmt;
}

StatementPtr Parser::parseCirclefStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_CIRCLEF, "CIRCLEF");
    advance(); // consume CIRCLEF

    // CIRCLEF x, y, radius [, color]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in CIRCLEF statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in CIRCLEF statement");
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());
    }

    return stmt;
}

// =============================================================================
// Expression Parsing (with operator precedence)
// =============================================================================

ExpressionPtr Parser::parseExpression() {
    return parseLogicalImp();
}

ExpressionPtr Parser::parseLogicalImp() {
    auto expr = parseLogicalEqv();

    while (match(TokenType::IMP)) {
        TokenType op = TokenType::IMP;
        auto right = parseLogicalEqv();
        expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
    }

    return expr;
}

ExpressionPtr Parser::parseLogicalEqv() {
    auto expr = parseLogicalOr();

    while (match(TokenType::EQV)) {
        TokenType op = TokenType::EQV;
        auto right = parseLogicalOr();
        expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
    }

    return expr;
}

ExpressionPtr Parser::parseLogicalOr() {
    auto expr = parseLogicalXor();

    while (match(TokenType::OR)) {
        TokenType op = TokenType::OR;
        auto right = parseLogicalXor();
        expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
    }

    return expr;
}

ExpressionPtr Parser::parseLogicalXor() {
    auto expr = parseLogicalAnd();

    while (match(TokenType::XOR)) {
        TokenType op = TokenType::XOR;
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
    }

    return expr;
}

ExpressionPtr Parser::parseLogicalAnd() {
    auto expr = parseLogicalNot();

    while (match(TokenType::AND)) {
        TokenType op = TokenType::AND;
        auto right = parseLogicalNot();
        expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
    }

    return expr;
}

ExpressionPtr Parser::parseLogicalNot() {
    if (match(TokenType::NOT)) {
        auto expr = parseLogicalNot();
        return std::make_unique<UnaryExpression>(TokenType::NOT, std::move(expr));
    }

    return parseComparison();
}

ExpressionPtr Parser::parseComparison() {
    auto expr = parseAdditive();

    while (true) {
        TokenType op = current().type;

        if (match({TokenType::EQUAL, TokenType::NOT_EQUAL,
                   TokenType::LESS_THAN, TokenType::LESS_EQUAL,
                   TokenType::GREATER_THAN, TokenType::GREATER_EQUAL})) {
            auto right = parseAdditive();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        } else {
            break;
        }
    }

    return expr;
}

ExpressionPtr Parser::parseAdditive() {
    auto expr = parseMultiplicative();

    while (true) {
        TokenType op = current().type;

        if (match({TokenType::PLUS, TokenType::MINUS})) {
            auto right = parseMultiplicative();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        } else {
            break;
        }
    }

    return expr;
}

ExpressionPtr Parser::parseMultiplicative() {
    auto expr = parseUnary();

    while (true) {
        TokenType op = current().type;

        if (match({TokenType::MULTIPLY, TokenType::DIVIDE, TokenType::INT_DIVIDE, TokenType::MOD})) {
            auto right = parseUnary();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        } else {
            break;
        }
    }

    return expr;
}

ExpressionPtr Parser::parseUnary() {
    if (match({TokenType::MINUS, TokenType::PLUS})) {
        TokenType op = (*m_tokens)[m_currentIndex - 1].type;
        auto expr = parseUnary();
        return std::make_unique<UnaryExpression>(op, std::move(expr));
    }

    return parsePower();
}

ExpressionPtr Parser::parsePower() {
    auto expr = parsePostfix();

    // Right-associative: 2^3^4 = 2^(3^4)
    if (match(TokenType::POWER)) {
        auto right = parsePower();
        expr = std::make_unique<BinaryExpression>(std::move(expr), TokenType::POWER, std::move(right));
    }

    return expr;
}

ExpressionPtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    // Handle member access (dot notation)
    while (match(TokenType::DOT)) {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected member name after '.'");
            break;
        }
        
        std::string memberName = current().value;
        advance();
        
        expr = std::make_unique<MemberAccessExpression>(std::move(expr), memberName);
    }

    return expr;
}

ExpressionPtr Parser::parsePrimary() {
    // Number literal
    if (current().type == TokenType::NUMBER) {
        auto expr = std::make_unique<NumberExpression>(current().numberValue);
        advance();
        return expr;
    }

    // String literal
    if (current().type == TokenType::STRING) {
        auto expr = std::make_unique<StringExpression>(current().value);
        advance();
        return expr;
    }

    // Registry function call (but check if it's a constant first!)
    if (current().type == TokenType::REGISTRY_FUNCTION) {
        // Fast constant check: if we have a ConstantsManager, check if this is actually a constant
        // Constants should be resolved before function calls for speed and correctness
        if (m_constantsManager && m_constantsManager->hasConstant(current().value)) {
            // This is a constant, not a function - treat it as a variable reference
            // The semantic analyzer will resolve it to the actual constant value
            std::string name = current().value;
            advance();
            return std::make_unique<VariableExpression>(name, TokenType::UNKNOWN);
        }
        
        // Not a constant, proceed with function call parsing
        return parseRegistryFunctionExpression();
    }

    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    // FN function call
    if (match(TokenType::FN)) {
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected function name after FN");
            return std::make_unique<NumberExpression>(0);
        }

        std::string funcName = current().value;
        advance();

        auto call = std::make_unique<FunctionCallExpression>(funcName, true);

        if (match(TokenType::LPAREN)) {
            if (current().type != TokenType::RPAREN) {
                do {
                    call->addArgument(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after function arguments");
        }

        return call;
    }

    // IIF (Immediate IF) function - inline conditional expression
    if (match(TokenType::IIF)) {
        consume(TokenType::LPAREN, "Expected '(' after IIF");
        
        // Parse condition
        auto condition = parseExpression();
        
        consume(TokenType::COMMA, "Expected ',' after IIF condition");
        
        // Parse true value
        auto trueValue = parseExpression();
        
        consume(TokenType::COMMA, "Expected ',' after IIF true value");
        
        // Parse false value
        auto falseValue = parseExpression();
        
        consume(TokenType::RPAREN, "Expected ')' after IIF false value");
        
        return std::make_unique<IIFExpression>(std::move(condition), std::move(trueValue), std::move(falseValue));
    }

    // Variable, array access, or function call
    // Also allow type keywords as function/variable names (e.g., FUNCTION double(x))
    if (current().type == TokenType::IDENTIFIER ||
        current().type == TokenType::KEYWORD_DOUBLE ||
        current().type == TokenType::KEYWORD_INTEGER ||
        current().type == TokenType::KEYWORD_SINGLE ||
        current().type == TokenType::KEYWORD_STRING) {

        std::string name;
        TokenType suffix = TokenType::UNKNOWN;

        // Handle type keywords specially - they don't have suffixes
        if (current().type == TokenType::KEYWORD_DOUBLE ||
            current().type == TokenType::KEYWORD_INTEGER ||
            current().type == TokenType::KEYWORD_SINGLE ||
            current().type == TokenType::KEYWORD_STRING) {
            name = current().value;
            advance();
        } else {
            name = parseVariableName(suffix);
        }

        // FAST CONSTANT CHECK: Check if this identifier is a constant BEFORE treating as function/variable/array
        // This allows case-insensitive constant lookup (pi, PI, Pi all work)
        if (m_constantsManager && m_constantsManager->hasConstant(name)) {
            // This is a constant - treat it as a simple variable reference
            // The semantic analyzer will resolve it to the actual constant value
            return std::make_unique<VariableExpression>(name, suffix);
        }

        // Check for array access or function call
        if (match(TokenType::LPAREN)) {
            // Check if this is a known user-defined function
            if (m_userDefinedFunctions.find(name) != m_userDefinedFunctions.end()) {
                // This is a user-defined function call
                auto call = std::make_unique<FunctionCallExpression>(name, false);

                if (current().type != TokenType::RPAREN) {
                    do {
                        call->addArgument(parseExpression());
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RPAREN, "Expected ')' after function arguments");
                return call;
            }

            // Otherwise, it's array access
            auto arrayAccess = std::make_unique<ArrayAccessExpression>(name, suffix);

            if (current().type != TokenType::RPAREN) {
                do {
                    arrayAccess->addIndex(parseExpression());
                } while (match(TokenType::COMMA));
            }

            consume(TokenType::RPAREN, "Expected ')' after array indices");
            return arrayAccess;
        }

        // Simple variable reference
        return std::make_unique<VariableExpression>(name, suffix);
    }

    error("Expected expression, got: " + current().toString());
    return std::make_unique<NumberExpression>(0);
}

ExpressionPtr Parser::parseRegistryFunctionExpression() {
    // Get the function name from the current token
    std::string functionName = current().value;
    advance(); // consume the function token

    // Ensure the global registry is initialized
    FasterBASIC::ModularCommands::initializeGlobalRegistry();

    // Get the function definition from the registry
    auto& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    const auto* functionDef = registry.getFunction(functionName);

    if (!functionDef) {
        error("Unknown registry function: " + functionName);
        return std::make_unique<NumberExpression>(0);
    }

    // Create a registry function expression
    auto funcExpr = std::make_unique<RegistryFunctionExpression>(functionName, functionDef->returnType);

    // Parse parameters according to the function definition
    size_t requiredParams = functionDef->getRequiredParameterCount();
    size_t totalParams = functionDef->getTotalParameterCount();

    // Helper function to validate parameter type at parse time
    auto validateParameterType = [&](const ExpressionPtr& expr, const FasterBASIC::ModularCommands::ParameterDefinition& paramDef, size_t paramIndex) {
        using ParamType = FasterBASIC::ModularCommands::ParameterType;

        // Check basic type compatibility based on expression type
        switch (paramDef.type) {
            case ParamType::INT:
            case ParamType::COLOR:
                if (dynamic_cast<StringExpression*>(expr.get())) {
                    error("Parameter " + std::to_string(paramIndex + 1) + " of " + functionName +
                         " ('" + paramDef.name + "') expects " +
                         FasterBASIC::ModularCommands::parameterTypeToString(paramDef.type) +
                         " but got string");
                }
                break;

            case ParamType::FLOAT:
                if (dynamic_cast<StringExpression*>(expr.get())) {
                    error("Parameter " + std::to_string(paramIndex + 1) + " of " + functionName +
                         " ('" + paramDef.name + "') expects " +
                         FasterBASIC::ModularCommands::parameterTypeToString(paramDef.type) +
                         " but got string");
                }
                break;

            case ParamType::STRING:
                // Strings can accept most expressions (they'll be converted)
                break;

            case ParamType::BOOL:
                // For now, accept numeric and boolean expressions
                if (dynamic_cast<StringExpression*>(expr.get())) {
                    // Check if it's a valid boolean string literal
                    auto strExpr = static_cast<StringExpression*>(expr.get());
                    std::string value = strExpr->value;
                    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                    if (value != "true" && value != "false") {
                        error("Parameter " + std::to_string(paramIndex + 1) + " of " + functionName +
                             " ('" + paramDef.name + "') expects boolean but got invalid string '" +
                             strExpr->value + "'");
                    }
                }
                break;

            case ParamType::OPTIONAL:
                break; // Optional is a modifier, not a type
        }
    };

    if (match(TokenType::LPAREN)) {
        // Parse parameters
        if (totalParams > 0) {
            // Parse first parameter
            auto expr = parseExpression();
            if (functionDef->parameters.size() > 0) {
                validateParameterType(expr, functionDef->parameters[0], 0);
            }
            funcExpr->addArgument(std::move(expr));

            // Parse remaining parameters
            for (size_t i = 1; i < totalParams; i++) {
                const auto& paramDef = functionDef->parameters[i];

                if (match(TokenType::COMMA)) {
                    auto paramExpr = parseExpression();
                    validateParameterType(paramExpr, paramDef, i);
                    funcExpr->addArgument(std::move(paramExpr));
                } else if (!paramDef.isOptional) {
                    error("Expected ',' in " + functionName + " function call - missing parameter '" + paramDef.name + "'");
                    break;
                } else {
                    // Add default value for optional parameter
                    if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::INT) {
                        int defaultVal = paramDef.defaultValue.empty() ? 0 : std::stoi(paramDef.defaultValue);
                        funcExpr->addArgument(std::make_unique<NumberExpression>(defaultVal));
                    } else if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::COLOR) {
                        uint32_t defaultVal = paramDef.defaultValue.empty() ? 0xFF000000 :
                            FasterBASIC::ModularCommands::parseColorValue(paramDef.defaultValue);
                        funcExpr->addArgument(std::make_unique<NumberExpression>(defaultVal));
                    } else if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::STRING) {
                        std::string defaultVal = paramDef.defaultValue.empty() ? "" : paramDef.defaultValue;
                        funcExpr->addArgument(std::make_unique<StringExpression>(defaultVal));
                    } else {
                        // Default numeric value
                        funcExpr->addArgument(std::make_unique<NumberExpression>(0));
                    }
                }
            }
        }

        consume(TokenType::RPAREN, "Expected ')' after function arguments");
    } else if (requiredParams > 0) {
        error("Registry function " + functionName + " requires parentheses and parameters");
    }

    return funcExpr;
}

// =============================================================================
// Helper Functions
// =============================================================================

bool Parser::isStartOfExpression() const {
    TokenType type = current().type;
    return type == TokenType::NUMBER ||
           type == TokenType::STRING ||
           type == TokenType::IDENTIFIER ||
           type == TokenType::LPAREN ||
           type == TokenType::MINUS ||
           type == TokenType::PLUS ||
           type == TokenType::NOT ||
           type == TokenType::FN;
}

bool Parser::isStartOfStatement() const {
    TokenType type = current().type;
    return type == TokenType::PRINT ||
           type == TokenType::CONSOLE ||
           type == TokenType::INPUT ||
           type == TokenType::LET ||
           type == TokenType::GOTO ||
           type == TokenType::GOSUB ||
           type == TokenType::ON ||
           type == TokenType::CONSTANT ||
           type == TokenType::IF ||
           type == TokenType::FOR ||
           type == TokenType::WHILE ||
           type == TokenType::REPEAT ||
           type == TokenType::DIM ||
           type == TokenType::REM ||
           (type == TokenType::IDENTIFIER && m_allowImplicitLet);
}

bool Parser::isAssignment() const {
    // Look ahead to see if this is an assignment
    // identifier = expr  or  identifier(...) = expr  or  identifier.member = expr

    size_t lookAhead = m_currentIndex + 1;

    // Skip type suffix if present
    if (lookAhead < m_tokens->size()) {
        TokenType type = (*m_tokens)[lookAhead].type;
        if (type == TokenType::TYPE_INT || type == TokenType::TYPE_STRING ||
            type == TokenType::TYPE_FLOAT || type == TokenType::TYPE_DOUBLE) {
            lookAhead++;
        }
    }

    // Check for array indices
    if (lookAhead < m_tokens->size() && (*m_tokens)[lookAhead].type == TokenType::LPAREN) {
        // Skip to matching )
        int depth = 1;
        lookAhead++;
        while (lookAhead < m_tokens->size() && depth > 0) {
            if ((*m_tokens)[lookAhead].type == TokenType::LPAREN) depth++;
            if ((*m_tokens)[lookAhead].type == TokenType::RPAREN) depth--;
            lookAhead++;
        }
    }

    // Check for member access (dot notation)
    while (lookAhead < m_tokens->size() && (*m_tokens)[lookAhead].type == TokenType::DOT) {
        lookAhead++; // skip DOT
        // Skip member name
        if (lookAhead < m_tokens->size() && (*m_tokens)[lookAhead].type == TokenType::IDENTIFIER) {
            lookAhead++;
        } else {
            break; // malformed member access, but not our problem here
        }
    }

    // Now check for =
    if (lookAhead < m_tokens->size()) {
        return (*m_tokens)[lookAhead].type == TokenType::EQUAL;
    }

    return false;
}

TokenType Parser::peekTypeSuffix() const {
    if (m_currentIndex + 1 < m_tokens->size()) {
        TokenType type = (*m_tokens)[m_currentIndex + 1].type;
        if (type == TokenType::TYPE_INT || type == TokenType::TYPE_STRING ||
            type == TokenType::TYPE_FLOAT || type == TokenType::TYPE_DOUBLE) {
            return type;
        }
    }
    return TokenType::UNKNOWN;
}

bool Parser::isTypeKeyword(TokenType type) const {
    return type == TokenType::KEYWORD_INTEGER || type == TokenType::KEYWORD_DOUBLE ||
           type == TokenType::KEYWORD_SINGLE || type == TokenType::KEYWORD_STRING ||
           type == TokenType::KEYWORD_LONG;
}

// Convert AS type keyword to equivalent type suffix
TokenType Parser::asTypeToSuffix(TokenType asType) const {
    switch (asType) {
        case TokenType::KEYWORD_INTEGER: return TokenType::TYPE_INT;
        case TokenType::KEYWORD_DOUBLE:  return TokenType::TYPE_DOUBLE;
        case TokenType::KEYWORD_SINGLE:  return TokenType::TYPE_FLOAT;
        case TokenType::KEYWORD_STRING:  return TokenType::TYPE_STRING;
        case TokenType::KEYWORD_LONG:    return TokenType::TYPE_INT; // Treat LONG as INT for now
        default: return TokenType::UNKNOWN;
    }
}

TokenType Parser::parseAsType() {
    // Check if we have AS keyword followed by a type
    if (current().type != TokenType::AS) {
        return TokenType::UNKNOWN;
    }

    advance(); // consume AS

    // Expect a type keyword
    if (isTypeKeyword(current().type)) {
        TokenType asType = current().type;
        advance(); // consume type keyword
        // Convert to equivalent suffix token
        return asTypeToSuffix(asType);
    }

    error("Expected type name (INTEGER, DOUBLE, SINGLE, STRING, LONG) after AS");
    return TokenType::UNKNOWN;
}

// Validate and merge type suffix with AS type declaration
TokenType Parser::mergeTypes(TokenType suffix, TokenType asType, const std::string& varName) {
    // No AS type specified, use suffix (or UNKNOWN)
    if (asType == TokenType::UNKNOWN) {
        return suffix;
    }

    // No suffix specified, use AS type
    if (suffix == TokenType::UNKNOWN) {
        return asType;
    }

    // Both specified - they must match
    if (suffix == asType) {
        return suffix; // Redundant but allowed
    }

    // Conflict - report error
    std::ostringstream oss;
    oss << "Type suffix '" << tokenTypeToString(suffix)
        << "' conflicts with AS " << tokenTypeToString(asType)
        << " for variable '" << varName << "'";
    error(oss.str());

    // Return suffix (it wins in case of conflict)
    return suffix;
}

std::string Parser::parseVariableName(TokenType& outSuffix) {
    const std::string& tokenValue = current().value;
    advance();

    // Check for type suffix in the identifier itself (e.g., x$, n%, value#)
    // Mangle the name immediately instead of stripping the suffix
    outSuffix = TokenType::UNKNOWN;

    if (!tokenValue.empty()) {
        char lastChar = tokenValue.back();

        // Fast path: no suffix - just return the token value
        if (lastChar != '$' && lastChar != '%' && lastChar != '#' &&
            lastChar != '!' && lastChar != '&') {
            // Also check if next token is a separate type suffix (alternative syntax)
            if (current().type == TokenType::TYPE_INT ||
                current().type == TokenType::TYPE_STRING ||
                current().type == TokenType::TYPE_FLOAT ||
                current().type == TokenType::TYPE_DOUBLE) {
                outSuffix = current().type;
                advance();
            }
            return tokenValue;
        }

        // Suffix path: pre-calculate final size to avoid reallocations
        size_t baseLen = tokenValue.size() - 1;  // without suffix char
        const char* suffixStr;
        size_t suffixLen;

        switch (lastChar) {
            case '$':
                outSuffix = TokenType::TYPE_STRING;
                suffixStr = "_STRING";
                suffixLen = 7;
                break;
            case '%':
                outSuffix = TokenType::TYPE_INT;
                suffixStr = "_INT";
                suffixLen = 4;
                break;
            case '#':
                outSuffix = TokenType::TYPE_DOUBLE;
                suffixStr = "_DOUBLE";
                suffixLen = 7;
                break;
            case '!':
                outSuffix = TokenType::TYPE_FLOAT;
                suffixStr = "_FLOAT";
                suffixLen = 6;
                break;
            case '&':
                outSuffix = TokenType::TYPE_INT;  // Treat LONG as INT
                suffixStr = "_LONG";
                suffixLen = 5;
                break;
            default:
                // Should not reach here, but handle gracefully
                return tokenValue;
        }

        // Build mangled name with single allocation
        std::string name;
        name.reserve(baseLen + suffixLen);
        name.assign(tokenValue, 0, baseLen);
        name.append(suffixStr, suffixLen);
        return name;
    }

    // Also check if next token is a separate type suffix (alternative syntax)
    if (outSuffix == TokenType::UNKNOWN) {
        if (current().type == TokenType::TYPE_INT ||
            current().type == TokenType::TYPE_STRING ||
            current().type == TokenType::TYPE_FLOAT ||
            current().type == TokenType::TYPE_DOUBLE) {
            outSuffix = current().type;
            advance();
        }
    }

    return tokenValue;
}

int Parser::parseLineNumber() {
    if (current().type != TokenType::NUMBER) {
        error("Expected line number");
        return 0;
    }

    int line = static_cast<int>(current().numberValue);
    advance();
    return line;
}

std::vector<ExpressionPtr> Parser::parseExpressionList() {
    std::vector<ExpressionPtr> exprs;

    // Reserve capacity for common case (1-4 arguments)
    exprs.reserve(4);

    if (isStartOfExpression()) {
        do {
            exprs.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    return exprs;
}

// =============================================================================
// INCLUDE File Processing
// =============================================================================

void Parser::expandIncludes(const std::vector<Token>& tokens) {
    m_expandedTokens.clear();
    m_includedFiles.clear();
    m_onceFiles.clear();
    m_includeStack.clear();

    // Track main file as already included
    std::string canonical = getCanonicalPath(m_currentSourceFile);
    m_includedFiles.insert(canonical);

    // Process tokens and expand INCLUDE statements
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& tok = tokens[i];

        // Check for OPTION ONCE at file level
        if (tok.type == TokenType::OPTION &&
            i + 1 < tokens.size() &&
            tokens[i + 1].type == TokenType::ONCE) {

            // Mark current file as ONCE
            m_onceFiles.insert(canonical);

            // Don't copy OPTION ONCE to expanded stream - it's consumed
            i++; // Skip ONCE token
            continue;
        }

        // Check for INCLUDE statement
        if (tok.type == TokenType::INCLUDE) {
            // Next token should be string literal
            if (i + 1 >= tokens.size() || tokens[i + 1].type != TokenType::STRING) {
                error("INCLUDE requires a string filename", tok.location);
                continue;
            }

            std::string filename = tokens[i + 1].value;
            i++; // Skip the string token

            // Process the include (this will recursively expand the file)
            if (!expandIncludesFromFile(filename, tok.location)) {
                // Error already reported, continue with next token
                continue;
            }
        } else {
            // Regular token - copy to expanded stream
            m_expandedTokens.push_back(tok);
        }
    }
}

bool Parser::expandIncludesFromFile(const std::string& filename, const SourceLocation& includeLoc) {
    // Resolve the include path
    std::string fullPath = resolveIncludePath(filename);
    if (fullPath.empty()) {
        error("Cannot find include file: " + filename, includeLoc);
        return false;
    }

    // Get canonical path for tracking
    std::string canonicalPath = getCanonicalPath(fullPath);

    // Check if this file was marked with OPTION ONCE and already included
    if (m_onceFiles.count(canonicalPath) > 0) {
        // Silently skip - OPTION ONCE prevents re-inclusion
        return true;
    }

    // Check for circular includes
    if (m_includedFiles.count(canonicalPath) > 0) {
        error("Circular include detected: " + filename, includeLoc);
        return false;
    }

    // Read the file
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        error("Cannot open include file: " + fullPath, includeLoc);
        return false;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    // Track this include
    IncludeContext ctx;
    ctx.filename = filename;
    ctx.fullPath = fullPath;
    ctx.includeLocation = includeLoc;
    m_includeStack.push_back(ctx);
    m_includedFiles.insert(canonicalPath);

    // Tokenize the included file
    Lexer lexer;
    lexer.tokenize(source);
    auto includedTokens = lexer.getTokens();

    // Remove EOF token from included file (we'll add it at the end of everything)
    if (!includedTokens.empty() &&
        includedTokens.back().type == TokenType::END_OF_FILE) {
        includedTokens.pop_back();
    }

    // Save current source file
    std::string savedSourceFile = m_currentSourceFile;
    m_currentSourceFile = fullPath;

    // Recursively expand includes in the included file
    // Process each token and handle nested includes
    for (size_t i = 0; i < includedTokens.size(); ++i) {
        const Token& tok = includedTokens[i];

        // Check for OPTION ONCE in included file
        if (tok.type == TokenType::OPTION &&
            i + 1 < includedTokens.size() &&
            includedTokens[i + 1].type == TokenType::ONCE) {

            // Mark this included file as ONCE
            m_onceFiles.insert(canonicalPath);
            i++; // Skip ONCE token
            continue;
        }

        // Check for nested INCLUDE
        if (tok.type == TokenType::INCLUDE) {
            if (i + 1 >= includedTokens.size() ||
                includedTokens[i + 1].type != TokenType::STRING) {
                error("INCLUDE requires a string filename", tok.location);
                continue;
            }

            std::string nestedFilename = includedTokens[i + 1].value;
            i++; // Skip string token

            // Recursively process nested include
            if (!expandIncludesFromFile(nestedFilename, tok.location)) {
                // Error already reported
                continue;
            }
        } else {
            // Regular token - add to expanded stream
            m_expandedTokens.push_back(tok);
        }
    }

    // Restore source file
    m_currentSourceFile = savedSourceFile;
    m_includeStack.pop_back();

    return true;
}

std::string Parser::resolveIncludePath(const std::string& filename) {
    // 1. Try relative to current file's directory
    if (!m_currentSourceFile.empty() && m_currentSourceFile != "<stdin>") {
        std::string dir = getDirectoryPart(m_currentSourceFile);
        if (!dir.empty()) {
            std::string candidate = dir + "/" + filename;
            if (fileExists(candidate)) {
                return candidate;
            }
        }
    }

    // 2. Try include paths (from -I options)
    for (const auto& path : m_includePaths) {
        std::string candidate = path + "/" + filename;
        if (fileExists(candidate)) {
            return candidate;
        }
    }

    // 3. Try current working directory
    if (fileExists(filename)) {
        return filename;
    }

    return "";  // Not found
}

std::string Parser::getCanonicalPath(const std::string& path) {
    // Handle empty or special paths
    if (path.empty() || path == "<stdin>" || path == "untitled") {
        return path;
    }

    // Convert to absolute path - wrap in try-catch for thread safety
    try {
        // Use realpath() on Unix/macOS
        #ifndef _WIN32
        char resolved[PATH_MAX];
        if (realpath(path.c_str(), resolved) != nullptr) {
            return std::string(resolved);
        }
        #else
        // Windows: use _fullpath
        char resolved[_MAX_PATH];
        if (_fullpath(resolved, path.c_str(), _MAX_PATH) != nullptr) {
            return std::string(resolved);
        }
        #endif
    } catch (...) {
        // If any exception occurs, fall back to original path
    }

    // Fallback: return original path
    return path;
}

std::string Parser::getDirectoryPart(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
}

bool Parser::fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

// =============================================================================
// SuperTerminal API Statement Parsers
// =============================================================================

// Graphics Commands

StatementPtr Parser::parseClgStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_GCLS, "CLG");
    advance(); // consume CLG or GCLS
    return stmt;
}

StatementPtr Parser::parseHlineStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_HLINE, "HLINE");
    advance(); // consume HLINE

    // HLINE x, y, length, color
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in HLINE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in HLINE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in HLINE statement");
    stmt->addArgument(parseExpression());

    return stmt;
}



// Text Layer Commands

StatementPtr Parser::parseAtStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_AT, "AT");
    advance(); // consume AT or LOCATE

    // AT x, y
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in AT statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseTextputStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_TEXTPUT, "TEXTPUT");
    advance(); // consume TEXTPUT

    // TEXTPUT x, y, text$ [, fg [, bg]]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TEXTPUT statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TEXTPUT statement");
    stmt->addArgument(parseExpression());

    // Optional foreground color (default: white 0xFFFFFFFF)
    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());

        // Optional background color (default: black 0xFF000000)
        if (match(TokenType::COMMA)) {
            stmt->addArgument(parseExpression());
        } else {
            // Default background: black
            stmt->addArgument(std::make_unique<NumberExpression>(0xFF000000));
        }
    } else {
        // Default foreground: white, background: black
        stmt->addArgument(std::make_unique<NumberExpression>(0xFFFFFFFF));
        stmt->addArgument(std::make_unique<NumberExpression>(0xFF000000));
    }

    return stmt;
}

StatementPtr Parser::parsePrintAtStatement() {
    auto stmt = std::make_unique<PrintAtStatement>();
    advance(); // consume PRINT_AT

    // Check for PRINT_AT USING syntax (USING comes first)
    // Syntax: PRINT_AT USING format, x, y, values...
    if (match(TokenType::USING)) {
        stmt->hasUsing = true;
        stmt->formatExpr = parseExpression();

        // Require comma after format string
        consume(TokenType::COMMA, "Expected ',' after PRINT_AT USING format string");
    }

    // Parse x, y coordinates (always required)
    stmt->x = parseExpression();
    consume(TokenType::COMMA, "Expected ',' after x coordinate in PRINT_AT");
    stmt->y = parseExpression();
    
    // Check for alternative syntax: PRINT_AT x, y, USING format, values...
    // This allows coordinates before USING (more intuitive)
    if (!stmt->hasUsing && match(TokenType::COMMA)) {
        // Check if next token is USING
        if (match(TokenType::USING)) {
            stmt->hasUsing = true;
            stmt->formatExpr = parseExpression();
            // Require comma after format string
            consume(TokenType::COMMA, "Expected ',' after USING format string");
        } else {
            // Not USING, continue with normal PRINT_AT parsing
            // We already consumed the comma, so don't consume again
        }
    } else if (!stmt->hasUsing) {
        consume(TokenType::COMMA, "Expected ',' after y coordinate in PRINT_AT");
    } else {
        // USING was at the beginning, we need comma after y
        consume(TokenType::COMMA, "Expected ',' after y coordinate in PRINT_AT");
    }

    if (stmt->hasUsing) {
        // PRINT_AT USING mode: parse values to format
        while (!isAtEnd() &&
               current().type != TokenType::END_OF_LINE &&
               current().type != TokenType::COLON) {
            stmt->usingValues.push_back(parseExpression());

            // Check for separator
            if (!match(TokenType::SEMICOLON) && !match(TokenType::COMMA)) {
                break;
            }
        }
    } else {
        // Regular PRINT_AT: parse text items with semicolons (like PRINT)
        // Syntax: PRINT_AT x, y, item1 ; item2 ; item3 , fg, bg
        // Semicolon (;) = append/concatenate next argument
        // Comma (,) = ends concatenation list, introduces optional colors

        while (!isAtEnd() &&
               current().type != TokenType::END_OF_LINE &&
               current().type != TokenType::COLON) {

            auto expr = parseExpression();

            // Check what separator follows
            if (match(TokenType::SEMICOLON)) {
                // Semicolon means concatenate - add item and continue
                stmt->addItem(std::move(expr), true, false);
            } else if (match(TokenType::COMMA)) {
                // Comma ends the concatenation list
                stmt->addItem(std::move(expr), false, true);

                // Check if there are color parameters following
                // Colors are optional - check if we're at end of line
                if (!isAtEnd() &&
                    current().type != TokenType::END_OF_LINE &&
                    current().type != TokenType::COLON) {
                    // Parse foreground color
                    stmt->fg = parseExpression();
                    stmt->hasExplicitColors = true;

                    // Check for background color
                    if (match(TokenType::COMMA)) {
                        if (!isAtEnd() &&
                            current().type != TokenType::END_OF_LINE &&
                            current().type != TokenType::COLON) {
                            stmt->bg = parseExpression();
                        }
                    }
                }
                break; // Comma ends the text items
            } else {
                // No separator - add final item and done
                stmt->addItem(std::move(expr), false, false);
                break;
            }
        }
    }

    return stmt;
}

StatementPtr Parser::parseInputAtStatement() {
    auto stmt = std::make_unique<InputAtStatement>();
    advance(); // consume INPUT_AT

    // Parse x, y coordinates (always required)
    stmt->x = parseExpression();
    consume(TokenType::COMMA, "Expected ',' after x coordinate in INPUT_AT");
    stmt->y = parseExpression();

    // Check for optional prompt and variable
    if (match(TokenType::COMMA)) {
        // Optional prompt string
        if (current().type == TokenType::STRING) {
            stmt->prompt = current().value;
            advance();

            // Accept either semicolon (BASIC INPUT style) or comma
            if (current().type == TokenType::SEMICOLON) {
                advance(); // consume semicolon
            } else if (current().type == TokenType::COMMA) {
                advance(); // consume comma
            } else {
                error("Expected ',' or ';' after prompt in INPUT_AT");
            }
        }

        // Variable name (required)
        if (current().type != TokenType::IDENTIFIER) {
            error("Expected variable name in INPUT_AT statement");
        } else {
            stmt->variable = current().value;
            advance();
        }

        // Optional foreground color
        if (match(TokenType::COMMA)) {
            stmt->fgColor = parseExpression();

            // Optional background color
            if (match(TokenType::COMMA)) {
                stmt->bgColor = parseExpression();
            }
        }
    } else {
        error("INPUT_AT requires at least x, y coordinates and a variable name");
    }

    return stmt;
}

StatementPtr Parser::parseRegistryCommandStatement() {
    // Get the command name from the current token
    std::string commandName = current().value;
    advance(); // consume the command token

    // Ensure the global registry is initialized
    FasterBASIC::ModularCommands::initializeGlobalRegistry();

    // Get the command definition from the registry
    auto& registry = FasterBASIC::ModularCommands::getGlobalCommandRegistry();
    const auto* commandDef = registry.getCommand(commandName);

    if (!commandDef) {
        error("Unknown registry command: " + commandName);
        return nullptr;
    }

    // Create a generic statement node - we'll use the existing STMT_PRINT_AT type for now
    // and store the actual command name in the statement name field
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_PRINT_AT, commandName);

    // Parse parameters according to the command definition
    size_t requiredParams = commandDef->getRequiredParameterCount();
    size_t totalParams = commandDef->getTotalParameterCount();

    // Helper function to validate parameter type at parse time
    auto validateParameterType = [&](const ExpressionPtr& expr, const FasterBASIC::ModularCommands::ParameterDefinition& paramDef, size_t paramIndex) {
        using ParamType = FasterBASIC::ModularCommands::ParameterType;

        // Check basic type compatibility based on expression type
        switch (paramDef.type) {
            case ParamType::INT:
            case ParamType::COLOR:
                if (dynamic_cast<StringExpression*>(expr.get())) {
                    error("Parameter " + std::to_string(paramIndex + 1) + " of " + commandName +
                         " ('" + paramDef.name + "') expects " +
                         FasterBASIC::ModularCommands::parameterTypeToString(paramDef.type) +
                         " but got string");
                }
                break;

            case ParamType::FLOAT:
                if (dynamic_cast<StringExpression*>(expr.get())) {
                    error("Parameter " + std::to_string(paramIndex + 1) + " of " + commandName +
                         " ('" + paramDef.name + "') expects " +
                         FasterBASIC::ModularCommands::parameterTypeToString(paramDef.type) +
                         " but got string");
                }
                break;

            case ParamType::STRING:
                // Strings can accept most expressions (they'll be converted)
                break;

            case ParamType::BOOL:
                // For now, accept numeric and boolean expressions
                // Could add more sophisticated boolean validation here
                if (dynamic_cast<StringExpression*>(expr.get())) {
                    // Check if it's a valid boolean string literal
                    auto strExpr = static_cast<StringExpression*>(expr.get());
                    std::string value = strExpr->value;
                    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                    if (value != "true" && value != "false") {
                        error("Parameter " + std::to_string(paramIndex + 1) + " of " + commandName +
                             " ('" + paramDef.name + "') expects boolean but got invalid string '" +
                             strExpr->value + "'");
                    }
                }
                break;

            case ParamType::OPTIONAL:
                break; // Optional is a modifier, not a type
        }
    };

    // Check for optional parentheses around argument list
    bool hasParens = match(TokenType::LPAREN);

    if (totalParams > 0) {
        // Parse first parameter
        auto expr = parseExpression();
        if (commandDef->parameters.size() > 0) {
            validateParameterType(expr, commandDef->parameters[0], 0);
        }
        stmt->addArgument(std::move(expr));

        // Parse remaining parameters
        for (size_t i = 1; i < totalParams; i++) {
            const auto& paramDef = commandDef->parameters[i];

            if (match(TokenType::COMMA)) {
                auto paramExpr = parseExpression();
                validateParameterType(paramExpr, paramDef, i);
                stmt->addArgument(std::move(paramExpr));
            } else if (!paramDef.isOptional) {
                error("Expected ',' in " + commandName + " statement - missing parameter '" + paramDef.name + "'");
                break;
            } else {
                // Add default value for optional parameter
                if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::INT) {
                    int defaultVal = paramDef.defaultValue.empty() ? 0 : std::stoi(paramDef.defaultValue);
                    stmt->addArgument(std::make_unique<NumberExpression>(defaultVal));
                } else if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::COLOR) {
                    uint32_t defaultVal = paramDef.defaultValue.empty() ? 0xFF000000 :
                        FasterBASIC::ModularCommands::parseColorValue(paramDef.defaultValue);
                    stmt->addArgument(std::make_unique<NumberExpression>(defaultVal));
                } else if (paramDef.type == FasterBASIC::ModularCommands::ParameterType::STRING) {
                    std::string defaultVal = paramDef.defaultValue.empty() ? "" : paramDef.defaultValue;
                    stmt->addArgument(std::make_unique<StringExpression>(defaultVal));
                } else {
                    // Default numeric value
                    stmt->addArgument(std::make_unique<NumberExpression>(0));
                }
            }
        }
    }

    // Consume closing parenthesis if we had an opening one
    if (hasParens) {
        consume(TokenType::RPAREN, "Expected ')' after " + commandName + " arguments");
    }

    return stmt;
}

StatementPtr Parser::parseTcharStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_TCHAR, "TCHAR");
    advance(); // consume TCHAR

    // TCHAR x, y, char$, fg, bg
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCHAR statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCHAR statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCHAR statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCHAR statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseTgridStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_TGRID, "TGRID");
    advance(); // consume TGRID

    // TGRID width, height
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TGRID statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseTscrollStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_TSCROLL, "TSCROLL");
    advance(); // consume TSCROLL

    // TSCROLL lines
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseTclearStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_TCLEAR, "TCLEAR");
    advance(); // consume TCLEAR

    // TCLEAR x, y, w, h
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCLEAR statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCLEAR statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in TCLEAR statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

// Sprite Commands

StatementPtr Parser::parseSprloadStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRLOAD, "SPRLOAD");
    advance(); // consume SPRLOAD

    // SPRLOAD id, filename$ [, builtin_flag]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRLOAD statement");
    stmt->addArgument(parseExpression());

    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());
    }

    return stmt;
}

StatementPtr Parser::parseSprfreeStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRFREE, "SPRFREE");
    advance(); // consume SPRFREE

    // SPRFREE id
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprshowStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRSHOW, "SPRSHOW");
    advance(); // consume SPRSHOW

    // SPRSHOW id
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parsSprhideStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRHIDE, "SPRHIDE");
    advance(); // consume SPRHIDE

    // SPRHIDE id
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprmoveStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRMOVE, "SPRMOVE");
    advance(); // consume SPRMOVE

    // SPRMOVE id, x, y
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRMOVE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRMOVE statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprposStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRPOS, "SPRPOS");
    advance(); // consume SPRPOS

    // SPRPOS id, x, y, scale, angle
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRPOS statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRPOS statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRPOS statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRPOS statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprtintStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRTINT, "SPRTINT");
    advance(); // consume SPRTINT

    // SPRTINT id, r, g, b, a
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRTINT statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRTINT statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRTINT statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRTINT statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprscaleStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRSCALE, "SPRSCALE");
    advance(); // consume SPRSCALE

    // SPRSCALE id, scale
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRSCALE statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprrotStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPRROT, "SPRROT");
    advance(); // consume SPRROT

    // SPRROT id, angle
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPRROT statement");
    stmt->addArgument(parseExpression());

    return stmt;
}

StatementPtr Parser::parseSprexplodeStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_SPREXPLODE, "SPREXPLODE");
    advance(); // consume SPREXPLODE

    // SPREXPLODE id, x, y [, count, speed, spread, lifetime, fade]
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPREXPLODE statement");
    stmt->addArgument(parseExpression());
    consume(TokenType::COMMA, "Expected ',' in SPREXPLODE statement");
    stmt->addArgument(parseExpression());

    // Optional parameters
    if (match(TokenType::COMMA)) {
        stmt->addArgument(parseExpression());
        if (match(TokenType::COMMA)) {
            stmt->addArgument(parseExpression());
            if (match(TokenType::COMMA)) {
                stmt->addArgument(parseExpression());
                if (match(TokenType::COMMA)) {
                    stmt->addArgument(parseExpression());
                    if (match(TokenType::COMMA)) {
                        stmt->addArgument(parseExpression());
                    }
                }
            }
        }
    }

    return stmt;
}

// Audio Commands



// Timing Commands

StatementPtr Parser::parseVsyncStatement() {
    auto stmt = std::make_unique<ExpressionStatement>(ASTNodeType::STMT_VSYNC, "VSYNC");
    advance(); // consume VSYNC
    return stmt;
}

// Parse AFTER statement: AFTER duration [MS|SECS|FRAMES] CALL handler | DO...DONE
StatementPtr Parser::parseAfterStatement() {
    advance(); // consume AFTER
    
    // Parse duration expression
    auto duration = parseExpression();
    
    // Parse optional time unit (default to MS for backward compatibility)
    TimeUnit unit = TimeUnit::MILLISECONDS;
    if (match(TokenType::MS)) {
        unit = TimeUnit::MILLISECONDS;
    } else if (match(TokenType::SECS)) {
        unit = TimeUnit::SECONDS;
    } else if (match(TokenType::FRAMES)) {
        unit = TimeUnit::FRAMES;
    }
    // If no unit specified, default to MS (backward compatible)
    
    // Check for CALL or DO
    if (match(TokenType::CALL)) {
        // CALL handler syntax
        if (!check(TokenType::IDENTIFIER)) {
            error("Expected handler name after CALL");
            return nullptr;
        }
        
        std::string handlerName = current().value;
        advance(); // consume handler name
        
        return std::make_unique<AfterStatement>(std::move(duration), unit, handlerName);
        
    } else if (match(TokenType::DO)) {
        // DO...DONE inline body syntax
        std::vector<StatementPtr> body;
        
        // Parse statements until DONE
        while (!check(TokenType::DONE) && !isAtEnd()) {
            // Skip END_OF_LINE tokens
            if (match(TokenType::END_OF_LINE)) {
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                body.push_back(std::move(stmt));
            }
        }
        
        if (!match(TokenType::DONE)) {
            error("Expected DONE to close DO block");
            return nullptr;
        }
        
        // Generate unique handler name
        std::string handlerName = "__timer_handler_" + std::to_string(++m_inlineHandlerCounter);
        
        return std::make_unique<AfterStatement>(std::move(duration), unit, handlerName, std::move(body));
        
    } else {
        error("Expected CALL or DO after AFTER duration");
        return nullptr;
    }
}

// Parse EVERY statement: EVERY duration [MS|SECS|FRAMES] CALL handler | DO...DONE
StatementPtr Parser::parseEveryStatement() {
    advance(); // consume EVERY
    
    // Parse duration expression
    auto duration = parseExpression();
    
    // Parse optional time unit (default to MS for backward compatibility)
    TimeUnit unit = TimeUnit::MILLISECONDS;
    if (match(TokenType::MS)) {
        unit = TimeUnit::MILLISECONDS;
    } else if (match(TokenType::SECS)) {
        unit = TimeUnit::SECONDS;
    } else if (match(TokenType::FRAMES)) {
        unit = TimeUnit::FRAMES;
    }
    // If no unit specified, default to MS (backward compatible)
    
    // Check for CALL or DO
    if (match(TokenType::CALL)) {
        // CALL handler syntax
        if (!check(TokenType::IDENTIFIER)) {
            error("Expected handler name after CALL");
            return nullptr;
        }
        
        std::string handlerName = current().value;
        advance(); // consume handler name
        
        return std::make_unique<EveryStatement>(std::move(duration), unit, handlerName);
        
    } else if (match(TokenType::DO)) {
        // DO...DONE inline body syntax
        std::vector<StatementPtr> body;
        
        // Parse statements until DONE
        while (!check(TokenType::DONE) && !isAtEnd()) {
            // Skip END_OF_LINE tokens
            if (match(TokenType::END_OF_LINE)) {
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                body.push_back(std::move(stmt));
            }
        }
        
        if (!match(TokenType::DONE)) {
            error("Expected DONE to close DO block");
            return nullptr;
        }
        
        // Generate unique handler name
        std::string handlerName = "__timer_handler_" + std::to_string(++m_inlineHandlerCounter);
        
        return std::make_unique<EveryStatement>(std::move(duration), unit, handlerName, std::move(body));
        
    } else {
        error("Expected CALL or DO after EVERY duration");
        return nullptr;
    }
}

// Parse AFTERFRAMES statement: AFTERFRAMES count CALL handler
StatementPtr Parser::parseAfterFramesStatement() {
    advance(); // consume AFTERFRAMES
    
    // Parse frame count expression
    auto frameCount = parseExpression();
    
    // Expect CALL keyword
    if (!match(TokenType::CALL)) {
        error("Expected CALL after AFTERFRAMES count");
        return nullptr;
    }
    // match() already advanced past CALL
    
    // Expect handler name (identifier)
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected handler name after CALL");
        return nullptr;
    }
    
    std::string handlerName = current().value;
    advance(); // consume handler name
    
    return std::make_unique<AfterFramesStatement>(std::move(frameCount), handlerName);
}

// Parse EVERYFRAME statement: EVERYFRAME count CALL handler
StatementPtr Parser::parseEveryFrameStatement() {
    advance(); // consume EVERYFRAME
    
    // Parse frame count expression
    auto frameCount = parseExpression();
    
    // Expect CALL keyword
    if (!match(TokenType::CALL)) {
        error("Expected CALL after EVERYFRAME count");
        return nullptr;
    }
    // match() already advanced past CALL
    
    // Expect handler name (identifier)
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected handler name after CALL");
        return nullptr;
    }
    
    std::string handlerName = current().value;
    advance(); // consume handler name
    
    return std::make_unique<EveryFrameStatement>(std::move(frameCount), handlerName);
}

// Parse RUN statement: RUN [UNTIL condition]
StatementPtr Parser::parseRunStatement() {
    advance(); // consume RUN
    
    ExpressionPtr condition = nullptr;
    
    // Check for optional UNTIL condition
    if (match(TokenType::UNTIL)) {
        // match() already consumed UNTIL, no need to advance again
        condition = parseExpression();
    }
    
    return std::make_unique<RunStatement>(std::move(condition));
}

// Parse TIMER statement: TIMER STOP [handler|timer_id|ALL] or TIMER INTERVAL value
StatementPtr Parser::parseTimerStatement() {
    advance(); // consume TIMER
    
    // Check for STOP or INTERVAL
    if (match(TokenType::STOP)) {
        // TIMER STOP statement
        auto stmt = std::make_unique<TimerStopStatement>();
        
        // Check what follows: ALL, identifier (handler name), or expression (timer ID)
        if (check(TokenType::IDENTIFIER) && current().value == "ALL") {
            stmt->targetType = TimerStopStatement::StopTarget::ALL;
            advance(); // consume ALL
        } else if (check(TokenType::IDENTIFIER)) {
            // Handler name
            stmt->targetType = TimerStopStatement::StopTarget::HANDLER;
            stmt->handlerName = current().value;
            advance(); // consume handler name
        } else {
            // Timer ID expression
            stmt->targetType = TimerStopStatement::StopTarget::TIMER_ID;
            stmt->timerId = parseExpression();
        }
        
        return stmt;
    } else if (check(TokenType::IDENTIFIER) && current().value == "INTERVAL") {
        // TIMER INTERVAL statement
        advance(); // consume INTERVAL
        
        // Parse interval value expression
        auto intervalExpr = parseExpression();
        if (!intervalExpr) {
            error("Expected interval value after TIMER INTERVAL");
            return nullptr;
        }
        
        // Create a TIMER INTERVAL statement
        return std::make_unique<TimerIntervalStatement>(std::move(intervalExpr));
    } else {
        error("Expected STOP or INTERVAL after TIMER");
        return nullptr;
    }
}



// =============================================================================
// Prescan for Function/Sub Declarations (allows forward references)
// =============================================================================

void Parser::prescanForFunctions() {
    m_userDefinedFunctions.clear();
    m_userDefinedSubs.clear();

    size_t savedIndex = m_currentIndex;
    m_currentIndex = 0;

    while (!isAtEnd()) {
        // Skip line numbers and end-of-line markers
        if (match(TokenType::NUMBER) || match(TokenType::END_OF_LINE)) {
            continue;
        }

        // Look for FUNCTION keyword
        if (current().type == TokenType::FUNCTION) {
            advance(); // consume FUNCTION
            if (current().type == TokenType::IDENTIFIER) {
                std::string funcName = current().value;
                m_userDefinedFunctions.insert(funcName);
                advance();
            }
            // Skip rest of line
            while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
                advance();
            }
            continue;
        }

        // Look for SUB keyword
        if (current().type == TokenType::SUB) {
            advance(); // consume SUB
            if (current().type == TokenType::IDENTIFIER) {
                std::string subName = current().value;
                m_userDefinedSubs.insert(subName);
                advance();
            }
            // Skip rest of line
            while (!isAtEnd() && current().type != TokenType::END_OF_LINE) {
                advance();
            }
            continue;
        }

        // Skip other tokens
        advance();
    }

    // Restore token position
    m_currentIndex = savedIndex;
}

} // namespace FasterBASIC
