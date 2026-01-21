//
// fasterbasic_semantic.cpp
// FasterBASIC - Semantic Analyzer Implementation
//
// Implements two-pass semantic analysis:
// Pass 1: Collect all declarations (line numbers, DIM, DEF FN, DATA)
// Pass 2: Validate usage, type check, control flow validation
//

#include "fasterbasic_semantic.h"

#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

#ifdef FBRUNNER3_BUILD
#include "../../FBRunner3/register_voice.h"
#endif

namespace FasterBASIC {

// =============================================================================
// SymbolTable toString
// =============================================================================

std::string SymbolTable::toString() const {
    std::ostringstream oss;
    
    oss << "=== SYMBOL TABLE ===\n\n";
    
    // Line numbers
    if (!lineNumbers.empty()) {
        oss << "Line Numbers (" << lineNumbers.size() << "):\n";
        std::vector<int> sortedLines;
        for (const auto& pair : lineNumbers) {
            sortedLines.push_back(pair.first);
        }
        std::sort(sortedLines.begin(), sortedLines.end());
        for (int line : sortedLines) {
            const auto& sym = lineNumbers.at(line);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Labels
    if (!labels.empty()) {
        oss << "Labels (" << labels.size() << "):\n";
        std::vector<std::string> sortedLabels;
        for (const auto& pair : labels) {
            sortedLabels.push_back(pair.first);
        }
        std::sort(sortedLabels.begin(), sortedLabels.end());
        for (const auto& name : sortedLabels) {
            const auto& sym = labels.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Variables
    if (!variables.empty()) {
        oss << "Variables (" << variables.size() << "):\n";
        std::vector<std::string> sortedVars;
        for (const auto& pair : variables) {
            sortedVars.push_back(pair.first);
        }
        std::sort(sortedVars.begin(), sortedVars.end());
        for (const auto& name : sortedVars) {
            const auto& sym = variables.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Arrays
    if (!arrays.empty()) {
        oss << "Arrays (" << arrays.size() << "):\n";
        std::vector<std::string> sortedArrays;
        for (const auto& pair : arrays) {
            sortedArrays.push_back(pair.first);
        }
        std::sort(sortedArrays.begin(), sortedArrays.end());
        for (const auto& name : sortedArrays) {
            const auto& sym = arrays.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Functions
    if (!functions.empty()) {
        oss << "Functions (" << functions.size() << "):\n";
        std::vector<std::string> sortedFuncs;
        for (const auto& pair : functions) {
            sortedFuncs.push_back(pair.first);
        }
        std::sort(sortedFuncs.begin(), sortedFuncs.end());
        for (const auto& name : sortedFuncs) {
            const auto& sym = functions.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Data segment
    if (!dataSegment.values.empty()) {
        oss << "Data Segment:\n";
        oss << "  " << dataSegment.toString() << "\n";
        oss << "  Values: ";
        for (size_t i = 0; i < std::min(dataSegment.values.size(), size_t(10)); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << dataSegment.values[i] << "\"";
        }
        if (dataSegment.values.size() > 10) {
            oss << ", ... (" << (dataSegment.values.size() - 10) << " more)";
        }
        oss << "\n\n";
    }
    
    oss << "=== END SYMBOL TABLE ===\n";
    
    return oss.str();
}

// =============================================================================
// Constructor/Destructor
// =============================================================================

SemanticAnalyzer::SemanticAnalyzer()
    : m_strictMode(false)
    , m_warnUnused(true)
    , m_requireExplicitDim(true)
    , m_cancellableLoops(true)
    , m_program(nullptr)
    , m_currentLineNumber(0)
    , m_inTimerHandler(false)
    , m_currentFunctionName("")
{
    initializeBuiltinFunctions();
    
    // Load additional functions from the global command registry
    loadFromCommandRegistry(ModularCommands::getGlobalCommandRegistry());
    
    m_constantsManager.addPredefinedConstants();
    
    // Register voice waveform constants (WAVE_SINE, WAVE_SQUARE, etc.)
#ifdef FBRUNNER3_BUILD
    FBRunner3::VoiceRegistration::registerVoiceConstants(m_constantsManager);
#endif
    
    // Register ALL predefined constants from ConstantsManager into symbol table
    // This allows them to be resolved like user-defined constants during compilation
    // Dynamically loads all constants - no hardcoded list needed!
    // Constants are stored in lowercase and the formatter will NOT uppercase them
    std::vector<std::string> predefinedNames = m_constantsManager.getAllConstantNames();
    
    for (const auto& name : predefinedNames) {
        int index = m_constantsManager.getConstantIndex(name);
        if (index >= 0) {
            ConstantValue val = m_constantsManager.getConstant(index);
            ConstantSymbol sym;
            if (std::holds_alternative<int64_t>(val)) {
                sym = ConstantSymbol(std::get<int64_t>(val));
            } else if (std::holds_alternative<double>(val)) {
                sym = ConstantSymbol(std::get<double>(val));
            } else if (std::holds_alternative<std::string>(val)) {
                sym = ConstantSymbol(std::get<std::string>(val));
            }
            sym.index = index;
            
            // Store with lowercase key (as returned from manager)
            m_symbolTable.constants[name] = sym;
        }
    }
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

// =============================================================================
// Constants Management
// =============================================================================

void SemanticAnalyzer::ensureConstantsLoaded() {
    // Check if constants are already loaded
    if (m_constantsManager.getConstantCount() > 0) {
        return; // Already loaded
    }
    
    // Clear and reload predefined constants
    m_constantsManager.clear();
    m_constantsManager.addPredefinedConstants();
    
    // Register voice waveform constants (WAVE_SINE, WAVE_SQUARE, etc.)
#ifdef FBRUNNER3_BUILD
    FBRunner3::VoiceRegistration::registerVoiceConstants(m_constantsManager);
#endif
    
    // Register ALL predefined constants from ConstantsManager into symbol table
    // Constants are stored in lowercase and the formatter will NOT uppercase them
    std::vector<std::string> predefinedNames = m_constantsManager.getAllConstantNames();
    
    for (const auto& name : predefinedNames) {
        int index = m_constantsManager.getConstantIndex(name);
        if (index >= 0) {
            ConstantValue val = m_constantsManager.getConstant(index);
            ConstantSymbol sym;
            if (std::holds_alternative<int64_t>(val)) {
                sym = ConstantSymbol(std::get<int64_t>(val));
            } else if (std::holds_alternative<double>(val)) {
                sym = ConstantSymbol(std::get<double>(val));
            } else if (std::holds_alternative<std::string>(val)) {
                sym = ConstantSymbol(std::get<std::string>(val));
            }
            sym.index = index;
            
            // Store with lowercase key (as returned from manager)
            m_symbolTable.constants[name] = sym;
        }
    }
}

// =============================================================================
// Runtime Constant Injection
// =============================================================================

void SemanticAnalyzer::injectRuntimeConstant(const std::string& name, int64_t value) {
    // Add to ConstantsManager and get index (manager will normalize to lowercase)
    int index = m_constantsManager.addConstant(name, value);
    
    // Create symbol and add to symbol table (use lowercase key)
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    ConstantSymbol sym(value);
    sym.index = index;
    m_symbolTable.constants[lowerName] = sym;
}

void SemanticAnalyzer::injectRuntimeConstant(const std::string& name, double value) {
    // Add to ConstantsManager and get index (manager will normalize to lowercase)
    int index = m_constantsManager.addConstant(name, value);
    
    // Create symbol and add to symbol table (use lowercase key)
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    ConstantSymbol sym(value);
    sym.index = index;
    m_symbolTable.constants[lowerName] = sym;
}

void SemanticAnalyzer::injectRuntimeConstant(const std::string& name, const std::string& value) {
    // Add to ConstantsManager and get index (manager will normalize to lowercase)
    int index = m_constantsManager.addConstant(name, value);
    
    // Create symbol and add to symbol table (use lowercase key)
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    ConstantSymbol sym(value);
    sym.index = index;
    m_symbolTable.constants[lowerName] = sym;
}

// =============================================================================
// DATA Label Registration
// =============================================================================

void SemanticAnalyzer::registerDataLabels(const std::map<std::string, int>& dataLabels) {
    // Register labels from DATA preprocessing so RESTORE can find them
    for (const auto& [labelName, lineNumber] : dataLabels) {
        // Create a label symbol for this DATA label
        LabelSymbol sym;
        sym.name = labelName;
        sym.labelId = m_symbolTable.nextLabelId++;
        sym.programLineIndex = 0; // DATA labels don't have a program line index
        sym.definition.line = lineNumber;
        sym.definition.column = 0;
        
        m_symbolTable.labels[labelName] = sym;
    }
}

// =============================================================================
// Main Analysis Entry Point
// =============================================================================

bool SemanticAnalyzer::analyze(Program& program, const CompilerOptions& options) {
    m_program = &program;
    m_errors.clear();
    m_warnings.clear();
    
    // Preserve predefined constants before resetting symbol table
    auto savedConstants = m_symbolTable.constants;
    
    m_symbolTable = SymbolTable();
    
    // Restore predefined constants
    m_symbolTable.constants = savedConstants;
    
    // Apply compiler options to symbol table
    m_symbolTable.arrayBase = options.arrayBase;
    m_symbolTable.unicodeMode = options.unicodeMode;
    m_symbolTable.errorTracking = options.errorTracking;
    m_symbolTable.cancellableLoops = options.cancellableLoops;
    m_symbolTable.forceYieldEnabled = options.forceYieldEnabled;
    m_symbolTable.forceYieldBudget = options.forceYieldBudget;
    m_cancellableLoops = options.cancellableLoops;
    
    // Clear control flow stacks
    while (!m_forStack.empty()) m_forStack.pop();
    while (!m_whileStack.empty()) m_whileStack.pop();
    while (!m_repeatStack.empty()) m_repeatStack.pop();
    
    // Two-pass analysis
    pass1_collectDeclarations(program);
    pass2_validate(program);
    
    // Final validation
    validateControlFlow(program);
    
    if (m_warnUnused) {
        checkUnusedVariables();
    }
    
    return m_errors.empty();
}

// =============================================================================
// Pass 1: Declaration Collection
// =============================================================================

void SemanticAnalyzer::pass1_collectDeclarations(Program& program) {
    collectLineNumbers(program);
    collectLabels(program);
    // NOTE: collectOptionStatements removed - options are now collected by parser
    collectTypeDeclarations(program);  // Collect TYPE/END TYPE declarations first
    collectConstantStatements(program);  // Collect constants BEFORE DIM statements (they may use constants)
    collectDimStatements(program);
    collectDefStatements(program);
    collectFunctionAndSubStatements(program);
    collectDataStatements(program);
    collectTimerHandlers(program);  // Collect AFTER/EVERY handlers before validation
}

void SemanticAnalyzer::collectLineNumbers(Program& program) {
    for (size_t i = 0; i < program.lines.size(); ++i) {
        const auto& line = program.lines[i];
        if (line->lineNumber > 0) {
            // Check for duplicate line numbers
            if (m_symbolTable.lineNumbers.find(line->lineNumber) != m_symbolTable.lineNumbers.end()) {
                error(SemanticErrorType::DUPLICATE_LINE_NUMBER,
                      "Duplicate line number: " + std::to_string(line->lineNumber),
                      line->location);
                continue;
            }
            
            LineNumberSymbol sym;
            sym.lineNumber = line->lineNumber;
            sym.programLineIndex = i;
            m_symbolTable.lineNumbers[line->lineNumber] = sym;
        }
    }
}

void SemanticAnalyzer::collectLabels(Program& program) {
    for (size_t i = 0; i < program.lines.size(); ++i) {
        const auto& line = program.lines[i];
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_LABEL) {
                const auto& labelStmt = static_cast<const LabelStatement&>(*stmt);
                declareLabel(labelStmt.labelName, i, stmt->location);
            }
        }
    }
}

void SemanticAnalyzer::collectOptionStatements(Program& program) {
    // NOTE: This function is now deprecated. OPTION statements are collected
    // by the parser before AST generation and passed as CompilerOptions.
    // This function is kept for backward compatibility but does nothing.
    // OPTION statements should not appear in the AST anymore.
}

void SemanticAnalyzer::collectDimStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_DIM) {
                processDimStatement(static_cast<const DimStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::collectDefStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_DEF) {
                processDefStatement(static_cast<const DefStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::collectConstantStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_CONSTANT) {
                processConstantStatement(static_cast<const ConstantStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::collectTypeDeclarations(Program& program) {
    // Collect all TYPE declarations in pass 1
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_TYPE) {
                processTypeDeclarationStatement(static_cast<const TypeDeclarationStatement*>(stmt.get()));
            }
        }
    }
}

void SemanticAnalyzer::processTypeDeclarationStatement(const TypeDeclarationStatement* stmt) {
    if (!stmt) return;
    
    // Check for duplicate type name
    if (lookupType(stmt->typeName) != nullptr) {
        error(SemanticErrorType::DUPLICATE_TYPE,
              "Type '" + stmt->typeName + "' is already defined",
              stmt->location);
        return;
    }
    
    // Create the type symbol
    TypeSymbol typeSymbol(stmt->typeName);
    typeSymbol.declaration = stmt->location;
    
    // Track field names to detect duplicates
    std::unordered_set<std::string> fieldNames;
    
    // Detect SIMD type pattern (PAIR = 2 doubles, QUAD = 4 floats)
    TypeDeclarationStatement::SIMDType detectedSIMDType = TypeDeclarationStatement::SIMDType::NONE;
    
    if (stmt->fields.size() == 2) {
        // Check for PAIR pattern: 2 consecutive doubles
        if (stmt->fields[0].isBuiltIn && stmt->fields[1].isBuiltIn &&
            stmt->fields[0].builtInType == TokenType::KEYWORD_DOUBLE &&
            stmt->fields[1].builtInType == TokenType::KEYWORD_DOUBLE) {
            detectedSIMDType = TypeDeclarationStatement::SIMDType::PAIR;
        }
    } else if (stmt->fields.size() == 4) {
        // Check for QUAD pattern: 4 consecutive floats (SINGLE)
        if (stmt->fields[0].isBuiltIn && stmt->fields[1].isBuiltIn &&
            stmt->fields[2].isBuiltIn && stmt->fields[3].isBuiltIn &&
            stmt->fields[0].builtInType == TokenType::KEYWORD_SINGLE &&
            stmt->fields[1].builtInType == TokenType::KEYWORD_SINGLE &&
            stmt->fields[2].builtInType == TokenType::KEYWORD_SINGLE &&
            stmt->fields[3].builtInType == TokenType::KEYWORD_SINGLE) {
            detectedSIMDType = TypeDeclarationStatement::SIMDType::QUAD;
        }
    }
    
    // Store SIMD type in the statement (mutable cast for metadata)
    const_cast<TypeDeclarationStatement*>(stmt)->simdType = detectedSIMDType;
    
    // Debug output for SIMD detection
    if (detectedSIMDType == TypeDeclarationStatement::SIMDType::PAIR) {
        std::cout << "[SIMD] Detected PAIR type: " << stmt->typeName 
                  << " (2 consecutive DOUBLEs - Vec2D pattern)" << std::endl;
    } else if (detectedSIMDType == TypeDeclarationStatement::SIMDType::QUAD) {
        std::cout << "[SIMD] Detected QUAD type: " << stmt->typeName 
                  << " (4 consecutive FLOATs - Color pattern)" << std::endl;
    }
    
    // Process each field
    for (const auto& field : stmt->fields) {
        // Check for duplicate field name
        if (fieldNames.find(field.name) != fieldNames.end()) {
            error(SemanticErrorType::DUPLICATE_FIELD,
                  "Duplicate field '" + field.name + "' in type '" + stmt->typeName + "'",
                  stmt->location);
            continue;
        }
        fieldNames.insert(field.name);
        
        // Convert TokenType to VariableType for built-in types
        VariableType varType = VariableType::UNKNOWN;
        if (field.isBuiltIn) {
            switch (field.builtInType) {
                case TokenType::KEYWORD_INTEGER:
                    varType = VariableType::INT;
                    break;
                case TokenType::KEYWORD_SINGLE:
                    varType = VariableType::FLOAT;
                    break;
                case TokenType::KEYWORD_DOUBLE:
                    varType = VariableType::DOUBLE;
                    break;
                case TokenType::KEYWORD_STRING:
                    varType = m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
                    break;
                case TokenType::KEYWORD_LONG:
                    varType = VariableType::INT;  // Treat LONG as INT for now
                    break;
                default:
                    error(SemanticErrorType::INVALID_TYPE_FIELD,
                          "Invalid field type in type '" + stmt->typeName + "'",
                          stmt->location);
                    continue;
            }
        }
        
        // Add field to type (validation of user-defined types will happen in second pass)
        TypeSymbol::Field typeField(field.name, field.typeName, varType, field.isBuiltIn);
        typeSymbol.fields.push_back(typeField);
    }
    
    // Store SIMD type in the TypeSymbol for later use
    typeSymbol.simdType = detectedSIMDType;
    
    // Register the type
    m_symbolTable.types[stmt->typeName] = typeSymbol;
}

void SemanticAnalyzer::collectTimerHandlers(Program& program) {
    // Collect all handlers registered via AFTER/EVERY/AFTERFRAMES/EVERYFRAME statements
    // This must be done in pass1 so that validation in pass2 knows which functions are handlers
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_AFTER) {
                const AfterStatement& afterStmt = static_cast<const AfterStatement&>(*stmt);
                if (!afterStmt.handlerName.empty()) {
                    m_registeredHandlers.insert(afterStmt.handlerName);
                }
            } else if (stmt->getType() == ASTNodeType::STMT_EVERY) {
                const EveryStatement& everyStmt = static_cast<const EveryStatement&>(*stmt);
                if (!everyStmt.handlerName.empty()) {
                    m_registeredHandlers.insert(everyStmt.handlerName);
                }
            } else if (stmt->getType() == ASTNodeType::STMT_AFTERFRAMES) {
                const AfterFramesStatement& afterFramesStmt = static_cast<const AfterFramesStatement&>(*stmt);
                if (!afterFramesStmt.handlerName.empty()) {
                    m_registeredHandlers.insert(afterFramesStmt.handlerName);
                }
            } else if (stmt->getType() == ASTNodeType::STMT_EVERYFRAME) {
                const EveryFrameStatement& everyFrameStmt = static_cast<const EveryFrameStatement&>(*stmt);
                if (!everyFrameStmt.handlerName.empty()) {
                    m_registeredHandlers.insert(everyFrameStmt.handlerName);
                }
            }
        }
    }
}

void SemanticAnalyzer::collectFunctionAndSubStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_FUNCTION) {
                processFunctionStatement(static_cast<const FunctionStatement&>(*stmt));
            } else if (stmt->getType() == ASTNodeType::STMT_SUB) {
                processSubStatement(static_cast<const SubStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::processFunctionStatement(const FunctionStatement& stmt) {
    // Check if already declared
    if (m_symbolTable.functions.find(stmt.functionName) != m_symbolTable.functions.end()) {
        error(SemanticErrorType::FUNCTION_REDECLARED,
              "Function " + stmt.functionName + " already declared",
              stmt.location);
        return;
    }
    
    FunctionSymbol sym;
    sym.name = stmt.functionName;
    sym.parameters = stmt.parameters;
    sym.parameterIsByRef = stmt.parameterIsByRef;
    
    // Process parameter types
    for (size_t i = 0; i < stmt.parameters.size(); ++i) {
        VariableType paramType = VariableType::UNKNOWN;
        std::string paramTypeName = "";
        
        if (i < stmt.parameterAsTypes.size() && !stmt.parameterAsTypes[i].empty()) {
            // Has AS TypeName
            paramTypeName = stmt.parameterAsTypes[i];
            
            // Check if it's a built-in type keyword or user-defined type
            if (paramTypeName == "INTEGER" || paramTypeName == "INT") {
                paramType = VariableType::INT;
                paramTypeName = "";  // It's built-in, don't store name
            } else if (paramTypeName == "DOUBLE") {
                paramType = VariableType::DOUBLE;
                paramTypeName = "";
            } else if (paramTypeName == "SINGLE" || paramTypeName == "FLOAT") {
                paramType = VariableType::FLOAT;
                paramTypeName = "";
            } else if (paramTypeName == "STRING") {
                paramType = VariableType::STRING;
                paramTypeName = "";
            } else if (paramTypeName == "LONG") {
                paramType = VariableType::INT;
                paramTypeName = "";
            } else {
                // User-defined type - validate it exists
                if (m_symbolTable.types.find(paramTypeName) == m_symbolTable.types.end()) {
                    error(SemanticErrorType::TYPE_ERROR,
                          "Unknown type '" + paramTypeName + "' in parameter " + stmt.parameters[i],
                          stmt.location);
                }
                // Keep paramTypeName for user-defined types
            }
        } else if (i < stmt.parameterTypes.size()) {
            // Has type suffix
            paramType = inferTypeFromSuffix(stmt.parameterTypes[i]);
        } else {
            paramType = VariableType::FLOAT;  // Default type
        }
        
        sym.parameterTypes.push_back(paramType);
        sym.parameterTypeNames.push_back(paramTypeName);
    }
    
    // Process return type
    if (stmt.hasReturnAsType && !stmt.returnTypeAsName.empty()) {
        sym.returnTypeName = stmt.returnTypeAsName;
        
        // Check if it's a built-in type keyword or user-defined type
        if (sym.returnTypeName == "INTEGER" || sym.returnTypeName == "INT") {
            sym.returnType = VariableType::INT;
            sym.returnTypeName = "";
        } else if (sym.returnTypeName == "DOUBLE") {
            sym.returnType = VariableType::DOUBLE;
            sym.returnTypeName = "";
        } else if (sym.returnTypeName == "SINGLE" || sym.returnTypeName == "FLOAT") {
            sym.returnType = VariableType::FLOAT;
            sym.returnTypeName = "";
        } else if (sym.returnTypeName == "STRING") {
            sym.returnType = VariableType::STRING;
            sym.returnTypeName = "";
        } else if (sym.returnTypeName == "LONG") {
            sym.returnType = VariableType::INT;
            sym.returnTypeName = "";
        } else {
            // User-defined type - validate it exists
            if (m_symbolTable.types.find(sym.returnTypeName) == m_symbolTable.types.end()) {
                error(SemanticErrorType::TYPE_ERROR,
                      "Unknown return type '" + sym.returnTypeName + "' for function " + stmt.functionName,
                      stmt.location);
            }
            // Keep returnTypeName for user-defined types
            sym.returnType = VariableType::UNKNOWN;  // Mark as user-defined
        }
    } else {
        sym.returnType = inferTypeFromSuffix(stmt.returnTypeSuffix);
    }
    
    m_symbolTable.functions[stmt.functionName] = sym;
}

void SemanticAnalyzer::processSubStatement(const SubStatement& stmt) {
    // Check if already declared
    if (m_symbolTable.functions.find(stmt.subName) != m_symbolTable.functions.end()) {
        error(SemanticErrorType::FUNCTION_REDECLARED,
              "Subroutine " + stmt.subName + " already declared",
              stmt.location);
        return;
    }
    
    FunctionSymbol sym;
    sym.name = stmt.subName;
    sym.parameters = stmt.parameters;
    sym.parameterIsByRef = stmt.parameterIsByRef;
    sym.returnType = VariableType::VOID;
    
    // Process parameter types
    for (size_t i = 0; i < stmt.parameters.size(); ++i) {
        VariableType paramType = VariableType::UNKNOWN;
        std::string paramTypeName = "";
        
        if (i < stmt.parameterAsTypes.size() && !stmt.parameterAsTypes[i].empty()) {
            // Has AS TypeName
            paramTypeName = stmt.parameterAsTypes[i];
            
            // Check if it's a built-in type keyword or user-defined type
            if (paramTypeName == "INTEGER" || paramTypeName == "INT") {
                paramType = VariableType::INT;
                paramTypeName = "";  // It's built-in, don't store name
            } else if (paramTypeName == "DOUBLE") {
                paramType = VariableType::DOUBLE;
                paramTypeName = "";
            } else if (paramTypeName == "SINGLE" || paramTypeName == "FLOAT") {
                paramType = VariableType::FLOAT;
                paramTypeName = "";
            } else if (paramTypeName == "STRING") {
                paramType = VariableType::STRING;
                paramTypeName = "";
            } else if (paramTypeName == "LONG") {
                paramType = VariableType::INT;
                paramTypeName = "";
            } else {
                // User-defined type - validate it exists
                if (m_symbolTable.types.find(paramTypeName) == m_symbolTable.types.end()) {
                    error(SemanticErrorType::TYPE_ERROR,
                          "Unknown type '" + paramTypeName + "' in parameter " + stmt.parameters[i],
                          stmt.location);
                }
                // Keep paramTypeName for user-defined types
            }
        } else if (i < stmt.parameterTypes.size()) {
            // Has type suffix
            paramType = inferTypeFromSuffix(stmt.parameterTypes[i]);
        } else {
            paramType = VariableType::FLOAT;  // Default type
        }
        
        sym.parameterTypes.push_back(paramType);
        sym.parameterTypeNames.push_back(paramTypeName);
    }
    
    m_symbolTable.functions[stmt.subName] = sym;
}

void SemanticAnalyzer::collectDataStatements(Program& program) {
    // Early pass - collect ONLY DATA statements
    // Track both line numbers and labels that appear on DATA lines
    // Also track labels on preceding lines (label followed by DATA on next line)
    
    std::string pendingLabel;  // Label from previous line waiting for DATA
    
    for (const auto& line : program.lines) {
        int lineNumber = line->lineNumber;
        std::string dataLabel;  // Label on this line (if any)
        bool hasData = false;
        bool hasLabel = false;
        
        // First pass: check if this line has DATA and/or collect any label
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_LABEL) {
                // Found a label on this line
                const auto* labelStmt = static_cast<const LabelStatement*>(stmt.get());
                dataLabel = labelStmt->labelName;
                hasLabel = true;
                // DEBUG
                fprintf(stderr, "[collectDataStatements] Found label '%s' on line %d\n", 
                       dataLabel.c_str(), lineNumber);
            } else if (stmt->getType() == ASTNodeType::STMT_DATA) {
                hasData = true;
                // DEBUG
                fprintf(stderr, "[collectDataStatements] Found DATA on line %d\n", lineNumber);
            }
        }
        
        // Second pass: if this line has DATA, process it with label info
        if (hasData) {
            // Use label from current line, or pending label from previous line
            std::string effectiveLabel = dataLabel.empty() ? pendingLabel : dataLabel;
            
            // DEBUG
            fprintf(stderr, "[collectDataStatements] Processing DATA on line %d with label '%s'\n", 
                   lineNumber, effectiveLabel.c_str());
            
            for (const auto& stmt : line->statements) {
                if (stmt->getType() == ASTNodeType::STMT_DATA) {
                    processDataStatement(static_cast<const DataStatement&>(*stmt), 
                                       lineNumber, effectiveLabel);
                }
            }
            
            // Clear pending label after using it
            pendingLabel.clear();
        } else if (hasLabel) {
            // Label without DATA on this line - save it for next DATA line
            pendingLabel = dataLabel;
        } else {
            // Line with neither label nor DATA - clear pending label
            pendingLabel.clear();
        }
    }
}

void SemanticAnalyzer::processDimStatement(const DimStatement& stmt) {
    // TODO: Add support for DIM var AS TypeName in future enhancement
    // For now, process arrays as usual
    
    for (const auto& arrayDim : stmt.arrays) {
        // Check if already declared
        if (m_symbolTable.arrays.find(arrayDim.name) != m_symbolTable.arrays.end()) {
            error(SemanticErrorType::ARRAY_REDECLARED,
                  "Array '" + arrayDim.name + "' already declared",
                  stmt.location);
            continue;
        }
        
        // Calculate dimensions
        // NOTE: Since arrays compile to Lua tables (which are dynamic), we don't strictly
        // need compile-time constant dimensions. We'll try to evaluate as constants for
        // optimization hints, but allow variables too.
        std::vector<int> dimensions;
        int totalSize = 1;
        bool hasUnknownDimensions = false;
        
        for (const auto& dimExpr : arrayDim.dimensions) {
            // Check if this is a compile-time constant expression
            bool isConstant = isConstantExpression(*dimExpr);
            
            if (isConstant) {
                // Try to evaluate as constant expression for optimization
                try {
                    FasterBASIC::ConstantValue constVal = evaluateConstantExpression(*dimExpr);
                    
                    // Convert to integer size
                    int size = 0;
                    if (std::holds_alternative<int64_t>(constVal)) {
                        size = static_cast<int>(std::get<int64_t>(constVal));
                    } else if (std::holds_alternative<double>(constVal)) {
                        size = static_cast<int>(std::get<double>(constVal));
                    } else {
                        // Non-numeric constant - this is an error
                        error(SemanticErrorType::INVALID_ARRAY_INDEX,
                              "Array dimension must be numeric",
                              stmt.location);
                        size = 10;  // Default fallback
                    }
                    
                    if (size <= 0) {
                        error(SemanticErrorType::INVALID_ARRAY_INDEX,
                              "Constant array dimension must be positive (got " + std::to_string(size) + ")",
                              stmt.location);
                        size = 1;
                    }
                    
                    // BASIC arrays: DIM A(N) creates array with indices 0 to N (inclusive)
                    // Store N+1 as the dimension size to allow N+1 elements
                    dimensions.push_back(size + 1);
                    totalSize *= (size + 1);
                } catch (...) {
                    // Evaluation failed even though it looked constant
                    dimensions.push_back(-1);
                    hasUnknownDimensions = true;
                }
            } else {
                // Non-constant dimension (e.g., variable) - allowed since Lua arrays are dynamic
                // Store -1 as a sentinel to indicate runtime-determined dimension
                dimensions.push_back(-1);
                hasUnknownDimensions = true;
                // Can't calculate total size if any dimension is unknown
            }
        }
        
        ArraySymbol sym;
        sym.name = arrayDim.name;
        sym.type = inferTypeFromSuffix(arrayDim.typeSuffix);
        if (sym.type == VariableType::UNKNOWN) {
            sym.type = inferTypeFromName(arrayDim.name);
        }
        sym.dimensions = dimensions;
        sym.isDeclared = true;
        sym.declaration = stmt.location;
        // Only store totalSize if all dimensions are known at compile time
        sym.totalSize = hasUnknownDimensions ? -1 : totalSize;
        // Store the AS TypeName for user-defined types
        sym.asTypeName = arrayDim.asTypeName;
        
        m_symbolTable.arrays[arrayDim.name] = sym;
    }
}

void SemanticAnalyzer::processDefStatement(const DefStatement& stmt) {
    // Check if already declared
    if (m_symbolTable.functions.find(stmt.functionName) != m_symbolTable.functions.end()) {
        error(SemanticErrorType::FUNCTION_REDECLARED,
              "Function FN" + stmt.functionName + " already declared",
              stmt.location);
        return;
    }
    
    FunctionSymbol sym;
    sym.name = stmt.functionName;
    sym.parameters = stmt.parameters;
    sym.body = stmt.body.get();
    sym.definition = stmt.location;
    
    // Infer return type from function name
    sym.returnType = inferTypeFromName(stmt.functionName);
    
    m_symbolTable.functions[stmt.functionName] = sym;
}

void SemanticAnalyzer::processConstantStatement(const ConstantStatement& stmt) {
    // Check if constant already declared (case-insensitive)
    std::string lowerName = stmt.name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (m_symbolTable.constants.find(lowerName) != m_symbolTable.constants.end()) {
        error(SemanticErrorType::DUPLICATE_LABEL,  // Reusing error type for constants
              "Constant " + stmt.name + " already declared",
              stmt.location);
        return;
    }
    
    // Evaluate constant expression at compile time (supports full expressions now)
    FasterBASIC::ConstantValue evalResult = evaluateConstantExpression(*stmt.value);
    
    // Convert ConstantValue to ConstantSymbol
    ConstantSymbol constValue;
    if (std::holds_alternative<int64_t>(evalResult)) {
        constValue = ConstantSymbol(std::get<int64_t>(evalResult));
    } else if (std::holds_alternative<double>(evalResult)) {
        constValue = ConstantSymbol(std::get<double>(evalResult));
    } else if (std::holds_alternative<std::string>(evalResult)) {
        constValue = ConstantSymbol(std::get<std::string>(evalResult));
    }
    
    // Add to C++ ConstantsManager and get index
    int index = -1;
    if (std::holds_alternative<int64_t>(evalResult)) {
        index = m_constantsManager.addConstant(stmt.name, std::get<int64_t>(evalResult));
    } else if (std::holds_alternative<double>(evalResult)) {
        index = m_constantsManager.addConstant(stmt.name, std::get<double>(evalResult));
    } else if (std::holds_alternative<std::string>(evalResult)) {
        index = m_constantsManager.addConstant(stmt.name, std::get<std::string>(evalResult));
    }
    
    constValue.index = index;
    m_symbolTable.constants[lowerName] = constValue;
}

void SemanticAnalyzer::processDataStatement(const DataStatement& stmt, int lineNumber,
                                            const std::string& dataLabel) {
    // Get current index (where this DATA starts)
    size_t currentIndex = m_symbolTable.dataSegment.values.size();
    
    // Record restore point by line number (if present)
    if (lineNumber > 0) {
        m_symbolTable.dataSegment.restorePoints[lineNumber] = currentIndex;
        // DEBUG
        fprintf(stderr, "[processDataStatement] Recorded line %d -> index %zu\n", 
               lineNumber, currentIndex);
    }
    
    // Record restore point by label (if present on this DATA line)
    if (!dataLabel.empty()) {
        m_symbolTable.dataSegment.labelRestorePoints[dataLabel] = currentIndex;
        // DEBUG
        fprintf(stderr, "[processDataStatement] Recorded label '%s' -> index %zu\n", 
               dataLabel.c_str(), currentIndex);
    }
    
    // Add values to data segment
    for (const auto& value : stmt.values) {
        m_symbolTable.dataSegment.values.push_back(value);
    }
}

// =============================================================================
// Pass 2: Validation
// =============================================================================

void SemanticAnalyzer::pass2_validate(Program& program) {
    for (const auto& line : program.lines) {
        validateProgramLine(*line);
    }
}

void SemanticAnalyzer::validateProgramLine(const ProgramLine& line) {
    m_currentLineNumber = line.lineNumber;
    
    for (const auto& stmt : line.statements) {
        validateStatement(*stmt);
    }
}

void SemanticAnalyzer::validateStatement(const Statement& stmt) {
    switch (stmt.getType()) {
        case ASTNodeType::STMT_PRINT:
            validatePrintStatement(static_cast<const PrintStatement&>(stmt));
            break;
        case ASTNodeType::STMT_CONSOLE:
            validateConsoleStatement(static_cast<const ConsoleStatement&>(stmt));
            break;
        case ASTNodeType::STMT_INPUT:
            validateInputStatement(static_cast<const InputStatement&>(stmt));
            break;
        case ASTNodeType::STMT_INPUT_AT:
            // Check if INPUT AT is being called from within a timer handler
            if (m_inTimerHandler) {
                error(SemanticErrorType::TYPE_MISMATCH,
                      "INPUT AT statement not allowed in timer event handlers. "
                      "Handlers must not block for user input.",
                      stmt.location);
            }
            break;
        case ASTNodeType::STMT_LET:
            validateLetStatement(static_cast<const LetStatement&>(stmt));
            break;
        case ASTNodeType::STMT_GOTO:
            validateGotoStatement(static_cast<const GotoStatement&>(stmt));
            break;
        case ASTNodeType::STMT_GOSUB:
            validateGosubStatement(static_cast<const GosubStatement&>(stmt));
            break;
        case ASTNodeType::STMT_IF:
            validateIfStatement(static_cast<const IfStatement&>(stmt));
            break;
        case ASTNodeType::STMT_FOR:
            validateForStatement(static_cast<const ForStatement&>(stmt));
            break;
        case ASTNodeType::STMT_FOR_IN:
            validateForInStatement(static_cast<const ForInStatement&>(stmt));
            break;
        case ASTNodeType::STMT_NEXT:
            validateNextStatement(static_cast<const NextStatement&>(stmt));
            break;
        case ASTNodeType::STMT_WHILE:
            validateWhileStatement(static_cast<const WhileStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_WEND:
            validateWendStatement(static_cast<const WendStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_REPEAT:
            validateRepeatStatement(static_cast<const RepeatStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_UNTIL:
            validateUntilStatement(static_cast<const UntilStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_DO:
            validateDoStatement(static_cast<const DoStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_LOOP:
            validateLoopStatement(static_cast<const LoopStatement&>(stmt));
            break;
        case ASTNodeType::STMT_READ:
            validateReadStatement(static_cast<const ReadStatement&>(stmt));
            break;
        case ASTNodeType::STMT_RESTORE:
            validateRestoreStatement(static_cast<const RestoreStatement&>(stmt));
            break;
        case ASTNodeType::STMT_ON_EVENT:
            // ONEVENT is deprecated - use AFTER/EVERY instead
            // validateOnEventStatement(static_cast<const OnEventStatement&>(stmt));
            break;
            
        // Timer event statements
        case ASTNodeType::STMT_AFTER:
            validateAfterStatement(static_cast<const AfterStatement&>(stmt));
            break;
        case ASTNodeType::STMT_EVERY:
            validateEveryStatement(static_cast<const EveryStatement&>(stmt));
            break;
        case ASTNodeType::STMT_AFTERFRAMES:
            validateAfterFramesStatement(static_cast<const AfterFramesStatement&>(stmt));
            break;
        case ASTNodeType::STMT_EVERYFRAME:
            validateEveryFrameStatement(static_cast<const EveryFrameStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_RUN:
            validateRunStatement(static_cast<const RunStatement&>(stmt));
            break;
        case ASTNodeType::STMT_TIMER_STOP:
            validateTimerStopStatement(static_cast<const TimerStopStatement&>(stmt));
            break;
        case ASTNodeType::STMT_TIMER_INTERVAL:
            validateTimerIntervalStatement(static_cast<const TimerIntervalStatement&>(stmt));
            break;
            
        case ASTNodeType::STMT_COLOR:
        case ASTNodeType::STMT_WAIT:
        case ASTNodeType::STMT_WAIT_MS:
        case ASTNodeType::STMT_PSET:
        case ASTNodeType::STMT_LINE:
        case ASTNodeType::STMT_RECT:
        case ASTNodeType::STMT_CIRCLE:
        case ASTNodeType::STMT_CIRCLEF:
            validateExpressionStatement(static_cast<const ExpressionStatement&>(stmt));
            break;
        case ASTNodeType::STMT_FUNCTION: {
            const FunctionStatement& funcStmt = static_cast<const FunctionStatement&>(stmt);
            std::string prevFuncName = m_currentFunctionName;
            bool prevInHandler = m_inTimerHandler;
            
            // Set up function scope
            FunctionScope prevScope = m_currentFunctionScope;
            m_currentFunctionScope = FunctionScope();
            m_currentFunctionScope.inFunction = true;
            m_currentFunctionScope.functionName = funcStmt.functionName;
            m_currentFunctionScope.isSub = false;  // This is a FUNCTION
            
            // Set expected return type
            auto* funcSym = lookupFunction(funcStmt.functionName);
            if (funcSym) {
                m_currentFunctionScope.expectedReturnType = funcSym->returnType;
                m_currentFunctionScope.expectedReturnTypeName = funcSym->returnTypeName;
            }
            
            // Add parameters to scope
            for (const auto& param : funcStmt.parameters) {
                m_currentFunctionScope.parameters.insert(param);
            }
            
            m_currentFunctionName = funcStmt.functionName;
            m_inTimerHandler = (m_registeredHandlers.find(funcStmt.functionName) != m_registeredHandlers.end());
            
            // Validate function body (will collect LOCAL/SHARED and check usage)
            for (const auto& bodyStmt : funcStmt.body) {
                validateStatement(*bodyStmt);
            }
            
            // Restore previous scope
            m_currentFunctionScope = prevScope;
            m_currentFunctionName = prevFuncName;
            m_inTimerHandler = prevInHandler;
            break;
        }
        case ASTNodeType::STMT_SUB: {
            const SubStatement& subStmt = static_cast<const SubStatement&>(stmt);
            std::string prevFuncName = m_currentFunctionName;
            bool prevInHandler = m_inTimerHandler;
            
            // Set up function scope
            FunctionScope prevScope = m_currentFunctionScope;
            m_currentFunctionScope = FunctionScope();
            m_currentFunctionScope.inFunction = true;
            m_currentFunctionScope.functionName = subStmt.subName;
            m_currentFunctionScope.isSub = true;  // This is a SUB
            m_currentFunctionScope.expectedReturnType = VariableType::VOID;
            
            // Add parameters to scope
            for (const auto& param : subStmt.parameters) {
                m_currentFunctionScope.parameters.insert(param);
            }
            
            m_currentFunctionName = subStmt.subName;
            m_inTimerHandler = (m_registeredHandlers.find(subStmt.subName) != m_registeredHandlers.end());
            
            // Validate sub body (will collect LOCAL/SHARED and check usage)
            for (const auto& bodyStmt : subStmt.body) {
                validateStatement(*bodyStmt);
            }
            
            // Restore previous scope
            m_currentFunctionScope = prevScope;
            m_currentFunctionName = prevFuncName;
            m_inTimerHandler = prevInHandler;
            break;
        }
        case ASTNodeType::STMT_LOCAL: {
            const LocalStatement& localStmt = static_cast<const LocalStatement&>(stmt);
            
            if (!m_currentFunctionScope.inFunction) {
                error(SemanticErrorType::CONTROL_FLOW_MISMATCH,
                      "LOCAL can only be used inside SUB or FUNCTION",
                      stmt.location);
            }
            
            // Add local variables to function scope
            for (const auto& var : localStmt.variables) {
                // Check for duplicate declaration
                if (m_currentFunctionScope.localVariables.count(var.name) ||
                    m_currentFunctionScope.sharedVariables.count(var.name)) {
                    error(SemanticErrorType::ARRAY_REDECLARED,
                          "Variable '" + var.name + "' already declared in this function",
                          stmt.location);
                }
                
                m_currentFunctionScope.localVariables.insert(var.name);
            }
            break;
        }
        case ASTNodeType::STMT_SHARED: {
            const SharedStatement& sharedStmt = static_cast<const SharedStatement&>(stmt);
            
            if (!m_currentFunctionScope.inFunction) {
                error(SemanticErrorType::CONTROL_FLOW_MISMATCH,
                      "SHARED can only be used inside SUB or FUNCTION",
                      stmt.location);
            }
            
            // Add shared variables to function scope
            for (const auto& var : sharedStmt.variables) {
                // Check for duplicate declaration
                if (m_currentFunctionScope.localVariables.count(var.name) ||
                    m_currentFunctionScope.sharedVariables.count(var.name)) {
                    error(SemanticErrorType::ARRAY_REDECLARED,
                          "Variable '" + var.name + "' already declared in this function",
                          stmt.location);
                }
                
                // Verify the variable exists at module level
                if (!lookupVariable(var.name)) {
                    error(SemanticErrorType::UNDEFINED_VARIABLE,
                          "SHARED variable '" + var.name + "' is not defined at module level",
                          stmt.location);
                }
                
                m_currentFunctionScope.sharedVariables.insert(var.name);
            }
            break;
        }
        case ASTNodeType::STMT_RETURN:
            validateReturnStatement(static_cast<const ReturnStatement&>(stmt));
            break;
        default:
            // Other statements don't need special validation
            break;
    }
}

void SemanticAnalyzer::validatePrintStatement(const PrintStatement& stmt) {
    for (const auto& item : stmt.items) {
        validateExpression(*item.expr);
    }
}

void SemanticAnalyzer::validateConsoleStatement(const ConsoleStatement& stmt) {
    for (const auto& item : stmt.items) {
        validateExpression(*item.expr);
    }
}

void SemanticAnalyzer::validateInputStatement(const InputStatement& stmt) {
    // Check if INPUT is being called from within a timer handler
    if (m_inTimerHandler) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "INPUT statement not allowed in timer event handlers. "
              "Handlers must not block for user input.",
              stmt.location);
    }
    
    for (const auto& varName : stmt.variables) {
        useVariable(varName, stmt.location);
    }
}

void SemanticAnalyzer::validateLetStatement(const LetStatement& stmt) {
    // Detect whole-array SIMD operations: A() = B() + C()
    // Check if left side is whole-array access (array with empty indices)
    if (!stmt.indices.empty() && stmt.indices.size() == 0) {
        // This is never true, but kept for clarity - empty() means size() == 0
    }
    
    // Check for whole-array assignment pattern
    bool isWholeArrayAssignment = false;
    if (stmt.indices.empty()) {
        // Could be either scalar variable or whole array
        // Check if this variable is declared as an array
        auto* arraySym = lookupArray(stmt.variable);
        if (arraySym) {
            isWholeArrayAssignment = true;
            
            // Check if the array is of a SIMD-capable type
            if (!arraySym->asTypeName.empty()) {
                auto* typeSym = lookupType(arraySym->asTypeName);
                if (typeSym && typeSym->simdType != TypeDeclarationStatement::SIMDType::NONE) {
                    // This is a SIMD-capable array assignment!
                    const char* simdTypeStr = (typeSym->simdType == TypeDeclarationStatement::SIMDType::PAIR) ? "PAIR" : "QUAD";
                    std::cout << "[SIMD] Detected whole-array assignment to SIMD type " 
                              << arraySym->asTypeName << " [" << simdTypeStr << "]: "
                              << stmt.variable << "() = <expression>" << std::endl;
                    
                    // Analyze right-hand side expression
                    analyzeArrayExpression(stmt.value.get(), typeSym->simdType);
                }
            }
        }
    }
    
    // Check if assigning to a FOR loop index variable (not allowed in compiled loops)
    if (stmt.indices.empty() && !isWholeArrayAssignment) {  // Only check simple variable assignment, not arrays
        // Check if this variable is an active FOR loop index
        std::stack<ForContext> tempStack = m_forStack;
        while (!tempStack.empty()) {
            const ForContext& ctx = tempStack.top();
            if (ctx.variable == stmt.variable) {
                // Found assignment to loop index!
                warning("Assignment to FOR loop index variable '" + stmt.variable + "' detected.\n"
                       "  This pattern does NOT work for early loop exit in compiled loops.\n"
                       "  The loop will continue to its original limit.\n"
                       "  SOLUTION: Use 'EXIT FOR' instead of '" + stmt.variable + " = <value>'",
                       stmt.location);
                break;
            }
            tempStack.pop();
        }
    }
    
    // Validate array indices if present
    for (const auto& index : stmt.indices) {
        validateExpression(*index);
        VariableType indexType = inferExpressionType(*index);
        if (!isNumericType(indexType)) {
            error(SemanticErrorType::INVALID_ARRAY_INDEX,
                  "Array index must be numeric",
                  stmt.location);
        }
    }
    
    // Check if array assignment
    if (!stmt.indices.empty()) {
        useArray(stmt.variable, stmt.indices.size(), stmt.location);
    } else {
        // Check variable declaration in function context
        if (m_currentFunctionScope.inFunction) {
            validateVariableInFunction(stmt.variable, stmt.location);
        } else {
            useVariable(stmt.variable, stmt.location);
        }
    }
    
    // Validate value expression
    validateExpression(*stmt.value);
    
    // Type check
    VariableType targetType;
    if (!stmt.indices.empty()) {
        auto* arraySym = lookupArray(stmt.variable);
        targetType = arraySym ? arraySym->type : VariableType::UNKNOWN;
    } else {
        auto* varSym = lookupVariable(stmt.variable);
        targetType = varSym ? varSym->type : VariableType::UNKNOWN;
    }
    
    VariableType valueType = inferExpressionType(*stmt.value);
    checkTypeCompatibility(targetType, valueType, stmt.location, "assignment");
}

void SemanticAnalyzer::validateGotoStatement(const GotoStatement& stmt) {
    if (stmt.isLabel) {
        // Symbolic label - resolve it
        auto* labelSym = lookupLabel(stmt.label);
        if (!labelSym) {
            error(SemanticErrorType::UNDEFINED_LABEL,
                  "GOTO target label :" + stmt.label + " does not exist",
                  stmt.location);
        } else {
            labelSym->references.push_back(stmt.location);
        }
    } else {
        // Line number
        auto* lineSym = lookupLine(stmt.lineNumber);
        if (!lineSym) {
            error(SemanticErrorType::UNDEFINED_LINE,
                  "GOTO target line " + std::to_string(stmt.lineNumber) + " does not exist",
                  stmt.location);
        } else {
            lineSym->references.push_back(stmt.location);
        }
    }
}

void SemanticAnalyzer::validateGosubStatement(const GosubStatement& stmt) {
    if (stmt.isLabel) {
        // Symbolic label - resolve it
        auto* labelSym = lookupLabel(stmt.label);
        if (!labelSym) {
            error(SemanticErrorType::UNDEFINED_LABEL,
                  "GOSUB target label :" + stmt.label + " does not exist",
                  stmt.location);
        } else {
            labelSym->references.push_back(stmt.location);
        }
    } else {
        // Line number
        auto* lineSym = lookupLine(stmt.lineNumber);
        if (!lineSym) {
            error(SemanticErrorType::UNDEFINED_LINE,
                  "GOSUB target line " + std::to_string(stmt.lineNumber) + " does not exist",
                  stmt.location);
        } else {
            lineSym->references.push_back(stmt.location);
        }
    }
}

void SemanticAnalyzer::validateIfStatement(const IfStatement& stmt) {
    validateExpression(*stmt.condition);
    
    if (stmt.hasGoto) {
        auto* lineSym = lookupLine(stmt.gotoLine);
        if (!lineSym) {
            error(SemanticErrorType::UNDEFINED_LINE,
                  "IF THEN target line " + std::to_string(stmt.gotoLine) + " does not exist",
                  stmt.location);
        } else {
            lineSym->references.push_back(stmt.location);
        }
    } else {
        for (const auto& thenStmt : stmt.thenStatements) {
            validateStatement(*thenStmt);
        }
    }
    
    for (const auto& elseStmt : stmt.elseStatements) {
        validateStatement(*elseStmt);
    }
}

void SemanticAnalyzer::validateForStatement(const ForStatement& stmt) {
    // Declare/use loop variable
    if (m_currentFunctionScope.inFunction) {
        validateVariableInFunction(stmt.variable, stmt.location);
    } else {
        useVariable(stmt.variable, stmt.location);
    }
    
    // Validate expressions
    validateExpression(*stmt.start);
    validateExpression(*stmt.end);
    if (stmt.step) {
        validateExpression(*stmt.step);
    }
    
    // Type check
    VariableType varType = inferTypeFromName(stmt.variable);
    VariableType startType = inferExpressionType(*stmt.start);
    VariableType endType = inferExpressionType(*stmt.end);
    
    if (!isNumericType(startType) || !isNumericType(endType)) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "FOR loop bounds must be numeric",
              stmt.location);
    }
    
    // Push to control flow stack
    ForContext ctx;
    ctx.variable = stmt.variable;
    ctx.location = stmt.location;
    m_forStack.push(ctx);
}

void SemanticAnalyzer::validateForInStatement(const ForInStatement& stmt) {
    // Declare/use loop variable
    if (m_currentFunctionScope.inFunction) {
        validateVariableInFunction(stmt.variable, stmt.location);
    } else {
        useVariable(stmt.variable, stmt.location);
    }
    
    // Declare/use optional index variable
    if (!stmt.indexVariable.empty()) {
        if (m_currentFunctionScope.inFunction) {
            validateVariableInFunction(stmt.indexVariable, stmt.location);
        } else {
            useVariable(stmt.indexVariable, stmt.location);
        }
    }
    
    // Validate array expression
    validateExpression(*stmt.array);
    
    // Type check - array expression should be an array access
    VariableType arrayType = inferExpressionType(*stmt.array);
    
    // The array should be a valid array reference
    // For now, we'll allow any type but could add stricter checking later
    
    // Push to control flow stack (reuse ForContext for simplicity)
    ForContext ctx;
    ctx.variable = stmt.variable;
    ctx.location = stmt.location;
    m_forStack.push(ctx);
}

void SemanticAnalyzer::validateNextStatement(const NextStatement& stmt) {
    if (m_forStack.empty()) {
        error(SemanticErrorType::NEXT_WITHOUT_FOR,
              "NEXT without matching FOR",
              stmt.location);
    } else {
        const auto& forCtx = m_forStack.top();
        
        // Check variable match if specified
        if (!stmt.variable.empty() && stmt.variable != forCtx.variable) {
            error(SemanticErrorType::CONTROL_FLOW_MISMATCH,
                  "NEXT variable '" + stmt.variable + "' does not match FOR variable '" + 
                  forCtx.variable + "'",
                  stmt.location);
        }
        
        m_forStack.pop();
    }
}

void SemanticAnalyzer::validateWhileStatement(const WhileStatement& stmt) {
    validateExpression(*stmt.condition);
    m_whileStack.push(stmt.location);
}

void SemanticAnalyzer::validateWendStatement(const WendStatement& stmt) {
    if (m_whileStack.empty()) {
        error(SemanticErrorType::WEND_WITHOUT_WHILE,
              "WEND without matching WHILE",
              stmt.location);
    } else {
        m_whileStack.pop();
    }
}

void SemanticAnalyzer::validateRepeatStatement(const RepeatStatement& stmt) {
    m_repeatStack.push(stmt.location);
}

void SemanticAnalyzer::validateUntilStatement(const UntilStatement& stmt) {
    if (m_repeatStack.empty()) {
        error(SemanticErrorType::UNTIL_WITHOUT_REPEAT,
              "UNTIL without matching REPEAT",
              stmt.location);
    } else {
        m_repeatStack.pop();
    }
    
    validateExpression(*stmt.condition);
}

void SemanticAnalyzer::validateDoStatement(const DoStatement& stmt) {
    // Validate condition if present (DO WHILE or DO UNTIL)
    if (stmt.condition) {
        validateExpression(*stmt.condition);
    }
    m_doStack.push(stmt.location);
}

void SemanticAnalyzer::validateLoopStatement(const LoopStatement& stmt) {
    if (m_doStack.empty()) {
        error(SemanticErrorType::LOOP_WITHOUT_DO,
              "LOOP without matching DO",
              stmt.location);
    } else {
        m_doStack.pop();
    }
    
    // Validate condition if present (LOOP WHILE or LOOP UNTIL)
    if (stmt.condition) {
        validateExpression(*stmt.condition);
    }
}

void SemanticAnalyzer::validateReadStatement(const ReadStatement& stmt) {
    for (const auto& varName : stmt.variables) {
        useVariable(varName, stmt.location);
    }
}

void SemanticAnalyzer::validateRestoreStatement(const RestoreStatement& stmt) {
    // RESTORE targets can be:
    // 1. Regular labels/lines in the program (checked here)
    // 2. DATA labels/lines (handled by DataManager at runtime)
    // So we don't error if not found - just record the reference if it exists
    
    if (stmt.isLabel) {
        // Symbolic label - try to resolve it
        auto* labelSym = lookupLabel(stmt.label);
        if (labelSym) {
            // Found in symbol table - record reference
            labelSym->references.push_back(stmt.location);
        }
        // If not found, assume it's a DATA label - will be resolved at runtime
    } else if (stmt.lineNumber > 0) {
        auto* lineSym = lookupLine(stmt.lineNumber);
        // If not found, assume it's a DATA line - will be resolved at runtime
        // No error needed - DataManager will handle it
    }
}

void SemanticAnalyzer::validateExpressionStatement(const ExpressionStatement& stmt) {
    for (const auto& arg : stmt.arguments) {
        validateExpression(*arg);
    }
}

void SemanticAnalyzer::validateOnEventStatement(const OnEventStatement& stmt) {
    // ONEVENT is deprecated - use AFTER/EVERY instead
    // This function is kept for backwards compatibility but does nothing
    (void)stmt; // Suppress unused parameter warning
}

// =============================================================================
// Timer Event Statement Validation
// =============================================================================

void SemanticAnalyzer::validateAfterStatement(const AfterStatement& stmt) {
    // Validate duration expression
    if (stmt.duration) {
        validateExpression(*stmt.duration);
        VariableType durationType = inferExpressionType(*stmt.duration);
        
        if (!isNumericType(durationType)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "AFTER duration must be numeric (milliseconds)",
                  stmt.location);
        }
        
        // Try to evaluate as constant and check if positive
        try {
            auto constVal = evaluateConstantExpression(*stmt.duration);
            double duration = 0.0;
            
            if (std::holds_alternative<int64_t>(constVal)) {
                duration = static_cast<double>(std::get<int64_t>(constVal));
            } else if (std::holds_alternative<double>(constVal)) {
                duration = std::get<double>(constVal);
            }
            
            if (duration < 0.0) {
                error(SemanticErrorType::TYPE_MISMATCH,
                      "AFTER duration must be non-negative",
                      stmt.location);
            }
        } catch (...) {
            // Not a constant expression - will be checked at runtime
        }
    }
    
    // Validate handler exists and is a SUB/FUNCTION
    if (!stmt.handlerName.empty()) {
        // If this is an inline handler (using DO...DONE syntax), register it as a function
        if (stmt.isInlineHandler) {
            // Create a function symbol for the inline handler
            FunctionSymbol funcSym;
            funcSym.name = stmt.handlerName;
            funcSym.returnType = VariableType::VOID;  // SUBs have no return type
            funcSym.definition = stmt.location;
            m_symbolTable.functions[stmt.handlerName] = funcSym;
            
            // Validate the inline body statements
            for (const auto& bodyStmt : stmt.inlineBody) {
                validateStatement(*bodyStmt);
            }
        } else {
            // External handler - must exist
            auto* funcSym = lookupFunction(stmt.handlerName);
            if (!funcSym) {
                error(SemanticErrorType::UNDEFINED_FUNCTION,
                      "AFTER handler '" + stmt.handlerName + "' is not defined. Handlers must be SUB or FUNCTION declarations.",
                      stmt.location);
            } else {
                // Handler should have zero parameters
                // Warn if handler has parameters
                if (!funcSym->parameters.empty()) {
                    warning("Timer handler '" + stmt.handlerName + "' has parameters but will be called with no arguments",
                           stmt.location);
                }
            }
        }
    }
}

void SemanticAnalyzer::validateEveryStatement(const EveryStatement& stmt) {
    // Validate duration expression
    if (stmt.duration) {
        validateExpression(*stmt.duration);
        VariableType durationType = inferExpressionType(*stmt.duration);
        
        if (!isNumericType(durationType)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "EVERY interval must be numeric (milliseconds)",
                  stmt.location);
        }
    }
    
    // Validate handler exists
    if (!stmt.handlerName.empty()) {
        // If this is an inline handler (using DO...DONE syntax), register it as a function
        if (stmt.isInlineHandler) {
            // Create a function symbol for the inline handler
            FunctionSymbol funcSym;
            funcSym.name = stmt.handlerName;
            funcSym.returnType = VariableType::VOID;  // SUBs have no return type
            funcSym.definition = stmt.location;
            m_symbolTable.functions[stmt.handlerName] = funcSym;
            
            // Validate the inline body statements
            for (const auto& bodyStmt : stmt.inlineBody) {
                validateStatement(*bodyStmt);
            }
        } else {
            // External handler - must exist
            auto* funcSym = lookupFunction(stmt.handlerName);
            if (!funcSym) {
                error(SemanticErrorType::UNDEFINED_FUNCTION,
                      "EVERY handler '" + stmt.handlerName + "' is not defined. Handlers must be SUB or FUNCTION declarations.",
                      stmt.location);
            } else {
                // Handler should have zero parameters
                // Warn if handler has parameters
                if (!funcSym->parameters.empty()) {
                    warning("Timer handler '" + stmt.handlerName + "' has parameters but will be called with no arguments",
                           stmt.location);
                }
            }
        }
    }
}

void SemanticAnalyzer::validateAfterFramesStatement(const AfterFramesStatement& stmt) {
    // Validate frame count expression
    if (stmt.frameCount) {
        validateExpression(*stmt.frameCount);
        VariableType frameCountType = inferExpressionType(*stmt.frameCount);
        
        if (!isNumericType(frameCountType)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "AFTERFRAMES count must be numeric (frames)",
                  stmt.location);
        }
    }
    
    // Validate handler exists
    if (!stmt.handlerName.empty()) {
        auto* funcSym = lookupFunction(stmt.handlerName);
        if (!funcSym) {
            error(SemanticErrorType::UNDEFINED_FUNCTION,
                  "AFTERFRAMES handler '" + stmt.handlerName + "' is not defined. Handlers must be SUB or FUNCTION declarations.",
                  stmt.location);
        } else {
            // Handler should have zero parameters
            // Warn if handler has parameters
            if (!funcSym->parameters.empty()) {
                warning("Timer handler '" + stmt.handlerName + "' has parameters but will be called with no arguments",
                       stmt.location);
            }
        }
    }
}

void SemanticAnalyzer::validateEveryFrameStatement(const EveryFrameStatement& stmt) {
    // Validate frame count expression
    if (stmt.frameCount) {
        validateExpression(*stmt.frameCount);
        VariableType frameCountType = inferExpressionType(*stmt.frameCount);
        
        if (!isNumericType(frameCountType)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "EVERYFRAME count must be numeric (frames)",
                  stmt.location);
        }
    }
    
    // Validate handler exists
    if (!stmt.handlerName.empty()) {
        auto* funcSym = lookupFunction(stmt.handlerName);
        if (!funcSym) {
            error(SemanticErrorType::UNDEFINED_FUNCTION,
                  "EVERYFRAME handler '" + stmt.handlerName + "' is not defined. Handlers must be SUB or FUNCTION declarations.",
                  stmt.location);
        } else {
            // Handler should have zero parameters
            // Warn if handler has parameters
            if (!funcSym->parameters.empty()) {
                warning("Timer handler '" + stmt.handlerName + "' has parameters but will be called with no arguments",
                       stmt.location);
            }
        }
    }
}

void SemanticAnalyzer::validateRunStatement(const RunStatement& stmt) {
    // Validate UNTIL condition if present
    if (stmt.untilCondition) {
        validateExpression(*stmt.untilCondition);
        // Condition should be boolean/numeric (any type that can be evaluated as true/false)
        // No strict type checking needed - BASIC allows any type in conditions
    }
}

void SemanticAnalyzer::validateTimerStopStatement(const TimerStopStatement& stmt) {
    // Validate based on stop target type
    switch (stmt.targetType) {
        case TimerStopStatement::StopTarget::TIMER_ID:
            // Validate timer ID expression if present
            if (stmt.timerId) {
                validateExpression(*stmt.timerId);
                VariableType idType = inferExpressionType(*stmt.timerId);
                if (!isNumericType(idType)) {
                    error(SemanticErrorType::TYPE_MISMATCH,
                          "TIMER STOP timer ID must be numeric",
                          stmt.location);
                }
            }
            break;
            
        case TimerStopStatement::StopTarget::HANDLER:
            // Validate handler name exists
            if (!stmt.handlerName.empty()) {
                auto* funcSym = lookupFunction(stmt.handlerName);
                if (!funcSym) {
                    error(SemanticErrorType::UNDEFINED_FUNCTION,
                          "TIMER STOP handler '" + stmt.handlerName + "' is not defined",
                          stmt.location);
                }
            }
            break;
            
        case TimerStopStatement::StopTarget::ALL:
            // No validation needed for STOP ALL
            break;
    }
}

void SemanticAnalyzer::validateTimerIntervalStatement(const TimerIntervalStatement& stmt) {
    // Validate interval expression
    if (stmt.interval) {
        validateExpression(*stmt.interval);
        VariableType intervalType = inferExpressionType(*stmt.interval);
        
        if (!isNumericType(intervalType)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "TIMER INTERVAL must be numeric (instruction count)",
                  stmt.location);
        }
        
        // Try to evaluate as constant and check if positive
        try {
            auto constVal = evaluateConstantExpression(*stmt.interval);
            int64_t interval = 0;
            
            if (std::holds_alternative<int64_t>(constVal)) {
                interval = std::get<int64_t>(constVal);
            } else if (std::holds_alternative<double>(constVal)) {
                interval = static_cast<int64_t>(std::get<double>(constVal));
            }
            
            if (interval <= 0) {
                error(SemanticErrorType::TYPE_MISMATCH,
                      "TIMER INTERVAL must be positive",
                      stmt.location);
            }
            
            if (interval > 1000000) {
                warning("TIMER INTERVAL of " + std::to_string(interval) + 
                       " is very high - may reduce timer responsiveness significantly",
                       stmt.location);
            } else if (interval < 100) {
                warning("TIMER INTERVAL of " + std::to_string(interval) + 
                       " is very low - may increase CPU usage significantly",
                       stmt.location);
            }
        } catch (...) {
            // Not a constant expression - will be checked at runtime
        }
    }
}

// =============================================================================
// Expression Validation and Type Inference
// =============================================================================

void SemanticAnalyzer::analyzeArrayExpression(const Expression* expr, TypeDeclarationStatement::SIMDType targetSIMDType) {
    if (!expr) return;
    
    // For now, just detect simple array copy: A() = B()
    if (expr->getType() == ASTNodeType::EXPR_ARRAY_ACCESS) {
        auto* arrayAccess = static_cast<const ArrayAccessExpression*>(expr);
        if (arrayAccess->indices.empty()) {
            std::cout << "[SIMD] Detected whole-array copy: <target>() = " 
                      << arrayAccess->name << "()" << std::endl;
            
            // Check if source array is also SIMD-capable
            auto* arraySym = lookupArray(arrayAccess->name);
            if (arraySym && !arraySym->asTypeName.empty()) {
                auto* typeSym = lookupType(arraySym->asTypeName);
                if (typeSym && typeSym->simdType == targetSIMDType) {
                    std::cout << "[SIMD] Source and target are compatible SIMD types - can optimize!" << std::endl;
                }
            }
        }
    }
    
    // TODO: Detect binary operations on arrays (A() + B(), etc.)
    // This will require understanding how expressions are represented in the AST
}

void SemanticAnalyzer::validateExpression(const Expression& expr) {
    // This also performs type inference as a side effect
    inferExpressionType(expr);
}

void SemanticAnalyzer::validateReturnStatement(const ReturnStatement& stmt) {
    // Check if we're in a function or subroutine
    if (!m_currentFunctionScope.inFunction) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "RETURN statement outside of FUNCTION or SUB",
              stmt.location);
        return;
    }
    
    if (m_currentFunctionScope.isSub) {
        // In a SUB - should not have a return value
        if (stmt.returnValue) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "SUB " + m_currentFunctionScope.functionName + " cannot return a value",
                  stmt.location);
        }
    } else {
        // In a FUNCTION - must have a return value
        if (!stmt.returnValue) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "FUNCTION " + m_currentFunctionScope.functionName + " must return a value",
                  stmt.location);
            return;
        }
        
        // Validate return value expression
        validateExpression(*stmt.returnValue);
        
        // Check return type compatibility
        VariableType returnType = inferExpressionType(*stmt.returnValue);
        VariableType expectedType = m_currentFunctionScope.expectedReturnType;
        std::string expectedTypeName = m_currentFunctionScope.expectedReturnTypeName;
        
        // Skip validation if expected type is unknown
        if (expectedType == VariableType::UNKNOWN && expectedTypeName.empty()) {
            return;
        }
        
        // For user-defined return types
        if (!expectedTypeName.empty()) {
            // Returning a user-defined type
            // We need to check if the return expression is of the right user-defined type
            // For now, just ensure it's not a primitive type
            if (isNumericType(returnType) || returnType == VariableType::STRING) {
                error(SemanticErrorType::TYPE_MISMATCH,
                      "FUNCTION " + m_currentFunctionScope.functionName + 
                      " expects return type " + expectedTypeName + ", got " + typeToString(returnType),
                      stmt.location);
            }
        } else {
            // Built-in return type - check compatibility
            bool compatible = false;
            
            if (isNumericType(expectedType) && isNumericType(returnType)) {
                compatible = true;  // Allow numeric conversions
            } else if (expectedType == returnType) {
                compatible = true;  // Exact match
            } else if (expectedType == VariableType::STRING && 
                      (returnType == VariableType::STRING || returnType == VariableType::UNICODE)) {
                compatible = true;  // String types are compatible
            } else if (expectedType == VariableType::UNICODE && 
                      (returnType == VariableType::STRING || returnType == VariableType::UNICODE)) {
                compatible = true;
            }
            
            if (!compatible) {
                error(SemanticErrorType::TYPE_MISMATCH,
                      "FUNCTION " + m_currentFunctionScope.functionName + 
                      " expects return type " + typeToString(expectedType) + 
                      ", got " + typeToString(returnType),
                      stmt.location);
            }
        }
    }
}

VariableType SemanticAnalyzer::inferExpressionType(const Expression& expr) {
    switch (expr.getType()) {
        case ASTNodeType::EXPR_NUMBER:
            return VariableType::FLOAT;
        
        case ASTNodeType::EXPR_STRING:
            // Return UNICODE type if in Unicode mode
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        
        case ASTNodeType::EXPR_VARIABLE:
            return inferVariableType(static_cast<const VariableExpression&>(expr));
        
        case ASTNodeType::EXPR_ARRAY_ACCESS:
            return inferArrayAccessType(static_cast<const ArrayAccessExpression&>(expr));
        
        case ASTNodeType::EXPR_FUNCTION_CALL:
            // Check if this is actually a RegistryFunctionExpression
            if (auto* regFunc = dynamic_cast<const RegistryFunctionExpression*>(&expr)) {
                return inferRegistryFunctionType(*regFunc);
            } else {
                return inferFunctionCallType(static_cast<const FunctionCallExpression&>(expr));
            }
        
        case ASTNodeType::EXPR_BINARY:
            return inferBinaryExpressionType(static_cast<const BinaryExpression&>(expr));
        
        case ASTNodeType::EXPR_UNARY:
            return inferUnaryExpressionType(static_cast<const UnaryExpression&>(expr));
        
        default:
            return VariableType::UNKNOWN;
    }
}

VariableType SemanticAnalyzer::inferMemberAccessType(const MemberAccessExpression& expr) {
    // Infer the type of a member access expression (e.g., point.X)
    
    // First, determine the type of the base object
    VariableType baseType = VariableType::UNKNOWN;
    std::string baseTypeName;
    
    // Check if the object is a variable
    if (expr.object->getType() == ASTNodeType::EXPR_VARIABLE) {
        const VariableExpression* varExpr = static_cast<const VariableExpression*>(expr.object.get());
        VariableSymbol* varSym = lookupVariable(varExpr->name);
        if (varSym) {
            baseType = varSym->type;
            // For user-defined types, we need to find the type name
            // Variables of user-defined types store the type name (we'll enhance this later)
        }
    } else if (expr.object->getType() == ASTNodeType::EXPR_ARRAY_ACCESS) {
        // Array element access
        const ArrayAccessExpression* arrayExpr = static_cast<const ArrayAccessExpression*>(expr.object.get());
        ArraySymbol* arraySym = lookupArray(arrayExpr->name);
        if (arraySym) {
            baseType = arraySym->type;
        }
    } else if (expr.object->getType() == ASTNodeType::EXPR_MEMBER_ACCESS) {
        // Nested member access (e.g., a.b.c)
        baseType = inferMemberAccessType(*static_cast<const MemberAccessExpression*>(expr.object.get()));
    }
    
    // For now, return UNKNOWN - full implementation requires tracking type names in variables
    // This will be completed when we integrate DIM AS TypeName
    return VariableType::UNKNOWN;
}

VariableType SemanticAnalyzer::inferBinaryExpressionType(const BinaryExpression& expr) {
    VariableType leftType = inferExpressionType(*expr.left);
    VariableType rightType = inferExpressionType(*expr.right);
    
    // String concatenation
    if (leftType == VariableType::STRING || rightType == VariableType::STRING ||
        leftType == VariableType::UNICODE || rightType == VariableType::UNICODE) {
        if (expr.op == TokenType::PLUS) {
            // If either is UNICODE, result is UNICODE
            if (leftType == VariableType::UNICODE || rightType == VariableType::UNICODE) {
                return VariableType::UNICODE;
            }
            return VariableType::STRING;
        }
    }
    
    // Comparison operators return numeric
    if (expr.op >= TokenType::EQUAL && expr.op <= TokenType::GREATER_EQUAL) {
        return VariableType::FLOAT;
    }
    
    // Logical operators return numeric
    if (expr.op == TokenType::AND || expr.op == TokenType::OR) {
        return VariableType::FLOAT;
    }
    
    // Arithmetic operators
    return promoteTypes(leftType, rightType);
}

VariableType SemanticAnalyzer::inferUnaryExpressionType(const UnaryExpression& expr) {
    VariableType exprType = inferExpressionType(*expr.expr);
    
    if (expr.op == TokenType::NOT) {
        return VariableType::FLOAT;
    }
    
    // Unary + or -
    return exprType;
}

VariableType SemanticAnalyzer::inferVariableType(const VariableExpression& expr) {
    // Check variable declaration in function context
    if (m_currentFunctionScope.inFunction) {
        validateVariableInFunction(expr.name, expr.location);
        
        // For LOCAL variables and parameters, infer type from name
        // (they're not in the symbol table)
        if (m_currentFunctionScope.parameters.count(expr.name) ||
            m_currentFunctionScope.localVariables.count(expr.name)) {
            return inferTypeFromName(expr.name);
        }
        
        // For SHARED variables, look up in symbol table
        if (m_currentFunctionScope.sharedVariables.count(expr.name)) {
            auto* sym = lookupVariable(expr.name);
            if (sym) {
                return sym->type;
            }
            return inferTypeFromName(expr.name);
        }
        
        // Function name (for return value assignment)
        if (expr.name == m_currentFunctionScope.functionName) {
            return inferTypeFromName(expr.name);
        }
    } else {
        useVariable(expr.name, expr.location);
        
        auto* sym = lookupVariable(expr.name);
        if (sym) {
            return sym->type;
        }
    }
    
    return inferTypeFromName(expr.name);
}

VariableType SemanticAnalyzer::inferArrayAccessType(const ArrayAccessExpression& expr) {
    // Check if this is a function/sub call first
    if (m_symbolTable.functions.find(expr.name) != m_symbolTable.functions.end()) {
        // It's a function or sub call - validate arguments but don't treat as array
        const auto& funcSym = m_symbolTable.functions.at(expr.name);
        for (const auto& arg : expr.indices) {
            validateExpression(*arg);
        }
        return funcSym.returnType;
    }
    
    // Check symbol table - if it's a declared array, treat as array access
    auto* arraySym = lookupArray(expr.name);
    if (arraySym) {
        // This is a declared array - validate as array access
        useArray(expr.name, expr.indices.size(), expr.location);
        
        // Validate indices
        for (const auto& index : expr.indices) {
            validateExpression(*index);
            VariableType indexType = inferExpressionType(*index);
            if (!isNumericType(indexType)) {
                error(SemanticErrorType::INVALID_ARRAY_INDEX,
                      "Array index must be numeric",
                      expr.location);
            }
        }
        
        return arraySym->type;
    }
    
    // Not a declared array - check if it's a built-in function call
    if (isBuiltinFunction(expr.name)) {
        // Validate argument count
        int expectedArgs = getBuiltinArgCount(expr.name);
        if (expectedArgs >= 0 && static_cast<int>(expr.indices.size()) != expectedArgs) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Built-in function " + expr.name + " expects " + 
                  std::to_string(expectedArgs) + " argument(s), got " + 
                  std::to_string(expr.indices.size()),
                  expr.location);
        }
        
        // Validate arguments
        for (const auto& index : expr.indices) {
            validateExpression(*index);
        }
        
        return getBuiltinReturnType(expr.name);
    }
    
    // Not an array and not a built-in function - treat as undeclared array
    // (useArray will create an implicit array symbol if needed)
    useArray(expr.name, expr.indices.size(), expr.location);
    
    // Validate indices for the implicit array
    for (const auto& index : expr.indices) {
        validateExpression(*index);
        VariableType indexType = inferExpressionType(*index);
        if (!isNumericType(indexType)) {
            error(SemanticErrorType::INVALID_ARRAY_INDEX,
                  "Array index must be numeric",
                  expr.location);
        }
    }
    
    // Return type for implicit array (lookup again after useArray)
    arraySym = lookupArray(expr.name);
    if (arraySym) {
        return arraySym->type;
    }
    return VariableType::UNKNOWN;
}

VariableType SemanticAnalyzer::inferFunctionCallType(const FunctionCallExpression& expr) {
    // Validate arguments
    for (const auto& arg : expr.arguments) {
        validateExpression(*arg);
    }
    
    if (expr.isFN) {
        // User-defined function (DEF FN or FUNCTION statement)
        auto* sym = lookupFunction(expr.name);
        if (sym) {
            // Validate parameter count
            if (expr.arguments.size() != sym->parameters.size()) {
                error(SemanticErrorType::ARGUMENT_COUNT_MISMATCH,
                      "Function " + expr.name + " expects " + std::to_string(sym->parameters.size()) +
                      " arguments, got " + std::to_string(expr.arguments.size()),
                      expr.location);
                return sym->returnType;
            }
            
            // Validate parameter types
            for (size_t i = 0; i < expr.arguments.size() && i < sym->parameterTypes.size(); ++i) {
                VariableType argType = inferExpressionType(*expr.arguments[i]);
                VariableType paramType = sym->parameterTypes[i];
                std::string paramTypeName = i < sym->parameterTypeNames.size() ? sym->parameterTypeNames[i] : "";
                
                // Skip validation if parameter type is unknown (untyped parameter)
                if (paramType == VariableType::UNKNOWN && paramTypeName.empty()) {
                    continue;
                }
                
                // For user-defined types, check type compatibility
                if (!paramTypeName.empty()) {
                    // Parameter is a user-defined type
                    // Check if argument is also the same user-defined type
                    // For now, we need to track the type name of the argument expression
                    // This requires expression type tracking which we'll enhance
                    // For now, just ensure it's not a built-in numeric type when expecting user type
                    if (isNumericType(argType) || argType == VariableType::STRING) {
                        error(SemanticErrorType::TYPE_MISMATCH,
                              "Parameter " + std::to_string(i + 1) + " of function " + expr.name +
                              " expects user-defined type " + paramTypeName + ", got " + typeToString(argType),
                              expr.location);
                    }
                } else {
                    // Built-in type - check compatibility
                    // Allow implicit numeric conversions (INT -> FLOAT, etc.)
                    bool compatible = false;
                    if (isNumericType(paramType) && isNumericType(argType)) {
                        compatible = true;  // Allow numeric conversions
                    } else if (paramType == argType) {
                        compatible = true;  // Exact match
                    } else if (paramType == VariableType::STRING && 
                              (argType == VariableType::STRING || argType == VariableType::UNICODE)) {
                        compatible = true;  // String types are compatible
                    } else if (paramType == VariableType::UNICODE && 
                              (argType == VariableType::STRING || argType == VariableType::UNICODE)) {
                        compatible = true;
                    }
                    
                    if (!compatible) {
                        error(SemanticErrorType::TYPE_MISMATCH,
                              "Parameter " + std::to_string(i + 1) + " of function " + expr.name +
                              " expects " + typeToString(paramType) + ", got " + typeToString(argType),
                              expr.location);
                    }
                }
            }
            
            // Return the function's return type
            return sym->returnType;
        } else {
            error(SemanticErrorType::UNDEFINED_FUNCTION,
                  "Undefined function FN" + expr.name,
                  expr.location);
            return VariableType::UNKNOWN;
        }
    } else {
        // Built-in function - most return FLOAT
        return VariableType::FLOAT;
    }
}

VariableType SemanticAnalyzer::inferRegistryFunctionType(const RegistryFunctionExpression& expr) {
    // Validate arguments
    for (const auto& arg : expr.arguments) {
        validateExpression(*arg);
    }
    
    // Convert ModularCommands::ReturnType to VariableType
    switch (expr.returnType) {
        case FasterBASIC::ModularCommands::ReturnType::INT:
            return VariableType::INT;
        case FasterBASIC::ModularCommands::ReturnType::FLOAT:
            return VariableType::FLOAT;
        case FasterBASIC::ModularCommands::ReturnType::STRING:
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        case FasterBASIC::ModularCommands::ReturnType::BOOL:
            return VariableType::INT; // BASIC treats booleans as integers
        case FasterBASIC::ModularCommands::ReturnType::VOID:
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Registry function " + expr.name + " has invalid return type",
                  expr.location);
            return VariableType::UNKNOWN;
    }
}

// =============================================================================
// Type Checking
// =============================================================================

void SemanticAnalyzer::checkTypeCompatibility(VariableType expected, VariableType actual,
                                              const SourceLocation& loc, const std::string& context) {
    if (expected == VariableType::UNKNOWN || actual == VariableType::UNKNOWN) {
        return;  // Can't check
    }
    
    // String to numeric or vice versa is an error
    bool expectedString = (expected == VariableType::STRING || expected == VariableType::UNICODE);
    bool actualString = (actual == VariableType::STRING || actual == VariableType::UNICODE);
    
    if (expectedString != actualString) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "Type mismatch in " + context + ": cannot assign " +
              std::string(typeToString(actual)) + " to " + std::string(typeToString(expected)),
              loc);
    }
}

VariableType SemanticAnalyzer::promoteTypes(VariableType left, VariableType right) {
    // String/Unicode takes precedence
    if (left == VariableType::UNICODE || right == VariableType::UNICODE) {
        return VariableType::UNICODE;
    }
    if (left == VariableType::STRING || right == VariableType::STRING) {
        return VariableType::STRING;
    }
    
    // Numeric promotion
    if (left == VariableType::DOUBLE || right == VariableType::DOUBLE) {
        return VariableType::DOUBLE;
    }
    if (left == VariableType::FLOAT || right == VariableType::FLOAT) {
        return VariableType::FLOAT;
    }
    if (left == VariableType::INT || right == VariableType::INT) {
        return VariableType::INT;
    }
    
    return VariableType::FLOAT;
}

bool SemanticAnalyzer::isNumericType(VariableType type) {
    return type == VariableType::INT || 
           type == VariableType::FLOAT || 
           type == VariableType::DOUBLE;
}

// =============================================================================
// Symbol Table Management
// =============================================================================

VariableSymbol* SemanticAnalyzer::declareVariable(const std::string& name, VariableType type,
                                                  const SourceLocation& loc, bool isDeclared) {
    auto it = m_symbolTable.variables.find(name);
    if (it != m_symbolTable.variables.end()) {
        return &it->second;
    }
    
    VariableSymbol sym;
    sym.name = name;
    sym.type = type;
    sym.isDeclared = isDeclared;
    sym.isUsed = false;
    sym.firstUse = loc;
    
    m_symbolTable.variables[name] = sym;
    return &m_symbolTable.variables[name];
}

VariableSymbol* SemanticAnalyzer::lookupVariable(const std::string& name) {
    auto it = m_symbolTable.variables.find(name);
    if (it != m_symbolTable.variables.end()) {
        return &it->second;
    }
    
    // Also check arrays table - DIM x$ AS STRING creates a 0-dimensional array (scalar)
    // We need to treat it as a variable for assignment purposes
    auto arrIt = m_symbolTable.arrays.find(name);
    if (arrIt != m_symbolTable.arrays.end() && arrIt->second.dimensions.empty()) {
        // Found a scalar array - create a corresponding variable entry
        VariableSymbol sym;
        sym.name = name;
        sym.type = arrIt->second.type;
        sym.isDeclared = true;
        sym.firstUse = arrIt->second.declaration;
        m_symbolTable.variables[name] = sym;
        return &m_symbolTable.variables[name];
    }
    
    return nullptr;
}

ArraySymbol* SemanticAnalyzer::lookupArray(const std::string& name) {
    auto it = m_symbolTable.arrays.find(name);
    if (it != m_symbolTable.arrays.end()) {
        return &it->second;
    }
    return nullptr;
}

FunctionSymbol* SemanticAnalyzer::lookupFunction(const std::string& name) {
    auto it = m_symbolTable.functions.find(name);
    if (it != m_symbolTable.functions.end()) {
        return &it->second;
    }
    return nullptr;
}

LineNumberSymbol* SemanticAnalyzer::lookupLine(int lineNumber) {
    auto it = m_symbolTable.lineNumbers.find(lineNumber);
    if (it != m_symbolTable.lineNumbers.end()) {
        return &it->second;
    }
    return nullptr;
}

LabelSymbol* SemanticAnalyzer::declareLabel(const std::string& name, size_t programLineIndex,
                                            const SourceLocation& loc) {
    // Check for duplicate labels
    if (m_symbolTable.labels.find(name) != m_symbolTable.labels.end()) {
        error(SemanticErrorType::DUPLICATE_LABEL,
              "Label :" + name + " already defined",
              loc);
        return nullptr;
    }
    
    LabelSymbol sym;
    sym.name = name;
    sym.labelId = m_symbolTable.nextLabelId++;
    sym.programLineIndex = programLineIndex;
    sym.definition = loc;
    m_symbolTable.labels[name] = sym;
    
    return &m_symbolTable.labels[name];
}

LabelSymbol* SemanticAnalyzer::lookupLabel(const std::string& name) {
    auto it = m_symbolTable.labels.find(name);
    if (it != m_symbolTable.labels.end()) {
        return &it->second;
    }
    return nullptr;
}

TypeSymbol* SemanticAnalyzer::lookupType(const std::string& name) {
    auto it = m_symbolTable.types.find(name);
    if (it != m_symbolTable.types.end()) {
        return &it->second;
    }
    return nullptr;
}

TypeSymbol* SemanticAnalyzer::declareType(const std::string& name, const SourceLocation& loc) {
    TypeSymbol typeSymbol(name);
    typeSymbol.declaration = loc;
    m_symbolTable.types[name] = typeSymbol;
    return &m_symbolTable.types[name];
}

int SemanticAnalyzer::resolveLabelToId(const std::string& name, const SourceLocation& loc) {
    auto* sym = lookupLabel(name);
    if (!sym) {
        error(SemanticErrorType::UNDEFINED_LABEL,
              "Undefined label: " + name,
              loc);
        return -1;  // Return invalid ID on error
    }
    
    // Track this reference
    sym->references.push_back(loc);
    return sym->labelId;
}

void SemanticAnalyzer::useVariable(const std::string& name, const SourceLocation& loc) {
    auto* sym = lookupVariable(name);
    if (!sym) {
        // Implicitly declare
        VariableType type = inferTypeFromName(name);
        sym = declareVariable(name, type, loc, false);
    }
    sym->isUsed = true;
}

void SemanticAnalyzer::useArray(const std::string& name, size_t dimensionCount, 
                                const SourceLocation& loc) {
    // Check if this is actually a function/sub call, not an array access
    if (m_symbolTable.functions.find(name) != m_symbolTable.functions.end()) {
        // It's a function or sub, not an array - skip array validation
        return;
    }
    
    auto* sym = lookupArray(name);
    if (!sym) {
        if (m_requireExplicitDim) {
            error(SemanticErrorType::ARRAY_NOT_DECLARED,
                  "Array '" + name + "' used without DIM declaration",
                  loc);
        }
        return;
    }
    
    // Check dimension count
    if (dimensionCount != sym->dimensions.size()) {
        error(SemanticErrorType::WRONG_DIMENSION_COUNT,
              "Array '" + name + "' expects " + std::to_string(sym->dimensions.size()) +
              " dimensions, got " + std::to_string(dimensionCount),
              loc);
    }
}

// =============================================================================
// Type Inference from Name/Suffix
// =============================================================================

VariableType SemanticAnalyzer::inferTypeFromSuffix(TokenType suffix) {
    switch (suffix) {
        case TokenType::TYPE_INT:    return VariableType::INT;
        case TokenType::TYPE_FLOAT:  return VariableType::FLOAT;
        case TokenType::TYPE_DOUBLE: return VariableType::DOUBLE;
        case TokenType::TYPE_STRING: 
            // Return UNICODE type if in Unicode mode
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        default:                     return VariableType::UNKNOWN;
    }
}

VariableType SemanticAnalyzer::inferTypeFromName(const std::string& name) {
    if (name.empty()) return VariableType::FLOAT;
    
    // Check for normalized suffixes first (e.g., A_STRING, B_INT, C_DOUBLE)
    if (name.length() > 7 && name.substr(name.length() - 7) == "_STRING") {
        return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
    }
    if (name.length() > 4 && name.substr(name.length() - 4) == "_INT") {
        return VariableType::INT;
    }
    if (name.length() > 7 && name.substr(name.length() - 7) == "_DOUBLE") {
        return VariableType::DOUBLE;
    }
    
    // Check for original BASIC suffixes ($, %, !, #)
    char lastChar = name.back();
    switch (lastChar) {
        case '$': 
            // Return UNICODE type if in Unicode mode
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        case '%': return VariableType::INT;
        case '!': return VariableType::FLOAT;
        case '#': return VariableType::DOUBLE;
        default:  return VariableType::FLOAT;  // Default in BASIC
    }
}

// =============================================================================
// Control Flow and Final Validation
// =============================================================================

void SemanticAnalyzer::validateControlFlow(Program& program) {
    // Check for unclosed loops
    if (!m_forStack.empty()) {
        const auto& ctx = m_forStack.top();
        error(SemanticErrorType::FOR_WITHOUT_NEXT,
              "FOR loop starting at " + ctx.location.toString() + " has no matching NEXT",
              ctx.location);
    }
    
    if (!m_whileStack.empty()) {
        const auto& loc = m_whileStack.top();
        error(SemanticErrorType::WHILE_WITHOUT_WEND,
              "WHILE loop starting at " + loc.toString() + " has no matching WEND",
              loc);
    }
    
    if (!m_repeatStack.empty()) {
        const auto& loc = m_repeatStack.top();
        error(SemanticErrorType::REPEAT_WITHOUT_UNTIL,
              "REPEAT loop starting at " + loc.toString() + " has no matching UNTIL",
              loc);
    }
}

void SemanticAnalyzer::checkUnusedVariables() {
    for (const auto& pair : m_symbolTable.variables) {
        const auto& sym = pair.second;
        if (!sym.isUsed && sym.isDeclared) {
            warning("Variable '" + sym.name + "' declared but never used", sym.firstUse);
        }
    }
}

// =============================================================================
// Error Reporting
// =============================================================================

void SemanticAnalyzer::error(SemanticErrorType type, const std::string& message,
                             const SourceLocation& loc) {
    m_errors.emplace_back(type, message, loc);
}

void SemanticAnalyzer::warning(const std::string& message, const SourceLocation& loc) {
    m_warnings.emplace_back(message, loc);
}

// =============================================================================
// Report Generation
// =============================================================================

std::string SemanticAnalyzer::generateReport() const {
    std::ostringstream oss;
    
    oss << "=== SEMANTIC ANALYSIS REPORT ===\n\n";
    
    // Summary
    oss << "Status: ";
    if (m_errors.empty()) {
        oss << "✓ PASSED\n";
    } else {
        oss << "✗ FAILED (" << m_errors.size() << " error(s))\n";
    }
    
    oss << "Errors: " << m_errors.size() << "\n";
    oss << "Warnings: " << m_warnings.size() << "\n";
    oss << "\n";
    
    // Symbol table summary
    oss << "Symbol Table Summary:\n";
    oss << "  Line Numbers: " << m_symbolTable.lineNumbers.size() << "\n";
    oss << "  Variables: " << m_symbolTable.variables.size() << "\n";
    oss << "  Arrays: " << m_symbolTable.arrays.size() << "\n";
    oss << "  Functions: " << m_symbolTable.functions.size() << "\n";
    oss << "  Data Values: " << m_symbolTable.dataSegment.values.size() << "\n";
    oss << "\n";
    
    // Errors
    if (!m_errors.empty()) {
        oss << "Errors:\n";
        for (const auto& err : m_errors) {
            oss << "  " << err.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Warnings
    if (!m_warnings.empty()) {
        oss << "Warnings:\n";
        for (const auto& warn : m_warnings) {
            oss << "  " << warn.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Full symbol table
    oss << m_symbolTable.toString();
    
    oss << "=== END SEMANTIC ANALYSIS REPORT ===\n";
    
    return oss.str();
}

// =============================================================================
// Built-in Function Support
// =============================================================================

void SemanticAnalyzer::initializeBuiltinFunctions() {
    // Math functions (all take 1 argument, return FLOAT)
    m_builtinFunctions["ABS"] = 1;
    m_builtinFunctions["SIN"] = 1;
    m_builtinFunctions["COS"] = 1;
    m_builtinFunctions["TAN"] = 1;
    m_builtinFunctions["ATN"] = 1;
    m_builtinFunctions["SQR"] = 1;
    m_builtinFunctions["INT"] = 1;
    m_builtinFunctions["SGN"] = 1;
    m_builtinFunctions["LOG"] = 1;
    m_builtinFunctions["EXP"] = 1;
    
    // RND takes 0 or 1 argument
    m_builtinFunctions["RND"] = -1;  // -1 = variable arg count
    
    // GETTICKS takes 0 arguments
    m_builtinFunctions["GETTICKS"] = 0;
    
    // String functions (register both $ and _STRING variants for parser compatibility)
    m_builtinFunctions["LEN"] = 1;    // Returns INT
    m_builtinFunctions["ASC"] = 1;    // Returns INT
    m_builtinFunctions["CHR$"] = 1;   // Returns STRING
    m_builtinFunctions["CHR_STRING"] = 1;   // Parser converts CHR$ to CHR_STRING
    m_builtinFunctions["STR$"] = 1;   // Returns STRING
    m_builtinFunctions["STR_STRING"] = 1;   // Parser converts STR$ to STR_STRING
    m_builtinFunctions["VAL"] = 1;    // Returns FLOAT
    m_builtinFunctions["LEFT$"] = 2;  // Returns STRING
    m_builtinFunctions["LEFT_STRING"] = 2;  // Parser converts LEFT$ to LEFT_STRING
    m_builtinFunctions["RIGHT$"] = 2; // Returns STRING
    m_builtinFunctions["RIGHT_STRING"] = 2; // Parser converts RIGHT$ to RIGHT_STRING
    m_builtinFunctions["MID$"] = 3;   // Returns STRING (string, start, length)
    m_builtinFunctions["MID_STRING"] = 3;   // Parser converts MID$ to MID_STRING
    m_builtinFunctions["INSTR"] = -1;  // Returns INT - 2 args: (haystack$, needle$) or 3 args: (start, haystack$, needle$)
    m_builtinFunctions["STRING$"] = 2; // Returns STRING (count, char$ or ascii) - repeat character
    m_builtinFunctions["STRING_STRING"] = 2; // Parser converts STRING$ to STRING_STRING
    m_builtinFunctions["SPACE$"] = 1; // Returns STRING (count) - generate spaces
    m_builtinFunctions["SPACE_STRING"] = 1; // Parser converts SPACE$ to SPACE_STRING
    m_builtinFunctions["LCASE$"] = 1; // Returns STRING (lowercase)
    m_builtinFunctions["LCASE_STRING"] = 1; // Parser converts LCASE$ to LCASE_STRING
    m_builtinFunctions["UCASE$"] = 1; // Returns STRING (uppercase)
    m_builtinFunctions["UCASE_STRING"] = 1; // Parser converts UCASE$ to UCASE_STRING
    m_builtinFunctions["LTRIM$"] = 1; // Returns STRING (remove leading spaces)
    m_builtinFunctions["LTRIM_STRING"] = 1; // Parser converts LTRIM$ to LTRIM_STRING
    m_builtinFunctions["RTRIM$"] = 1; // Returns STRING (remove trailing spaces)
    m_builtinFunctions["RTRIM_STRING"] = 1; // Parser converts RTRIM$ to RTRIM_STRING
    m_builtinFunctions["TRIM$"] = 1;  // Returns STRING (remove leading and trailing spaces)
    m_builtinFunctions["TRIM_STRING"] = 1;  // Parser converts TRIM$ to TRIM_STRING
    m_builtinFunctions["REVERSE$"] = 1; // Returns STRING (reverse string)
    m_builtinFunctions["REVERSE_STRING"] = 1; // Parser converts REVERSE$ to REVERSE_STRING
    
    // File I/O functions
    m_builtinFunctions["EOF"] = 1;    // (file_number) Returns INT (bool)
    m_builtinFunctions["LOC"] = 1;    // (file_number) Returns INT (position)
    m_builtinFunctions["LOF"] = 1;    // (file_number) Returns INT (length)
    
    // Array bounds functions
    m_builtinFunctions["LBOUND"] = -1;  // (array) or (array, dimension) Returns INT
    m_builtinFunctions["UBOUND"] = -1;  // (array) or (array, dimension) Returns INT
    
    // =============================================================================
    // SuperTerminal Runtime API
    // =============================================================================
    
    // Text Layer
    m_builtinFunctions["TEXT_CLEAR"] = 0;           // void
    m_builtinFunctions["TEXT_CLEAR_REGION"] = 4;   // (x, y, w, h) void
    m_builtinFunctions["TEXT_PUT"] = 5;            // (x, y, text$, fg, bg) void
    m_builtinFunctions["TEXT_PUTCHAR"] = 5;        // (x, y, chr, fg, bg) void
    m_builtinFunctions["TEXT_SCROLL"] = 1;         // (lines) void
    m_builtinFunctions["TEXT_SET_SIZE"] = 2;       // (width, height) void
    m_builtinFunctions["TEXT_GET_WIDTH"] = 0;      // Returns INT
    m_builtinFunctions["TEXT_GET_HEIGHT"] = 0;     // Returns INT
    
    // Chunky Graphics Layer (palette index + background color)
    m_builtinFunctions["CHUNKY_CLEAR"] = 1;        // (bg_color) void
    m_builtinFunctions["CHUNKY_PSET"] = 4;         // (x, y, color_idx, bg) void
    m_builtinFunctions["CHUNKY_LINE"] = 6;         // (x1, y1, x2, y2, color_idx, bg) void
    m_builtinFunctions["CHUNKY_RECT"] = 6;         // (x, y, w, h, color_idx, bg) void
    m_builtinFunctions["CHUNKY_FILLRECT"] = 6;     // (x, y, w, h, color_idx, bg) void
    m_builtinFunctions["CHUNKY_HLINE"] = 5;        // (x, y, length, color_idx, bg) void
    m_builtinFunctions["CHUNKY_VLINE"] = 5;        // (x, y, length, color_idx, bg) void
    m_builtinFunctions["CHUNKY_GET_WIDTH"] = 0;    // Returns INT
    m_builtinFunctions["CHUNKY_GET_HEIGHT"] = 0;   // Returns INT
    
    // Smooth Graphics Layer (STColor + thickness for outlines)
    m_builtinFunctions["GFX_CLEAR"] = 0;           // void
    m_builtinFunctions["GFX_LINE"] = 6;            // (x1, y1, x2, y2, color, thickness) void
    m_builtinFunctions["GFX_RECT"] = 5;            // (x, y, w, h, color) void
    m_builtinFunctions["GFX_RECT_OUTLINE"] = 6;    // (x, y, w, h, color, thickness) void
    m_builtinFunctions["GFX_CIRCLE"] = 4;          // (x, y, radius, color) void
    m_builtinFunctions["GFX_CIRCLE_OUTLINE"] = 5;  // (x, y, radius, color, thickness) void
    m_builtinFunctions["GFX_POINT"] = 3;           // (x, y, color) void
    
    // Color Utilities
    m_builtinFunctions["COLOR_RGB"] = 3;           // (r, g, b) Returns INT
    m_builtinFunctions["COLOR_RGBA"] = 4;          // (r, g, b, a) Returns INT
    m_builtinFunctions["COLOR_HSV"] = 3;           // (h, s, v) Returns INT
    
    // Frame Synchronization & Timing
    m_builtinFunctions["FRAME_WAIT"] = 0;          // void
    m_builtinFunctions["FRAME_COUNT"] = 0;         // Returns INT
    m_builtinFunctions["TIME"] = 0;                // Returns FLOAT
    m_builtinFunctions["DELTA_TIME"] = 0;          // Returns FLOAT
    
    // Random Utilities
    m_builtinFunctions["RANDOM"] = 0;              // Returns FLOAT
    m_builtinFunctions["RANDOM_INT"] = 2;          // (min, max) Returns INT
    m_builtinFunctions["RANDOM_SEED"] = 1;         // (seed) void
    
    // =============================================================================
    // SuperTerminal API - Phase 2: Input & Sprites
    // =============================================================================
    
    // Keyboard Input
    m_builtinFunctions["KEY_PRESSED"] = 1;         // (keycode) Returns INT (bool)
    m_builtinFunctions["KEY_JUST_PRESSED"] = 1;    // (keycode) Returns INT (bool)
    m_builtinFunctions["KEY_JUST_RELEASED"] = 1;   // (keycode) Returns INT (bool)
    m_builtinFunctions["KEY_GET_CHAR"] = 0;        // Returns INT (char code)
    m_builtinFunctions["KEY_CLEAR_BUFFER"] = 0;    // void
    
    // Mouse Input
    m_builtinFunctions["MOUSE_X"] = 0;             // Returns INT (pixel x)
    m_builtinFunctions["MOUSE_Y"] = 0;             // Returns INT (pixel y)
    m_builtinFunctions["MOUSE_GRID_X"] = 0;        // Returns INT (grid column)
    m_builtinFunctions["MOUSE_GRID_Y"] = 0;        // Returns INT (grid row)
    m_builtinFunctions["MOUSE_BUTTON"] = 1;        // (button) Returns INT (bool)
    m_builtinFunctions["MOUSE_BUTTON_PRESSED"] = 1;    // (button) Returns INT (bool)
    m_builtinFunctions["MOUSE_BUTTON_RELEASED"] = 1;   // (button) Returns INT (bool)
    m_builtinFunctions["MOUSE_WHEEL_X"] = 0;       // Returns FLOAT (wheel delta x)
    m_builtinFunctions["MOUSE_WHEEL_Y"] = 0;       // Returns FLOAT (wheel delta y)
    
    // Sprites
    m_builtinFunctions["SPRITE_LOAD"] = 1;         // (filename$) Returns INT (sprite ID)
    m_builtinFunctions["SPRITE_LOAD_BUILTIN"] = 1; // (name$) Returns INT (sprite ID)
    m_builtinFunctions["DRAWINTOSPRITE"] = 2;      // (width, height) Returns INT (sprite ID)
    m_builtinFunctions["ENDDRAWINTOSPRITE"] = 0;   // void
    m_builtinFunctions["DRAWTOFILE"] = 3;          // (filename$, width, height) Returns BOOL
    m_builtinFunctions["ENDDRAWTOFILE"] = 0;       // Returns BOOL
    m_builtinFunctions["DRAWTOTILESET"] = 4;       // (tile_width, tile_height, columns, rows) Returns INT
    m_builtinFunctions["DRAWTILE"] = 1;            // (tile_index) Returns BOOL
    m_builtinFunctions["ENDDRAWTOTILESET"] = 0;    // Returns BOOL
    m_builtinFunctions["SPRITE_SHOW"] = 3;         // (id, x, y) void
    m_builtinFunctions["SPRITE_HIDE"] = 1;         // (id) void
    m_builtinFunctions["SPRITE_TRANSFORM"] = 6;    // (id, x, y, rot, sx, sy) void
    m_builtinFunctions["SPRITE_TINT"] = 2;         // (id, color) void
    m_builtinFunctions["SPRITE_UNLOAD"] = 1;       // (id) void
    
    // Layers
    m_builtinFunctions["LAYER_SET_VISIBLE"] = 2;   // (layer, visible) void
    m_builtinFunctions["LAYER_SET_ALPHA"] = 2;     // (layer, alpha) void
    m_builtinFunctions["LAYER_SET_ORDER"] = 2;     // (layer, order) void
    
    // Display queries
    m_builtinFunctions["DISPLAY_WIDTH"] = 0;       // Returns INT
    m_builtinFunctions["DISPLAY_HEIGHT"] = 0;      // Returns INT
    m_builtinFunctions["CELL_WIDTH"] = 0;          // Returns INT
    m_builtinFunctions["CELL_HEIGHT"] = 0;         // Returns INT
    
    // =============================================================================
    // SuperTerminal API - Phase 3: Audio
    // =============================================================================
    
    // Sound Effects
    m_builtinFunctions["SOUND_LOAD"] = 1;          // (filename$) Returns INT (sound ID)
    m_builtinFunctions["SOUND_LOAD_BUILTIN"] = 1;  // (name$) Returns INT (sound ID)
    m_builtinFunctions["SOUND_PLAY"] = 2;          // (id, volume) void
    m_builtinFunctions["SOUND_STOP"] = 1;          // (id) void
    m_builtinFunctions["SOUND_UNLOAD"] = 1;        // (id) void
    
    // Music and Audio - loaded from command registry
    
    // Synthesis
    m_builtinFunctions["SYNTH_NOTE"] = 3;          // (note, duration, volume) void
    m_builtinFunctions["SYNTH_FREQUENCY"] = 3;     // (freq, duration, volume) void
    m_builtinFunctions["SYNTH_SET_INSTRUMENT"] = 1; // (instrument) void
    
    // =============================================================================
    // SuperTerminal API - Phase 5: Asset Management
    // =============================================================================
    
    // Initialization
    m_builtinFunctions["ASSET_INIT"] = 2;          // (db_path$, max_cache_size) Returns INT (bool)
    m_builtinFunctions["ASSET_SHUTDOWN"] = 0;      // void
    m_builtinFunctions["ASSET_IS_INITIALIZED"] = 0; // Returns INT (bool)
    
    // Loading / Unloading
    m_builtinFunctions["ASSET_LOAD"] = 1;          // (name$) Returns INT (asset ID)
    m_builtinFunctions["ASSET_LOAD_FILE"] = 2;     // (path$, type) Returns INT (asset ID)
    m_builtinFunctions["ASSET_LOAD_BUILTIN"] = 2;  // (name$, type) Returns INT (asset ID)
    m_builtinFunctions["ASSET_UNLOAD"] = 1;        // (id) void
    m_builtinFunctions["ASSET_IS_LOADED"] = 1;     // (name$) Returns INT (bool)
    
    // Import / Export
    m_builtinFunctions["ASSET_IMPORT"] = 3;        // (file_path$, asset_name$, type) Returns INT (bool)
    m_builtinFunctions["ASSET_IMPORT_DIR"] = 2;    // (directory$, recursive) Returns INT (count)
    m_builtinFunctions["ASSET_EXPORT"] = 2;        // (asset_name$, file_path$) Returns INT (bool)
    m_builtinFunctions["ASSET_DELETE"] = 1;        // (asset_name$) Returns INT (bool)
    
    // Data Access
    m_builtinFunctions["ASSET_GET_SIZE"] = 1;      // (id) Returns INT
    m_builtinFunctions["ASSET_GET_TYPE"] = 1;      // (id) Returns INT
    m_builtinFunctions["ASSET_GET_NAME"] = 1;      // (id) Returns STRING
    
    // Queries
    m_builtinFunctions["ASSET_EXISTS"] = 1;        // (name$) Returns INT (bool)
    m_builtinFunctions["ASSET_GET_COUNT"] = 1;     // (type) Returns INT
    
    // Cache Management
    m_builtinFunctions["ASSET_CLEAR_CACHE"] = 0;   // void
    m_builtinFunctions["ASSET_GET_CACHE_SIZE"] = 0; // Returns INT
    m_builtinFunctions["ASSET_GET_CACHED_COUNT"] = 0; // Returns INT
    m_builtinFunctions["ASSET_SET_MAX_CACHE"] = 1; // (max_size) void
    
    // Statistics
    m_builtinFunctions["ASSET_GET_HIT_RATE"] = 0;  // Returns FLOAT
    m_builtinFunctions["ASSET_GET_DB_SIZE"] = 0;   // Returns INT
    
    // Error Handling
    m_builtinFunctions["ASSET_GET_ERROR"] = 0;     // Returns STRING
    m_builtinFunctions["ASSET_CLEAR_ERROR"] = 0;   // void
    
    // =============================================================================
    // SuperTerminal API - Phase 4: Tilemaps & Particles
    // =============================================================================
    
    // Tilemap System
    m_builtinFunctions["TILEMAP_INIT"] = 2;        // (viewport_w, viewport_h) Returns INT (bool)
    m_builtinFunctions["TILEMAP_SHUTDOWN"] = 0;    // void
    m_builtinFunctions["TILEMAP_CREATE"] = 4;      // (w, h, tile_w, tile_h) Returns INT (ID)
    m_builtinFunctions["TILEMAP_DESTROY"] = 1;     // (id) void
    m_builtinFunctions["TILEMAP_GET_WIDTH"] = 1;   // (id) Returns INT
    m_builtinFunctions["TILEMAP_GET_HEIGHT"] = 1;  // (id) Returns INT
    
    // Tileset
    m_builtinFunctions["TILESET_LOAD"] = 5;        // (path$, tw, th, margin, spacing) Returns INT (ID)
    m_builtinFunctions["TILESET_DESTROY"] = 1;     // (id) void
    m_builtinFunctions["TILESET_GET_COUNT"] = 1;   // (id) Returns INT
    
    // Layer Management
    m_builtinFunctions["TILEMAP_CREATE_LAYER"] = 1;     // (name$) Returns INT (layer ID)
    m_builtinFunctions["TILEMAP_DESTROY_LAYER"] = 1;    // (layer_id) void
    m_builtinFunctions["TILEMAP_LAYER_SET_MAP"] = 2;    // (layer_id, map_id) void
    m_builtinFunctions["TILEMAP_LAYER_SET_TILESET"] = 2; // (layer_id, tileset_id) void
    m_builtinFunctions["TILEMAP_LAYER_SET_PARALLAX"] = 3; // (layer_id, px, py) void
    m_builtinFunctions["TILEMAP_LAYER_SET_VISIBLE"] = 2;  // (layer_id, visible) void
    m_builtinFunctions["TILEMAP_LAYER_SET_Z_ORDER"] = 2;  // (layer_id, z) void
    
    // Tile Operations
    m_builtinFunctions["TILEMAP_SET_TILE"] = 4;    // (layer_id, x, y, tile_id) void
    m_builtinFunctions["TILEMAP_GET_TILE"] = 3;    // (layer_id, x, y) Returns INT
    m_builtinFunctions["TILEMAP_FILL_RECT"] = 6;   // (layer_id, x, y, w, h, tile_id) void
    m_builtinFunctions["TILEMAP_CLEAR"] = 1;       // (layer_id) void
    
    // Camera Control
    m_builtinFunctions["TILEMAP_SET_CAMERA"] = 2;  // (x, y) void
    m_builtinFunctions["TILEMAP_MOVE_CAMERA"] = 2; // (dx, dy) void
    m_builtinFunctions["TILEMAP_GET_CAMERA_X"] = 0; // Returns FLOAT
    m_builtinFunctions["TILEMAP_GET_CAMERA_Y"] = 0; // Returns FLOAT
    m_builtinFunctions["TILEMAP_SET_ZOOM"] = 1;    // (zoom) void
    m_builtinFunctions["TILEMAP_CAMERA_SHAKE"] = 2; // (magnitude, duration) void
    
    // Update
    m_builtinFunctions["TILEMAP_UPDATE"] = 1;      // (delta_time) void
    
    // Particle System
    m_builtinFunctions["PARTICLE_INIT"] = 1;       // (max_particles) Returns INT (bool)
    m_builtinFunctions["PARTICLE_SHUTDOWN"] = 0;   // void
    m_builtinFunctions["PARTICLE_IS_READY"] = 0;   // Returns INT (bool)
    m_builtinFunctions["PARTICLE_EXPLODE"] = 4;    // (x, y, count, color) Returns INT (bool)
    m_builtinFunctions["PARTICLE_EXPLODE_ADV"] = 7; // (x, y, count, color, force, gravity, fade) Returns INT
    m_builtinFunctions["PARTICLE_CLEAR"] = 0;      // void
    m_builtinFunctions["PARTICLE_PAUSE"] = 0;      // void
    m_builtinFunctions["PARTICLE_RESUME"] = 0;     // void
    m_builtinFunctions["PARTICLE_GET_COUNT"] = 0;  // Returns INT
}

bool SemanticAnalyzer::isBuiltinFunction(const std::string& name) const {
    return m_builtinFunctions.find(name) != m_builtinFunctions.end();
}

VariableType SemanticAnalyzer::getBuiltinReturnType(const std::string& name) const {
    if (!isBuiltinFunction(name)) {
        return VariableType::UNKNOWN;
    }
    
    // String functions return STRING
    if (name.back() == '$') {
        // Return UNICODE type if in Unicode mode
        return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
    }
    
    // LEN and ASC return INT
    if (name == "LEN" || name == "ASC") {
        return VariableType::INT;
    }
    
    // SuperTerminal API functions that return INT
    if (name == "TEXT_GET_WIDTH" || name == "TEXT_GET_HEIGHT" ||
        name == "CHUNKY_GET_WIDTH" || name == "CHUNKY_GET_HEIGHT" ||
        name == "COLOR_RGB" || name == "COLOR_RGBA" || name == "COLOR_HSV" ||
        name == "FRAME_COUNT" || name == "RANDOM_INT" ||
        name == "KEY_PRESSED" || name == "KEY_JUST_PRESSED" || name == "KEY_JUST_RELEASED" ||
        name == "KEY_GET_CHAR" || 
        name == "MOUSE_X" || name == "MOUSE_Y" || 
        name == "MOUSE_GRID_X" || name == "MOUSE_GRID_Y" ||
        name == "MOUSE_BUTTON" || name == "MOUSE_BUTTON_PRESSED" || name == "MOUSE_BUTTON_RELEASED" ||
        name == "SPRITE_LOAD" || name == "SPRITE_LOAD_BUILTIN" || name == "DRAWINTOSPRITE" ||
        name == "DRAWTOFILE" || name == "ENDDRAWTOFILE" ||
        name == "DRAWTOTILESET" || name == "DRAWTILE" || name == "ENDDRAWTOTILESET" ||
        name == "DISPLAY_WIDTH" || name == "DISPLAY_HEIGHT" ||
        name == "CELL_WIDTH" || name == "CELL_HEIGHT" ||
        name == "SOUND_LOAD" || name == "SOUND_LOAD_BUILTIN" ||
        name == "MUSIC_IS_PLAYING" ||
        name == "TILEMAP_INIT" || name == "TILEMAP_CREATE" ||
        name == "TILEMAP_GET_WIDTH" || name == "TILEMAP_GET_HEIGHT" ||
        name == "TILESET_LOAD" || name == "TILESET_GET_COUNT" ||
        name == "TILEMAP_CREATE_LAYER" || name == "TILEMAP_GET_TILE" ||
        name == "PARTICLE_INIT" || name == "PARTICLE_IS_READY" ||
        name == "PARTICLE_EXPLODE" || name == "PARTICLE_EXPLODE_ADV" ||
        name == "PARTICLE_GET_COUNT" ||
        name == "ASSET_INIT" || name == "ASSET_IS_INITIALIZED" ||
        name == "ASSET_LOAD" || name == "ASSET_LOAD_FILE" || name == "ASSET_LOAD_BUILTIN" ||
        name == "ASSET_IS_LOADED" || name == "ASSET_IMPORT" || name == "ASSET_IMPORT_DIR" ||
        name == "ASSET_EXPORT" || name == "ASSET_DELETE" ||
        name == "ASSET_GET_SIZE" || name == "ASSET_GET_TYPE" ||
        name == "ASSET_EXISTS" || name == "ASSET_GET_COUNT" ||
        name == "ASSET_GET_CACHE_SIZE" || name == "ASSET_GET_CACHED_COUNT" ||
        name == "ASSET_GET_DB_SIZE") {
        return VariableType::INT;
    }
    
    // SuperTerminal API functions that return FLOAT
    if (name == "TIME" || name == "DELTA_TIME" || name == "RANDOM" ||
        name == "MOUSE_WHEEL_X" || name == "MOUSE_WHEEL_Y" ||
        name == "TILEMAP_GET_CAMERA_X" || name == "TILEMAP_GET_CAMERA_Y" ||
        name == "ASSET_GET_HIT_RATE") {
        return VariableType::FLOAT;
    }
    
    // SuperTerminal API void functions (no return type)
    if (name.find("TEXT_") == 0 || name.find("CHUNKY_") == 0 || 
        name.find("GFX_") == 0 || name.find("SPRITE_") == 0 ||
        name.find("LAYER_") == 0 || name.find("SOUND_") == 0 ||
        name.find("MUSIC_") == 0 || name.find("SYNTH_") == 0 ||
        name.find("TILEMAP_") == 0 || name.find("TILESET_") == 0 ||
        name.find("PARTICLE_") == 0 || name.find("ASSET_") == 0 ||
        name == "FRAME_WAIT" || name == "RANDOM_SEED" || 
        name == "KEY_CLEAR_BUFFER") {
        // These are void functions, but we need to return something
        // We'll return INT as a placeholder (value will be ignored)
        return VariableType::INT;
    }
    
    // Asset functions that return STRING
    if (name == "ASSET_GET_NAME" || name == "ASSET_GET_ERROR") {
        // These always return byte strings, not Unicode
        return VariableType::STRING;
    }
    
    // All other functions return FLOAT
    return VariableType::FLOAT;
}

int SemanticAnalyzer::getBuiltinArgCount(const std::string& name) const {
    auto it = m_builtinFunctions.find(name);
    if (it != m_builtinFunctions.end()) {
        return it->second;
    }
    return 0;
}

void SemanticAnalyzer::loadFromCommandRegistry(const ModularCommands::CommandRegistry& registry) {
    // Get all commands and functions from the registry
    const auto& commands = registry.getAllCommands();
    
    for (const auto& pair : commands) {
        const std::string& name = pair.first;
        const ModularCommands::CommandDefinition& def = pair.second;
        
        // Add to builtin functions map with parameter count
        // Use required parameter count (commands may have optional parameters)
        int paramCount = static_cast<int>(def.getRequiredParameterCount());
        
        // Only add if not already present (don't override hardcoded core functions)
        if (m_builtinFunctions.find(name) == m_builtinFunctions.end()) {
            m_builtinFunctions[name] = paramCount;
        }
    }
}

// =============================================================================
// Constant Expression Evaluation (Compile-Time)
// =============================================================================

FasterBASIC::ConstantValue SemanticAnalyzer::evaluateConstantExpression(const Expression& expr) {
    switch (expr.getType()) {
        case ASTNodeType::EXPR_NUMBER: {
            const auto& number = static_cast<const NumberExpression&>(expr);
            double val = number.value;
            // Check if it's an integer
            if (val == std::floor(val) && val >= INT64_MIN && val <= INT64_MAX) {
                return static_cast<int64_t>(val);
            }
            return val;
        }
        
        case ASTNodeType::EXPR_STRING: {
            const auto& str = static_cast<const StringExpression&>(expr);
            return str.value;
        }
        
        case ASTNodeType::EXPR_BINARY:
            return evalConstantBinary(static_cast<const BinaryExpression&>(expr));
        
        case ASTNodeType::EXPR_UNARY:
            return evalConstantUnary(static_cast<const UnaryExpression&>(expr));
        
        case ASTNodeType::EXPR_FUNCTION_CALL:
            return evalConstantFunction(static_cast<const FunctionCallExpression&>(expr));
        
        case ASTNodeType::EXPR_VARIABLE:
            return evalConstantVariable(static_cast<const VariableExpression&>(expr));
        
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Expression type not supported in constant evaluation",
                  expr.location);
            return static_cast<int64_t>(0);
    }
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantBinary(const BinaryExpression& expr) {
    FasterBASIC::ConstantValue left = evaluateConstantExpression(*expr.left);
    FasterBASIC::ConstantValue right = evaluateConstantExpression(*expr.right);
    
    // String concatenation
    if (expr.op == TokenType::PLUS && 
        (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right))) {
        std::string leftStr = std::holds_alternative<std::string>(left) ? 
            std::get<std::string>(left) : std::to_string(getConstantAsDouble(left));
        std::string rightStr = std::holds_alternative<std::string>(right) ? 
            std::get<std::string>(right) : std::to_string(getConstantAsDouble(right));
        return leftStr + rightStr;
    }
    
    // Numeric operations
    if (!isConstantNumeric(left) || !isConstantNumeric(right)) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "Constant expression requires numeric operands",
              expr.location);
        return static_cast<int64_t>(0);
    }
    
    bool isInteger = (std::holds_alternative<int64_t>(left) && 
                      std::holds_alternative<int64_t>(right));
    
    switch (expr.op) {
        case TokenType::PLUS:
            if (isInteger) {
                return std::get<int64_t>(left) + std::get<int64_t>(right);
            }
            return getConstantAsDouble(left) + getConstantAsDouble(right);
        
        case TokenType::MINUS:
            if (isInteger) {
                return std::get<int64_t>(left) - std::get<int64_t>(right);
            }
            return getConstantAsDouble(left) - getConstantAsDouble(right);
        
        case TokenType::MULTIPLY:
            if (isInteger) {
                return std::get<int64_t>(left) * std::get<int64_t>(right);
            }
            return getConstantAsDouble(left) * getConstantAsDouble(right);
        
        case TokenType::DIVIDE:
            return getConstantAsDouble(left) / getConstantAsDouble(right);
        
        case TokenType::POWER:
            return std::pow(getConstantAsDouble(left), getConstantAsDouble(right));
        
        case TokenType::MOD:
            if (isInteger) {
                return std::get<int64_t>(left) % std::get<int64_t>(right);
            }
            return std::fmod(getConstantAsDouble(left), getConstantAsDouble(right));
        
        case TokenType::INT_DIVIDE: // Integer division
            return getConstantAsInt(left) / getConstantAsInt(right);
        
        case TokenType::AND:
            return getConstantAsInt(left) & getConstantAsInt(right);
        
        case TokenType::OR:
            return getConstantAsInt(left) | getConstantAsInt(right);
        
        case TokenType::XOR:
            return getConstantAsInt(left) ^ getConstantAsInt(right);
        
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Operator not supported in constant expressions",
                  expr.location);
            return static_cast<int64_t>(0);
    }
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantUnary(const UnaryExpression& expr) {
    FasterBASIC::ConstantValue operand = evaluateConstantExpression(*expr.expr);
    
    switch (expr.op) {
        case TokenType::MINUS:
            if (std::holds_alternative<int64_t>(operand)) {
                return -std::get<int64_t>(operand);
            }
            return -std::get<double>(operand);
        
        case TokenType::PLUS:
            return operand;
        
        case TokenType::NOT:
            return ~getConstantAsInt(operand);
        
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Unary operator not supported in constant expressions",
                  expr.location);
            return static_cast<int64_t>(0);
    }
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantFunction(const FunctionCallExpression& expr) {
    std::string funcName = expr.name;
    
    // Convert to uppercase for comparison
    for (auto& c : funcName) c = std::toupper(c);
    
    // Math functions (single argument)
    if (funcName == "ABS" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (std::holds_alternative<int64_t>(arg)) {
            return std::abs(std::get<int64_t>(arg));
        }
        return std::abs(std::get<double>(arg));
    }
    
    if (funcName == "SIN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::sin(getConstantAsDouble(arg));
    }
    
    if (funcName == "COS" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::cos(getConstantAsDouble(arg));
    }
    
    if (funcName == "TAN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::tan(getConstantAsDouble(arg));
    }
    
    if (funcName == "ATN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::atan(getConstantAsDouble(arg));
    }
    
    if (funcName == "EXP" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::exp(getConstantAsDouble(arg));
    }
    
    if (funcName == "LOG" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::log(getConstantAsDouble(arg));
    }
    
    if (funcName == "SQR" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::sqrt(getConstantAsDouble(arg));
    }
    
    if (funcName == "INT" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return static_cast<int64_t>(std::floor(getConstantAsDouble(arg)));
    }
    
    if (funcName == "SGN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        double val = getConstantAsDouble(arg);
        return static_cast<int64_t>(val > 0 ? 1 : (val < 0 ? -1 : 0));
    }
    
    // String functions
    if (funcName == "LEN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (!std::holds_alternative<std::string>(arg)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "LEN requires string argument",
                  expr.location);
            return static_cast<int64_t>(0);
        }
        return static_cast<int64_t>(std::get<std::string>(arg).length());
    }
    
    if ((funcName == "LEFT$" || funcName == "LEFT") && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue str = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue len = evaluateConstantExpression(*expr.arguments[1]);
        if (!std::holds_alternative<std::string>(str)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "LEFT$ requires string argument",
                  expr.location);
            return std::string("");
        }
        int64_t n = getConstantAsInt(len);
        return std::get<std::string>(str).substr(0, std::max(int64_t(0), n));
    }
    
    if ((funcName == "RIGHT$" || funcName == "RIGHT") && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue str = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue len = evaluateConstantExpression(*expr.arguments[1]);
        if (!std::holds_alternative<std::string>(str)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "RIGHT$ requires string argument",
                  expr.location);
            return std::string("");
        }
        int64_t n = getConstantAsInt(len);
        std::string strVal = std::get<std::string>(str);
        size_t strLen = strVal.length();
        if (n >= static_cast<int64_t>(strLen)) {
            return str;
        }
        return strVal.substr(strLen - n);
    }
    
    if ((funcName == "MID$" || funcName == "MID") && 
        (expr.arguments.size() == 2 || expr.arguments.size() == 3)) {
        FasterBASIC::ConstantValue str = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue start = evaluateConstantExpression(*expr.arguments[1]);
        if (!std::holds_alternative<std::string>(str)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "MID$ requires string argument",
                  expr.location);
            return std::string("");
        }
        int64_t startPos = getConstantAsInt(start) - 1; // BASIC is 1-indexed
        if (startPos < 0) startPos = 0;
        
        std::string strVal = std::get<std::string>(str);
        if (expr.arguments.size() == 3) {
            FasterBASIC::ConstantValue len = evaluateConstantExpression(*expr.arguments[2]);
            int64_t length = getConstantAsInt(len);
            return strVal.substr(startPos, length);
        } else {
            return strVal.substr(startPos);
        }
    }
    
    if ((funcName == "CHR$" || funcName == "CHR") && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        int64_t code = getConstantAsInt(arg);
        if (code < 0 || code > 255) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "CHR$ argument must be 0-255",
                  expr.location);
            return std::string("");
        }
        return std::string(1, static_cast<char>(code));
    }
    
    if (funcName == "STR$" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (std::holds_alternative<int64_t>(arg)) {
            return std::to_string(std::get<int64_t>(arg));
        } else if (std::holds_alternative<double>(arg)) {
            return std::to_string(std::get<double>(arg));
        }
        return arg; // Already a string
    }
    
    if (funcName == "VAL" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (!std::holds_alternative<std::string>(arg)) {
            return arg; // Already numeric
        }
        try {
            std::string strVal = std::get<std::string>(arg);
            // Try to parse as integer first
            size_t pos;
            int64_t intVal = std::stoll(strVal, &pos);
            if (pos == strVal.length()) {
                return intVal;
            }
            // Otherwise parse as double
            double dblVal = std::stod(strVal);
            return dblVal;
        } catch (...) {
            return 0.0;
        }
    }
    
    // Two-argument math functions
    if (funcName == "MIN" && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue arg1 = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue arg2 = evaluateConstantExpression(*expr.arguments[1]);
        double v1 = getConstantAsDouble(arg1);
        double v2 = getConstantAsDouble(arg2);
        return std::min(v1, v2);
    }
    
    if (funcName == "MAX" && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue arg1 = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue arg2 = evaluateConstantExpression(*expr.arguments[1]);
        double v1 = getConstantAsDouble(arg1);
        double v2 = getConstantAsDouble(arg2);
        return std::max(v1, v2);
    }
    
    error(SemanticErrorType::UNDEFINED_FUNCTION,
          "Function " + funcName + " not supported in constant expressions or wrong number of arguments",
          expr.location);
    return static_cast<int64_t>(0);
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantVariable(const VariableExpression& expr) {
    // Look up constant by name (case-insensitive)
    std::string lowerName = expr.name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    auto it = m_symbolTable.constants.find(lowerName);
    if (it == m_symbolTable.constants.end()) {
        error(SemanticErrorType::UNDEFINED_VARIABLE,
              "Undefined constant: " + expr.name,
              expr.location);
        return static_cast<int64_t>(0);
    }
    
    const ConstantSymbol& sym = it->second;
    if (sym.type == ConstantSymbol::Type::INTEGER) {
        return sym.intValue;
    } else if (sym.type == ConstantSymbol::Type::DOUBLE) {
        return sym.doubleValue;
    } else {
        return sym.stringValue;
    }
}

bool SemanticAnalyzer::isConstantNumeric(const FasterBASIC::ConstantValue& val) {
    return std::holds_alternative<int64_t>(val) || std::holds_alternative<double>(val);
}

double SemanticAnalyzer::getConstantAsDouble(const FasterBASIC::ConstantValue& val) {
    if (std::holds_alternative<int64_t>(val)) {
        return static_cast<double>(std::get<int64_t>(val));
    } else if (std::holds_alternative<double>(val)) {
        return std::get<double>(val);
    }
    return 0.0;
}

int64_t SemanticAnalyzer::getConstantAsInt(const FasterBASIC::ConstantValue& val) {
    if (std::holds_alternative<int64_t>(val)) {
        return std::get<int64_t>(val);
    } else if (std::holds_alternative<double>(val)) {
        return static_cast<int64_t>(std::get<double>(val));
    }
    return 0;
}

bool SemanticAnalyzer::isConstantExpression(const Expression& expr) {
    // Check if an expression can be evaluated at compile time
    switch (expr.getType()) {
        case ASTNodeType::EXPR_NUMBER:
        case ASTNodeType::EXPR_STRING:
            return true;
        
        case ASTNodeType::EXPR_VARIABLE: {
            // Check if this variable is a declared constant (case-insensitive)
            const auto& varExpr = static_cast<const VariableExpression&>(expr);
            std::string lowerName = varExpr.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            return m_symbolTable.constants.find(lowerName) != m_symbolTable.constants.end();
        }
        
        case ASTNodeType::EXPR_BINARY: {
            const auto& binExpr = static_cast<const BinaryExpression&>(expr);
            return isConstantExpression(*binExpr.left) && isConstantExpression(*binExpr.right);
        }
        
        case ASTNodeType::EXPR_UNARY: {
            const auto& unaryExpr = static_cast<const UnaryExpression&>(expr);
            return isConstantExpression(*unaryExpr.expr);
        }
        
        case ASTNodeType::EXPR_FUNCTION_CALL: {
            const auto& funcExpr = static_cast<const FunctionCallExpression&>(expr);
            // Check if all arguments are constant
            for (const auto& arg : funcExpr.arguments) {
                if (!isConstantExpression(*arg)) {
                    return false;
                }
            }
            return true;
        }
        
        default:
            return false;
    }
}

// =============================================================================
// Function Scope Variable Validation
// =============================================================================

void SemanticAnalyzer::validateVariableInFunction(const std::string& varName, 
                                                    const SourceLocation& loc) {
    if (!m_currentFunctionScope.inFunction) {
        // Not in a function - use normal variable lookup
        useVariable(varName, loc);
        return;
    }
    
    // Allow FUNCTION to assign to its own name (for return value)
    if (varName == m_currentFunctionScope.functionName) {
        return;
    }
    
    // Check if variable is declared in function scope
    if (m_currentFunctionScope.parameters.count(varName) ||
        m_currentFunctionScope.localVariables.count(varName) ||
        m_currentFunctionScope.sharedVariables.count(varName)) {
        // Variable is properly declared
        return;
    }
    
    // Variable not declared - ERROR!
    error(SemanticErrorType::UNDEFINED_VARIABLE,
          "Variable '" + varName + "' is not declared in " + 
          m_currentFunctionScope.functionName + ". " +
          "Use LOCAL or SHARED to declare it.",
          loc);
}

} // namespace FasterBASIC