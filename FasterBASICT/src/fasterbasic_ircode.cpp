//
// fasterbasic_ircode.cpp
// FasterBASIC - IR Code Generator Implementation
//
// Converts the Abstract Syntax Tree (AST) into Intermediate Representation (IR)
// bytecode. This is Phase 5 of the compilation pipeline.
//

#include "fasterbasic_ircode.h"
#include "modular_commands.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include <climits>
#include <iostream>

namespace FasterBASIC {

// =============================================================================
// Helper Functions
// =============================================================================

// Extract type suffix from variable/array name
static std::string extractTypeSuffix(const std::string& name) {
    if (name.empty()) return "";

    char lastChar = name.back();
    switch (lastChar) {
        case '%': return "%";  // Integer
        case '#': return "#";  // Double
        case '!': return "!";  // Float
        case '$': return "$";  // String
        case '&': return "&";  // Long
        default: return "";    // Default (float)
    }
}

// =============================================================================
// Constructor
// =============================================================================

IRGenerator::IRGenerator()
    : m_cfg(nullptr)
    , m_symbols(nullptr)
    , m_code(nullptr)
    , m_nextLabel(1)
    , m_traceEnabled(false)
    , m_currentLineNumber(0)
    , m_currentBlockId(-1)
    , m_inFunctionInlining(false)
{}

// =============================================================================
// Main Generation Entry Point
// =============================================================================

std::unique_ptr<IRCode> IRGenerator::generate(
    const ControlFlowGraph& cfg,
    const SymbolTable& symbols)
{
    m_cfg = &cfg;
    m_symbols = &symbols;
    m_code = std::make_unique<IRCode>();
    m_nextLabel = 1;
    m_blockLabels.clear();

    m_code->blockCount = cfg.getBlockCount();
    m_code->arrayBase = symbols.arrayBase;  // Copy OPTION BASE setting
    m_code->unicodeMode = symbols.unicodeMode;  // Copy OPTION UNICODE setting
    m_code->errorTracking = symbols.errorTracking;  // Copy OPTION ERROR setting
    m_code->forceYieldEnabled = symbols.forceYieldEnabled;  // Copy OPTION FORCE_YIELD setting
    m_code->forceYieldBudget = symbols.forceYieldBudget;  // Copy OPTION FORCE_YIELD budget
    m_code->cancellableLoops = symbols.cancellableLoops;  // Copy OPTION CANCELLABLE setting
    m_code->eventsUsed = symbols.eventsUsed;  // Copy EVENT DETECTION setting

    // Copy user-defined types from symbol table
    m_code->types = symbols.types;

    // Copy DATA segment from symbol table
    m_code->dataValues = symbols.dataSegment.values;
    
    // Copy line number restore points
    m_code->dataLineRestorePoints = symbols.dataSegment.restorePoints;
    
    // Copy label restore points
    m_code->dataLabelRestorePoints = symbols.dataSegment.labelRestorePoints;

    // Pre-populate m_functions with all function definitions from symbol table
    // This allows forward references (calling functions before they're defined in line order)
    for (const auto& [name, funcSymbol] : symbols.functions) {
        FunctionDef func;
        func.name = name;
        // Convert VariableType to TokenType for return type
        switch (funcSymbol.returnType) {
            case VariableType::INT: func.returnType = TokenType::TYPE_INT; break;
            case VariableType::FLOAT: func.returnType = TokenType::TYPE_FLOAT; break;
            case VariableType::DOUBLE: func.returnType = TokenType::TYPE_DOUBLE; break;
            case VariableType::STRING: func.returnType = TokenType::TYPE_STRING; break;
            default: func.returnType = TokenType::UNKNOWN; break;
        }
        // Note: parameters and body will be filled in when we encounter the actual FUNCTION statement
        m_functions[name] = func;
    }

    // Generate labels for all blocks first (needed for jumps)
    for (const auto& blockPtr : cfg.blocks) {
        if (blockPtr) {
            getLabelForBlock(blockPtr->id);
        }
    }

    // Generate code for each block in order
    for (const auto& blockPtr : cfg.blocks) {
        if (blockPtr) {
            generateBlock(*blockPtr);
        }
    }

    // Add final HALT instruction if not already present
    if (m_code->instructions.empty() ||
        m_code->instructions.back().opcode != IROpcode::HALT) {
        emit(IROpcode::HALT);
    }

    m_code->labelCount = m_nextLabel - 1;

    return std::move(m_code);
}

// =============================================================================
// Block Code Generation
// =============================================================================

void IRGenerator::generateBlock(const BasicBlock& block) {
    setSourceContext(block.getFirstLineNumber(), block.id);

    // Emit label for this block
    int labelId = getLabelForBlock(block.id);
    m_code->emitLabel(labelId, block.id);

    // Record line number mappings for all lines in this block
    for (int lineNum : block.lineNumbers) {
        if (lineNum > 0) {
            int addr = static_cast<int>(m_code->instructions.size());
            m_code->lineToAddress[lineNum] = addr;
        }
    }

    // Generate code for each statement in the block
    for (const Statement* stmt : block.statements) {
        if (!stmt) continue;

        // Get the specific line number for this statement
        int lineNum = block.getLineNumber(stmt);

        generateStatement(stmt, lineNum);
    }

    // Generate control flow edges
    // If block has no explicit control flow (no successors added by statements),
    // we need to add jumps based on CFG edges
    const auto& edges = m_cfg->edges;
    bool hasExplicitFlow = false;

    // Check if last statement was a control flow statement
    if (!block.statements.empty()) {
        const Statement* lastStmt = block.statements.back();
        if (dynamic_cast<const GotoStatement*>(lastStmt) ||
            dynamic_cast<const GosubStatement*>(lastStmt) ||
            dynamic_cast<const ReturnStatement*>(lastStmt) ||
            dynamic_cast<const EndStatement*>(lastStmt) ||
            dynamic_cast<const IfStatement*>(lastStmt)) {
            hasExplicitFlow = true;
        }
    }

    // Add fallthrough or jump if needed
    if (!hasExplicitFlow && !block.successors.empty()) {
        // Find fallthrough edge
        bool hasFallthrough = false;
        for (const auto& edge : edges) {
            if (edge.sourceBlock == block.id &&
                edge.type == EdgeType::FALLTHROUGH) {
                // Implicit fallthrough - just let execution continue
                hasFallthrough = true;
                break;
            }
        }

        // If no fallthrough but has successors, add explicit jump
        if (!hasFallthrough && !block.successors.empty()) {
            int targetLabel = getLabelForBlock(block.successors[0]);
            emit(IROpcode::JUMP, targetLabel);
        }
    }
}

// =============================================================================
// Statement Code Generation
// =============================================================================

void IRGenerator::generateStatement(const Statement* stmt, int lineNumber) {
    if (!stmt) return;

    setSourceContext(lineNumber, m_currentBlockId);

    if (auto* s = dynamic_cast<const PrintStatement*>(stmt)) {
        generatePrint(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ConsoleStatement*>(stmt)) {
        generateConsole(s, lineNumber);
    } else if (auto* s = dynamic_cast<const PrintAtStatement*>(stmt)) {
        generatePrintAt(s, lineNumber);
    } else if (auto* s = dynamic_cast<const PlayStatement*>(stmt)) {
        generatePlay(s, lineNumber);
    } else if (auto* s = dynamic_cast<const PlaySoundStatement*>(stmt)) {
        generatePlaySound(s, lineNumber);
    } else if (auto* s = dynamic_cast<const InputAtStatement*>(stmt)) {
        generateInputAt(s, lineNumber);
    } else if (auto* s = dynamic_cast<const LetStatement*>(stmt)) {
        generateLet(s, lineNumber);
    } else if (auto* s = dynamic_cast<const MidAssignStatement*>(stmt)) {
        generateMidAssign(s, lineNumber);
    } else if (auto* s = dynamic_cast<const IfStatement*>(stmt)) {
        generateIf(s, lineNumber);
    } else if (auto* s = dynamic_cast<const CaseStatement*>(stmt)) {
        generateCase(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ForStatement*>(stmt)) {
        generateFor(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ForInStatement*>(stmt)) {
        generateForIn(s, lineNumber);
    } else if (auto* s = dynamic_cast<const NextStatement*>(stmt)) {
        generateNext(s, lineNumber);
    } else if (auto* s = dynamic_cast<const WhileStatement*>(stmt)) {
        generateWhile(s, lineNumber);
    } else if (auto* s = dynamic_cast<const WendStatement*>(stmt)) {
        generateWend(s, lineNumber);
    } else if (auto* s = dynamic_cast<const RepeatStatement*>(stmt)) {
        generateRepeat(s, lineNumber);
    } else if (auto* s = dynamic_cast<const UntilStatement*>(stmt)) {
        generateUntil(s, lineNumber);
    } else if (auto* s = dynamic_cast<const DoStatement*>(stmt)) {
        generateDo(s, lineNumber);
    } else if (auto* s = dynamic_cast<const LoopStatement*>(stmt)) {
        generateLoop(s, lineNumber);
    } else if (auto* s = dynamic_cast<const GotoStatement*>(stmt)) {
        generateGoto(s, lineNumber);
    } else if (auto* s = dynamic_cast<const GosubStatement*>(stmt)) {
        generateGosub(s, lineNumber);
    } else if (auto* s = dynamic_cast<const OnGotoStatement*>(stmt)) {
        generateOnGoto(s, lineNumber);
    } else if (auto* s = dynamic_cast<const OnGosubStatement*>(stmt)) {
        generateOnGosub(s, lineNumber);
    } else if (auto* s = dynamic_cast<const OnCallStatement*>(stmt)) {
        generateOnCall(s, lineNumber);
    } else if (auto* s = dynamic_cast<const OnEventStatement*>(stmt)) {
        generateOnEvent(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ConstantStatement*>(stmt)) {
        generateConstant(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ReturnStatement*>(stmt)) {
        generateReturn(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ExitStatement*>(stmt)) {
        generateExit(s, lineNumber);
    } else if (auto* s = dynamic_cast<const DimStatement*>(stmt)) {
        generateDim(s, lineNumber);
    } else if (auto* s = dynamic_cast<const RedimStatement*>(stmt)) {
        generateRedim(s, lineNumber);
    } else if (auto* s = dynamic_cast<const EraseStatement*>(stmt)) {
        generateErase(s, lineNumber);
    } else if (auto* s = dynamic_cast<const SwapStatement*>(stmt)) {
        generateSwap(s, lineNumber);
    } else if (auto* s = dynamic_cast<const IncStatement*>(stmt)) {
        generateInc(s, lineNumber);
    } else if (auto* s = dynamic_cast<const DecStatement*>(stmt)) {
        generateDec(s, lineNumber);
    } else if (auto* s = dynamic_cast<const LocalStatement*>(stmt)) {
        generateLocal(s, lineNumber);
    } else if (auto* s = dynamic_cast<const SharedStatement*>(stmt)) {
        generateShared(s, lineNumber);
    } else if (auto* s = dynamic_cast<const InputStatement*>(stmt)) {
        generateInput(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ReadStatement*>(stmt)) {
        generateRead(s, lineNumber);
    } else if (auto* s = dynamic_cast<const RestoreStatement*>(stmt)) {
        generateRestore(s, lineNumber);
    } else if (auto* s = dynamic_cast<const OpenStatement*>(stmt)) {
        generateOpen(s, lineNumber);
    } else if (auto* s = dynamic_cast<const CloseStatement*>(stmt)) {
        generateClose(s, lineNumber);
    } else if (auto* s = dynamic_cast<const EndStatement*>(stmt)) {
        generateEnd(s, lineNumber);
    } else if (auto* s = dynamic_cast<const RemStatement*>(stmt)) {
        generateRem(s, lineNumber);
    } else if (auto* s = dynamic_cast<const DefStatement*>(stmt)) {
        generateDef(s, lineNumber);
    } else if (auto* s = dynamic_cast<const FunctionStatement*>(stmt)) {
        generateFunction(s, lineNumber);
    } else if (auto* s = dynamic_cast<const SubStatement*>(stmt)) {
        generateSub(s, lineNumber);
    } else if (auto* s = dynamic_cast<const CallStatement*>(stmt)) {
        generateCall(s, lineNumber);
    } else if (auto* s = dynamic_cast<const LabelStatement*>(stmt)) {
        generateLabel(s, lineNumber);
    } else if (auto* s = dynamic_cast<const ExpressionStatement*>(stmt)) {
        generateExpressionStatement(s, lineNumber);
    } else if (auto* s = dynamic_cast<const SimpleStatement*>(stmt)) {
        generateSimpleStatement(s, lineNumber);
    } else if (auto* s = dynamic_cast<const AfterStatement*>(stmt)) {
        generateAfter(s, lineNumber);
    } else if (auto* s = dynamic_cast<const EveryStatement*>(stmt)) {
        generateEvery(s, lineNumber);
    } else if (auto* s = dynamic_cast<const AfterFramesStatement*>(stmt)) {
        generateAfterFrames(s, lineNumber);
    } else if (auto* s = dynamic_cast<const EveryFrameStatement*>(stmt)) {
        generateEveryFrame(s, lineNumber);
    } else if (auto* s = dynamic_cast<const RunStatement*>(stmt)) {
        generateRun(s, lineNumber);
    } else if (auto* s = dynamic_cast<const TimerStopStatement*>(stmt)) {
        generateTimerStop(s, lineNumber);
    } else if (auto* s = dynamic_cast<const TimerIntervalStatement*>(stmt)) {
        generateTimerInterval(s, lineNumber);
    } else if (auto* s = dynamic_cast<const TypeDeclarationStatement*>(stmt)) {
        generateTypeDeclaration(s, lineNumber);
    }
    // Add more statement types as needed
}

// =============================================================================
// Specific Statement Generators
// =============================================================================

void IRGenerator::generatePrint(const PrintStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Handle file output (PRINT#)
    if (stmt->fileNumber > 0) {
        for (const auto& item : stmt->items) {
            generateExpression(item.expr.get());
            std::string separator = item.semicolon ? ";" : (item.comma ? "," : "\n");
            emit(IROpcode::PRINT_FILE, std::to_string(stmt->fileNumber), separator);
        }
        if (stmt->trailingNewline) {
            emit(IROpcode::PRINT_FILE_NEWLINE, std::to_string(stmt->fileNumber));
        }
        return;
    }

    // Handle PRINT USING
    if (stmt->hasUsing) {
        // Generate format string expression
        generateExpression(stmt->formatExpr.get());
        
        // Generate value expressions
        for (const auto& val : stmt->usingValues) {
            generateExpression(val.get());
        }
        
        // Emit PRINT_USING with value count
        emit(IROpcode::PRINT_USING, static_cast<int>(stmt->usingValues.size()));
        return;
    }

    // Handle regular PRINT
    for (size_t i = 0; i < stmt->items.size(); i++) {
        const auto& item = stmt->items[i];

        if (item.expr) {
            // Regular expression - evaluate and print
            generateExpression(item.expr.get());
            emit(IROpcode::PRINT, 0);  // 0 = no special formatting
        }

        // Handle separators (comma = tab to next zone, semicolon = no space)
        if (i < stmt->items.size() - 1) {
            if (item.comma) {
                // Comma means tab to next zone
                emit(IROpcode::PRINT_TAB, 14);  // Standard tab width
            }
        }
    }

    // Add newline unless statement ends with separator
    if (stmt->trailingNewline) {
        emit(IROpcode::PRINT_NEWLINE);
    }
}

void IRGenerator::generateConsole(const ConsoleStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Handle regular CONSOLE output (similar to PRINT but uses CONSOLE opcode)
    for (size_t i = 0; i < stmt->items.size(); i++) {
        const auto& item = stmt->items[i];

        if (item.expr) {
            // Regular expression - evaluate and print to console
            generateExpression(item.expr.get());
            emit(IROpcode::CONSOLE, 0);  // 0 = no special formatting
        }

        // Handle separators (comma = tab to next zone, semicolon = no space)
        if (i < stmt->items.size() - 1) {
            if (item.comma) {
                // Comma means tab to next zone (for console output)
                emit(IROpcode::PRINT_TAB, 14);  // Standard tab width
            }
        }
    }

    // Add newline unless statement ends with separator
    if (stmt->trailingNewline) {
        emit(IROpcode::PRINT_NEWLINE);
    }
}

void IRGenerator::generatePlay(const PlayStatement* stmt, int lineNumber) {
    // Generate code for PLAY command
    // PLAY "filename" [AS format] [INTO_WAV "output"] [INTO_SLOT n]
    
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Check if INTO_SLOT is specified - render to sound slot
    if (stmt->hasSlot) {
        // Generate the input filename expression
        generateExpression(stmt->filename.get());
        
        // Generate the slot number expression
        generateExpression(stmt->slotNumber.get());
        
        // Generate format (or null for auto-detect)
        if (stmt->hasFormat) {
            auto formatExpr = std::make_unique<StringExpression>(stmt->format);
            generateExpression(formatExpr.get());
        } else {
            // Push null/empty string for auto-detect
            auto nullExpr = std::make_unique<StringExpression>("");
            generateExpression(nullExpr.get());
        }
        
        // Push fastRender flag as boolean
        auto fastExpr = std::make_unique<NumberExpression>(stmt->fastRender ? 1.0 : 0.0);
        generateExpression(fastExpr.get());
        
        // Call st_music_render_to_slot(inputPath, slotNumber, format, fastRender) - 4 arguments
        emit(IROpcode::CALL_BUILTIN, "st_music_render_to_slot", 4);
    }
    // Check if INTO_WAV is specified - use render function instead of play
    else if (stmt->hasWavOutput) {
        // Generate the input filename expression
        generateExpression(stmt->filename.get());
        
        // Generate the output WAV filename expression
        generateExpression(stmt->wavOutput.get());
        
        // Generate format (or null for auto-detect)
        if (stmt->hasFormat) {
            auto formatExpr = std::make_unique<StringExpression>(stmt->format);
            generateExpression(formatExpr.get());
        } else {
            // Push null/empty string for auto-detect
            auto nullExpr = std::make_unique<StringExpression>("");
            generateExpression(nullExpr.get());
        }
        
        // Push fastRender flag as boolean
        auto fastExpr = std::make_unique<NumberExpression>(stmt->fastRender ? 1.0 : 0.0);
        generateExpression(fastExpr.get());
        
        // Call st_music_render_to_wav(inputPath, outputPath, format, fastRender) - 4 arguments
        emit(IROpcode::CALL_BUILTIN, "st_music_render_to_wav", 4);
    } else {
        // Normal playback mode
        // Generate the filename expression
        generateExpression(stmt->filename.get());
        
        // If format override is specified, generate it as well
        if (stmt->hasFormat) {
            // Push format string as a string constant expression
            auto formatExpr = std::make_unique<StringExpression>(stmt->format);
            generateExpression(formatExpr.get());
            
            // Call st_music_play_file_with_format(filename, format) - 2 arguments
            emit(IROpcode::CALL_BUILTIN, "st_music_play_file_with_format", 2);
        } else {
            // Call st_music_play_file(filename) - 1 argument
            emit(IROpcode::CALL_BUILTIN, "st_music_play_file", 1);
        }
    }
}

void IRGenerator::generatePlaySound(const PlaySoundStatement* stmt, int lineNumber) {
    // Generate code for PLAY_SOUND command
    // PLAY_SOUND id, volume [, cap_duration]
    
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Generate sound ID expression
    generateExpression(stmt->soundId.get());
    
    // Generate volume expression
    generateExpression(stmt->volume.get());
    
    // Generate cap duration if specified, otherwise push -1.0 (no cap)
    if (stmt->hasCapDuration) {
        generateExpression(stmt->capDuration.get());
    } else {
        auto noCap = std::make_unique<NumberExpression>(-1.0);
        generateExpression(noCap.get());
    }
    
    // Call st_sound_play_with_fade(id, volume, cap_duration) - 3 arguments
    emit(IROpcode::CALL_BUILTIN, "st_sound_play_with_fade", 3);
}

void IRGenerator::generatePrintAt(const PrintAtStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate x and y coordinates
    generateExpression(stmt->x.get());
    generateExpression(stmt->y.get());

    // Handle PRINT_AT USING
    if (stmt->hasUsing) {
        // Generate format string expression
        generateExpression(stmt->formatExpr.get());
        
        // Generate value expressions
        for (const auto& val : stmt->usingValues) {
            generateExpression(val.get());
        }
        
        // Generate colors (with defaults if not specified)
        if (stmt->hasExplicitColors && stmt->fg) {
            generateExpression(stmt->fg.get());
        } else {
            // Default solid_white: 0xFFFFFFFF = 4294967295
            auto defaultFg = std::make_unique<NumberExpression>(4294967295.0);
            generateExpression(defaultFg.get());
        }
        
        if (stmt->hasExplicitColors && stmt->bg) {
            generateExpression(stmt->bg.get());
        } else {
            // Default clear_black: 0x00000000 = 0
            auto defaultBg = std::make_unique<NumberExpression>(0.0);
            generateExpression(defaultBg.get());
        }
        
        // Emit PRINT_AT_USING with value count
        emit(IROpcode::PRINT_AT_USING, static_cast<int>(stmt->usingValues.size()));
        return;
    }

    // Handle regular PRINT_AT with text items
    // Generate all text expressions
    for (const auto& item : stmt->items) {
        if (item.expr) {
            generateExpression(item.expr.get());
        }
    }

    // Generate colors (with defaults if not specified)
    if (stmt->hasExplicitColors && stmt->fg) {
        generateExpression(stmt->fg.get());
    } else {
        // Default solid_white: 0xFFFFFFFF = 4294967295
        auto defaultFg = std::make_unique<NumberExpression>(4294967295.0);
        generateExpression(defaultFg.get());
    }
    
    if (stmt->hasExplicitColors && stmt->bg) {
        generateExpression(stmt->bg.get());
    } else {
        // Default clear_black: 0x00000000 = 0
        auto defaultBg = std::make_unique<NumberExpression>(0.0);
        generateExpression(defaultBg.get());
    }

    // Emit PRINT_AT with item count
    emit(IROpcode::PRINT_AT, static_cast<int>(stmt->items.size()));
}

void IRGenerator::generateInputAt(const InputAtStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate x coordinate expression
    generateExpression(stmt->x.get());
    
    // Generate y coordinate expression
    generateExpression(stmt->y.get());

    // Emit INPUT_AT with prompt and variable name
    emit(IROpcode::INPUT_AT, stmt->prompt, stmt->variable);
}

void IRGenerator::generateLet(const LetStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // =========================================================================
    // WHOLE-ARRAY OPERATION DETECTION
    // =========================================================================
    // Check if this is a whole-array assignment: A() = ...
    // Criteria: indices.empty() AND variable is an array AND no member access
    if (stmt->indices.empty() && stmt->memberChain.empty()) {
        // Check if the variable is actually an array in the symbol table
        auto arrayIt = m_symbols->arrays.find(stmt->variable);
        if (arrayIt != m_symbols->arrays.end()) {
            const ArraySymbol& lhsArray = arrayIt->second;
            
            // Check if this array has a user-defined type that is SIMD-capable
            if (!lhsArray.asTypeName.empty()) {
                auto typeIt = m_symbols->types.find(lhsArray.asTypeName);
                if (typeIt != m_symbols->types.end()) {
                    const TypeSymbol& arrayType = typeIt->second;
                    
                    // Check if this type is SIMD-capable (PAIR or QUAD)
                    if (arrayType.simdType != TypeDeclarationStatement::SIMDType::NONE) {
                        // This is a SIMD-capable whole-array assignment!
                        // Try to detect and emit SIMD IR opcodes
                        if (tryEmitSIMDArrayOperation(stmt, lhsArray, arrayType)) {
                            // SIMD operation successfully emitted, we're done
                            return;
                        }
                        // If tryEmitSIMDArrayOperation returns false, fall through to standard codegen
                    }
                }
            }
            
            // For regular (non-SIMD) arrays, check if this is an array operation
            // Try to detect and emit array operation IR opcodes
            if (tryEmitArrayOperation(stmt, lhsArray)) {
                // Array operation successfully emitted, we're done
                return;
            }
            
            // Fall through: handle whole-array scalar assignment: A() = value
            // Generate the value expression first, then emit FILL_ARRAY
            generateExpression(stmt->value.get());
            emit(IROpcode::FILL_ARRAY, stmt->variable);
            return;
        }
    }
    // End whole-array detection
    // =========================================================================

    // Generate code for the value expression
    generateExpression(stmt->value.get());

    // Check if this is a member assignment (e.g., P.X = value or Enemies(1).Name = value)
    if (!stmt->memberChain.empty()) {
        if (stmt->indices.empty()) {
            // Simple member assignment: variable.member = value
            // Load the record variable first
            emit(IROpcode::LOAD_VAR, stmt->variable);
            
            // For each member in the chain (supports nested: A.B.C)
            // We'll emit LOAD_MEMBER for all but the last, then STORE_MEMBER for the last
            for (size_t i = 0; i < stmt->memberChain.size(); ++i) {
                if (i < stmt->memberChain.size() - 1) {
                    // Intermediate member access
                    emit(IROpcode::LOAD_MEMBER, stmt->memberChain[i]);
                } else {
                    // Final member - store the value here
                    emit(IROpcode::STORE_MEMBER, stmt->memberChain[i]);
                }
            }
        } else {
            // Array element member assignment: array(index).member = value
            // Generate indices first
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }

            // Load array element as record
            emit(IROpcode::LOAD_ARRAY, stmt->variable, static_cast<int>(stmt->indices.size()));
            
            // For each member in the chain
            for (size_t i = 0; i < stmt->memberChain.size(); ++i) {
                if (i < stmt->memberChain.size() - 1) {
                    emit(IROpcode::LOAD_MEMBER, stmt->memberChain[i]);
                } else {
                    emit(IROpcode::STORE_MEMBER, stmt->memberChain[i]);
                }
            }
            
            // Store the modified record back to array
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            emit(IROpcode::STORE_ARRAY, stmt->variable, static_cast<int>(stmt->indices.size()));
        }
    } else if (!stmt->indices.empty()) {
        // Array element assignment: array(index) = value
        // Generate indices
        for (const auto& index : stmt->indices) {
            generateExpression(index.get());
        }

        // Emit array store instruction
        emit(IROpcode::STORE_ARRAY, stmt->variable, static_cast<int>(stmt->indices.size()));
    } else {
        // Simple variable assignment
        emit(IROpcode::STORE_VAR, stmt->variable);
    }
}

void IRGenerator::generateMidAssign(const MidAssignStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // MID$(var$, pos, len) = replacement$
    // Stack order: pos, len, replacement (will be popped in reverse)
    
    // 1. Generate position expression
    generateExpression(stmt->position.get());
    
    // 2. Generate length expression
    generateExpression(stmt->length.get());
    
    // 3. Generate replacement expression
    generateExpression(stmt->replacement.get());
    
    // 4. Emit MID_ASSIGN opcode (pops replacement, len, pos and updates variable)
    emit(IROpcode::MID_ASSIGN, stmt->variable);
}


void IRGenerator::generateIf(const IfStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate condition
    generateExpression(stmt->condition.get());

    // Check if this is IF...THEN GOTO lineNumber
    if (stmt->hasGoto) {
        // Simple conditional jump: IF condition THEN GOTO target
        int targetLabel = getLabelForLineNumber(stmt->gotoLine);
        emit(IROpcode::JUMP_IF_TRUE, targetLabel);
        // If false, fall through to next statement
        return;
    }

    // Otherwise, handle block-style IF with structured opcodes
    // IF_START: pops condition from stack
    emit(IROpcode::IF_START);

    // THEN block
    for (const auto& thenStmt : stmt->thenStatements) {
        generateStatement(thenStmt.get(), lineNumber);
    }

    // ELSEIF blocks
    for (size_t i = 0; i < stmt->elseIfClauses.size(); i++) {
        const auto& elseifClause = stmt->elseIfClauses[i];
        
        // Generate ELSEIF condition
        generateExpression(elseifClause.condition.get());
        
        // ELSEIF_START: pops condition from stack
        emit(IROpcode::ELSEIF_START);
        
        // ELSEIF body
        for (const auto& elseifStmt : elseifClause.statements) {
            generateStatement(elseifStmt.get(), lineNumber);
        }
    }

    // ELSE block
    if (!stmt->elseStatements.empty()) {
        emit(IROpcode::ELSE_START);
        for (const auto& elseStmt : stmt->elseStatements) {
            generateStatement(elseStmt.get(), lineNumber);
        }
    }

    // End of IF
    emit(IROpcode::IF_END);
}

void IRGenerator::generateCase(const CaseStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // CASE TRUE OF is essentially a chain of IF-ELSEIF-ELSE
    // We treat each WHEN clause as an IF/ELSEIF with its condition
    
    if (stmt->whenClauses.empty()) {
        // No WHEN clauses, just execute OTHERWISE if present
        if (!stmt->otherwiseStatements.empty()) {
            for (const auto& otherwiseStmt : stmt->otherwiseStatements) {
                generateStatement(otherwiseStmt.get(), lineNumber);
            }
        }
        return;
    }

    // Helper function to generate OR condition for multiple values
    auto generateWhenCondition = [&](const CaseStatement::WhenClause& clause) {
        if (clause.values.empty()) return;
        
        if (clause.values.size() == 1) {
            // Single value: just generate the comparison
            generateExpression(stmt->caseExpression.get());
            generateExpression(clause.values[0].get());
            emit(IROpcode::EQ);
        } else {
            // Multiple values: generate (expr = val1) OR (expr = val2) OR ...
            for (size_t i = 0; i < clause.values.size(); i++) {
                generateExpression(stmt->caseExpression.get());
                generateExpression(clause.values[i].get());
                emit(IROpcode::EQ);
                
                if (i > 0) {
                    // Chain with OR
                    emit(IROpcode::OR);
                }
            }
        }
    };

    // Generate first WHEN clause as IF
    const auto& firstClause = stmt->whenClauses[0];
    generateWhenCondition(firstClause);
    emit(IROpcode::IF_START);
    
    for (const auto& whenStmt : firstClause.statements) {
        generateStatement(whenStmt.get(), lineNumber);
    }

    // Generate remaining WHEN clauses as ELSEIF
    for (size_t i = 1; i < stmt->whenClauses.size(); i++) {
        const auto& whenClause = stmt->whenClauses[i];
        
        generateWhenCondition(whenClause);
        emit(IROpcode::ELSEIF_START);
        
        for (const auto& whenStmt : whenClause.statements) {
            generateStatement(whenStmt.get(), lineNumber);
        }
    }

    // Generate OTHERWISE as ELSE
    if (!stmt->otherwiseStatements.empty()) {
        emit(IROpcode::ELSE_START);
        for (const auto& otherwiseStmt : stmt->otherwiseStatements) {
            generateStatement(otherwiseStmt.get(), lineNumber);
        }
    }

    // End of CASE
    emit(IROpcode::IF_END);
}

void IRGenerator::generateFor(const ForStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Push start value (needed for FOR_INIT)
    generateExpression(stmt->start.get());

    // Push end value
    generateExpression(stmt->end.get());

    // Push step value
    if (stmt->step) {
        generateExpression(stmt->step.get());
    } else {
        emit(IROpcode::PUSH_INT, 1);  // Default step is 1
    }

    // FOR_INIT expects: start, end, step on stack
    // It will initialize the loop variable and push the loop context
    emit(IROpcode::FOR_INIT, stmt->variable);
}

void IRGenerator::generateForIn(const ForInStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate the array expression (leaves array reference on stack)
    generateExpression(stmt->array.get());

    // FOR_IN_INIT expects: array on stack
    // operand1: loop variable name
    // operand2: index variable name (or empty string if not used)
    std::string indexVar = stmt->indexVariable.empty() ? "" : stmt->indexVariable;
    emit(IROpcode::FOR_IN_INIT, stmt->variable, indexVar);
}

void IRGenerator::generateNext(const NextStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // NEXT increments the variable and checks loop condition
    // This needs to jump back to the loop start
    // The actual jump target is determined by the CFG

    if (!stmt->variable.empty()) {
        emit(IROpcode::FOR_NEXT, stmt->variable);
    } else {
        // NEXT without variable - use most recent FOR loop
        emit(IROpcode::FOR_NEXT, std::string(""));
    }
}

// Helper function to serialize simple expressions to a string for deferred evaluation
// Returns empty string for complex expressions (like function calls), which fall back to goto pattern
std::string IRGenerator::serializeExpression(const Expression* expr) {
    if (!expr) return "";
    
    ASTNodeType nodeType = expr->getType();
    
    switch (nodeType) {
        case ASTNodeType::EXPR_NUMBER: {
            auto* num = dynamic_cast<const NumberExpression*>(expr);
            return std::to_string(num->value);
        }
        
        case ASTNodeType::EXPR_STRING: {
            auto* str = dynamic_cast<const StringExpression*>(expr);
            std::string escaped = str->value;
            // Escape quotes and backslashes
            size_t pos = 0;
            while ((pos = escaped.find('\\', pos)) != std::string::npos) {
                escaped.replace(pos, 1, "\\\\");
                pos += 2;
            }
            pos = 0;
            while ((pos = escaped.find('"', pos)) != std::string::npos) {
                escaped.replace(pos, 1, "\\\"");
                pos += 2;
            }
            return "\"" + escaped + "\"";
        }
        
        case ASTNodeType::EXPR_VARIABLE: {
            auto* var = dynamic_cast<const VariableExpression*>(expr);
            // Add var_ prefix to match Lua code generation
            return "var_" + var->name;
        }
        
        case ASTNodeType::EXPR_BINARY: {
            auto* binop = dynamic_cast<const BinaryExpression*>(expr);
            std::string left = serializeExpression(binop->left.get());
            std::string right = serializeExpression(binop->right.get());
            if (left.empty() || right.empty()) return ""; // Can't serialize if subexpr failed
            
            std::string op;
            switch (binop->op) {
                case TokenType::PLUS: op = "+"; break;
                case TokenType::MINUS: op = "-"; break;
                case TokenType::MULTIPLY: op = "*"; break;
                case TokenType::DIVIDE: op = "/"; break;
                case TokenType::INT_DIVIDE: op = "//"; break;
                case TokenType::MOD: op = "%"; break;
                case TokenType::POWER: op = "^"; break;
                case TokenType::EQUAL: op = "=="; break;
                case TokenType::NOT_EQUAL: op = "~="; break;
                case TokenType::LESS_THAN: op = "<"; break;
                case TokenType::LESS_EQUAL: op = "<="; break;
                case TokenType::GREATER_THAN: op = ">"; break;
                case TokenType::GREATER_EQUAL: op = ">="; break;
                case TokenType::AND: op = "and"; break;
                case TokenType::OR: op = "or"; break;
                default: return ""; // Unknown operator, fall back
            }
            
            return "(" + left + " " + op + " " + right + ")";
        }
        
        case ASTNodeType::EXPR_UNARY: {
            auto* unop = dynamic_cast<const UnaryExpression*>(expr);
            std::string operand = serializeExpression(unop->expr.get());
            if (operand.empty()) return "";
            
            if (unop->op == TokenType::MINUS) {
                return "(-" + operand + ")";
            } else if (unop->op == TokenType::NOT) {
                return "(not " + operand + ")";
            }
            return operand;
        }
        
        case ASTNodeType::EXPR_FUNCTION_CALL:
            // Don't serialize function calls - they need to be re-evaluated via goto pattern
            return "";
        
        default:
            // For unsupported expressions, return empty to fall back to stack-based
            return "";
    }
}

void IRGenerator::generateWhile(const WhileStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Try to serialize the expression for deferred evaluation
    std::string serializedExpr = serializeExpression(stmt->condition.get());
    
    if (!serializedExpr.empty()) {
        // We can defer evaluation - pass the expression as a string
        // The code generator will use a native while loop
        emit(IROpcode::WHILE_START, serializedExpr);
        m_whileLoopLabels.push_back(-1); // No label needed for deferred evaluation
    } else {
        // Fall back to the old method for complex expressions
        // Emit a label before the condition so we can jump back to re-evaluate it
        int whileLabel = allocateLabel();
        emit(IROpcode::LABEL, whileLabel);
        
        // Push label onto stack so WEND knows where to jump back
        m_whileLoopLabels.push_back(whileLabel);
        
        // Generate condition expression - this will be re-evaluated each iteration
        generateExpression(stmt->condition.get());
        
        // WHILE_START: pops condition from stack, begins while loop
        // Store the loop label in operand1 for the code generator
        emit(IROpcode::WHILE_START, whileLabel);
    }
}

void IRGenerator::generateWend(const WendStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Pop the label from the stack
    if (m_whileLoopLabels.empty()) {
        throw std::runtime_error("WEND without matching WHILE");
    }
    int whileLabel = m_whileLoopLabels.back();
    m_whileLoopLabels.pop_back();
    
    // WHILE_END: marks end of while loop
    // If whileLabel is -1, we used deferred evaluation (no jump needed)
    // Otherwise, store the loop start label in operand1 so code generator can emit jump back
    if (whileLabel >= 0) {
        emit(IROpcode::WHILE_END, whileLabel);
    } else {
        emit(IROpcode::WHILE_END);
    }
}

void IRGenerator::generateRepeat(const RepeatStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // REPEAT_START: marks beginning of repeat loop body
    emit(IROpcode::REPEAT_START);
}

void IRGenerator::generateUntil(const UntilStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Generate condition expression
    generateExpression(stmt->condition.get());
    
    // REPEAT_END: pops condition from stack, loops if false
    emit(IROpcode::REPEAT_END);
}

void IRGenerator::generateDo(const DoStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Handle different DO variants based on condition type
    if (stmt->conditionType == DoStatement::ConditionType::WHILE) {
        // DO WHILE condition (pre-test)
        generateExpression(stmt->condition.get());
        emit(IROpcode::DO_WHILE_START);
    } else if (stmt->conditionType == DoStatement::ConditionType::UNTIL) {
        // DO UNTIL condition (pre-test)
        generateExpression(stmt->condition.get());
        emit(IROpcode::DO_UNTIL_START);
    } else {
        // Plain DO (infinite loop)
        emit(IROpcode::DO_START);
    }
}

void IRGenerator::generateLoop(const LoopStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Handle different LOOP variants based on condition type
    if (stmt->conditionType == LoopStatement::ConditionType::WHILE) {
        // LOOP WHILE condition (post-test)
        generateExpression(stmt->condition.get());
        emit(IROpcode::DO_LOOP_WHILE);
    } else if (stmt->conditionType == LoopStatement::ConditionType::UNTIL) {
        // LOOP UNTIL condition (post-test)
        generateExpression(stmt->condition.get());
        emit(IROpcode::DO_LOOP_UNTIL);
    } else {
        // Plain LOOP
        emit(IROpcode::DO_LOOP_END);
    }
}

void IRGenerator::generateGoto(const GotoStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    int targetLabel;
    int targetLineNumber = 0;
    
    if (stmt->isLabel) {
        // Symbolic label - look up the label ID from the symbol table
        auto it = m_symbols->labels.find(stmt->label);
        if (it != m_symbols->labels.end()) {
            targetLabel = it->second.labelId;
            // For symbolic labels, we can't easily check line numbers for back edges
            // This would need more complex analysis
        } else {
            // Error: undefined label (should have been caught in semantic analysis)
            targetLabel = allocateLabel();  // Fallback to avoid crash
        }
        emitLoopJump(IROpcode::JUMP, targetLabel, false);  // Conservative: don't mark symbolic labels as loops
    } else {
        // Line number - find the block containing this line
        targetLabel = getLabelForLineNumber(stmt->lineNumber);
        targetLineNumber = stmt->lineNumber;
        
        // Check if this GOTO creates a loop by using CFG analysis
        bool isLoop = false;
        if (m_cfg && lineNumber > 0 && targetLineNumber > 0) {
            isLoop = m_cfg->isBackEdge(lineNumber, targetLineNumber);
        }
        
        emitLoopJump(IROpcode::JUMP, targetLabel, isLoop);
    }
}

void IRGenerator::generateGosub(const GosubStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    int targetLabel;
    if (stmt->isLabel) {
        // Symbolic label - look up the label ID from the symbol table
        auto it = m_symbols->labels.find(stmt->label);
        if (it != m_symbols->labels.end()) {
            targetLabel = it->second.labelId;
        } else {
            // Error: undefined label (should have been caught in semantic analysis)
            targetLabel = allocateLabel();  // Fallback to avoid crash
        }
    } else {
        // Line number - find the block containing this line
        targetLabel = getLabelForLineNumber(stmt->lineNumber);
    }
    emit(IROpcode::CALL_GOSUB, targetLabel);
}

void IRGenerator::generateLabel(const LabelStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Look up the label ID from the symbol table
    auto it = m_symbols->labels.find(stmt->labelName);
    if (it != m_symbols->labels.end()) {
        int labelId = it->second.labelId;
        emit(IROpcode::LABEL, labelId);
    }
    // If label not found, semantic analysis should have caught it
}

void IRGenerator::generateExpressionStatement(const ExpressionStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // ExpressionStatement is used for graphics/API commands like PSET, SPRLOAD, etc.
    // These are essentially function calls to the runtime API
    
    // Generate arguments (push onto stack)
    for (const auto& arg : stmt->arguments) {
        generateExpression(arg.get());
    }
    
    // Emit a CALL_BUILTIN instruction with the command name and argument count
    emit(IROpcode::CALL_BUILTIN, stmt->name, static_cast<int>(stmt->arguments.size()));
}

void IRGenerator::generateSimpleStatement(const SimpleStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // SimpleStatement is used for commands with no arguments like BEEP, CLS, GCLS, etc.
    // These are essentially zero-argument function calls to the runtime API
    
    // Emit a CALL_BUILTIN instruction with the command name and 0 arguments
    emit(IROpcode::CALL_BUILTIN, stmt->name, 0);
}

void IRGenerator::generateReturn(const ReturnStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Check if RETURN has a value (for FUNCTION) or not (for SUB/GOSUB)
    if (stmt->returnValue) {
        // Generate the return value expression
        generateExpression(stmt->returnValue.get());
        // Emit RETURN_VALUE (pops value from stack and returns it)
        emit(IROpcode::RETURN_VALUE);
    } else {
        // No return value - could be GOSUB return or SUB return
        // For now, we emit RETURN_GOSUB for backward compatibility
        // TODO: Distinguish between RETURN_GOSUB and RETURN_VOID based on context
        emit(IROpcode::RETURN_GOSUB);
    }
}

void IRGenerator::generateOnGoto(const OnGotoStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate selector expression
    generateExpression(stmt->selector.get());

    // Build comma-separated list of target labels
    std::string targets;
    for (size_t i = 0; i < stmt->isLabelList.size(); i++) {
        if (i > 0) targets += ",";
        
        int targetLabel;
        if (stmt->isLabelList[i]) {
            // Symbolic label - look up the label ID
            auto it = m_symbols->labels.find(stmt->labels[i]);
            if (it != m_symbols->labels.end()) {
                targetLabel = it->second.labelId;
            } else {
                // Error: undefined label
                targetLabel = -1;
            }
        } else {
            // Line number - look up the label ID
            targetLabel = getLabelForLineNumber(stmt->lineNumbers[i]);
        }
        targets += std::to_string(targetLabel);
    }

    emit(IROpcode::ON_GOTO, targets);
}

void IRGenerator::generateOnGosub(const OnGosubStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate selector expression
    generateExpression(stmt->selector.get());

    // Build comma-separated list of target labels
    std::string targets;
    for (size_t i = 0; i < stmt->isLabelList.size(); i++) {
        if (i > 0) targets += ",";
        
        int targetLabel;
        if (stmt->isLabelList[i]) {
            // Symbolic label - look up the label ID
            auto it = m_symbols->labels.find(stmt->labels[i]);
            if (it != m_symbols->labels.end()) {
                targetLabel = it->second.labelId;
            } else {
                // Error: undefined label
                targetLabel = -1;
            }
        } else {
            // Line number - look up the label ID
            targetLabel = getLabelForLineNumber(stmt->lineNumbers[i]);
        }
        targets += std::to_string(targetLabel);
    }

    emit(IROpcode::ON_GOSUB, targets);
}

void IRGenerator::generateOnCall(const OnCallStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate selector expression
    generateExpression(stmt->selector.get());

    // Build comma-separated list of function names
    std::string targets;
    for (size_t i = 0; i < stmt->functionNames.size(); i++) {
        if (i > 0) targets += ",";
        targets += stmt->functionNames[i];
    }

    emit(IROpcode::ON_CALL, targets);
}

void IRGenerator::generateOnEvent(const OnEventStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Build operand string with event name and handler info
    // Format: "eventName|handlerType|target|isLineNumber"
    std::string operand = stmt->eventName + "|";
    
    std::string target = stmt->target;
    
    switch (stmt->handlerType) {
        case EventHandlerType::CALL:
            operand += "call";
            break;
        case EventHandlerType::GOTO:
            operand += "goto";
            // For GOTO, resolve line number to internal label ID
            if (stmt->isLineNumber) {
                int labelId = getLabelForLineNumber(std::stoi(stmt->target));
                target = std::to_string(labelId);
            }
            break;
        case EventHandlerType::GOSUB:
            operand += "gosub";
            // For GOSUB, resolve line number to internal label ID (same as generateGosub does)
            if (stmt->isLineNumber) {
                int labelId = getLabelForLineNumber(std::stoi(stmt->target));
                target = std::to_string(labelId);
            }
            break;
    }
    
    operand += "|" + target + "|";
    operand += stmt->isLineNumber ? "true" : "false";

    emit(IROpcode::ON_EVENT, operand);
}

void IRGenerator::generateConstant(const ConstantStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Constants are handled at compile time by the semantic analyzer.
    // The semantic analyzer evaluates constant expressions, stores them in the
    // C++ ConstantsManager with integer indices, and records those indices in the
    // symbol table. When code references a constant name, the IR generator emits
    // LOAD_CONST with the index (see generateExpression for VariableExpression).
    // No runtime code needs to be generated for the CONSTANT statement itself.
}

void IRGenerator::generateDim(const DimStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    for (const auto& arr : stmt->arrays) {
        // Handle user-defined types (TYPE declarations)
        if (arr.hasAsType && !arr.asTypeName.empty()) {
            // User-defined type: call the type constructor
            std::string constructorName = arr.asTypeName + "_new";
            
            if (arr.dimensions.empty()) {
                // Scalar typed variable: var = TypeName_new()
                emit(IROpcode::CALL_FUNCTION, constructorName, "0");
                emit(IROpcode::STORE_VAR, arr.name);
            } else {
                // Array of typed variables: need to handle array allocation
                // Push dimension sizes
                for (const auto& dim : arr.dimensions) {
                    generateExpression(dim.get());
                }
                
                // Allocate array with user type
                IRInstruction instr(IROpcode::DIM_ARRAY, arr.name,
                                   static_cast<int>(arr.dimensions.size()));
                instr.arrayElementTypeSuffix = ""; // User-defined type has no suffix
                instr.userDefinedType = arr.asTypeName;
                instr.sourceLineNumber = m_currentLineNumber;
                instr.blockId = m_currentBlockId;
                m_code->instructions.push_back(instr);
            }
            continue;
        }

        // Standard array/variable handling (built-in types)
        // Push dimension sizes
        for (const auto& dim : arr.dimensions) {
            generateExpression(dim.get());
        }

        // Extract type suffix (may be useful for future optimizations)
        std::string typeSuffix;
        switch (arr.typeSuffix) {
            case TokenType::TYPE_INT: typeSuffix = "%"; break;      // Integer
            case TokenType::TYPE_DOUBLE: typeSuffix = "#"; break;   // Double
            case TokenType::TYPE_FLOAT: typeSuffix = "!"; break;    // Float
            case TokenType::TYPE_STRING: typeSuffix = "$"; break;   // String
            default:
                // Fallback: extract suffix from array name
                typeSuffix = extractTypeSuffix(arr.name);
                break;
        }

        // Allocate array
        IRInstruction instr(IROpcode::DIM_ARRAY, arr.name,
                           static_cast<int>(arr.dimensions.size()));
        instr.arrayElementTypeSuffix = typeSuffix;
        instr.sourceLineNumber = m_currentLineNumber;
        instr.blockId = m_currentBlockId;
        m_code->instructions.push_back(instr);
    }
}

void IRGenerator::generateInput(const InputStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Handle file input (INPUT#)
    if (stmt->fileNumber > 0) {
        for (const auto& varName : stmt->variables) {
            if (stmt->isLineInput) {
                emit(IROpcode::LINE_INPUT_FILE, std::to_string(stmt->fileNumber), varName);
            } else {
                emit(IROpcode::INPUT_FILE, std::to_string(stmt->fileNumber), varName);
            }
        }
        return;
    }

    // If there's a prompt, use INPUT_PROMPT
    if (!stmt->prompt.empty()) {
        emit(IROpcode::INPUT_PROMPT, stmt->prompt);
    }

    // Read into each variable
    for (const auto& varName : stmt->variables) {
        emit(IROpcode::INPUT, varName);
    }
}

void IRGenerator::generateOpen(const OpenStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    emit(IROpcode::OPEN_FILE, stmt->filename, stmt->mode, std::to_string(stmt->fileNumber));
}

void IRGenerator::generateClose(const CloseStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    if (stmt->closeAll) {
        emit(IROpcode::CLOSE_FILE_ALL);
    } else {
        emit(IROpcode::CLOSE_FILE, std::to_string(stmt->fileNumber));
    }
}

void IRGenerator::generateRead(const ReadStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    for (const auto& varName : stmt->variables) {
        emit(IROpcode::READ_DATA, varName);
    }
}

void IRGenerator::generateRestore(const RestoreStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    if (stmt->isLabel) {
        // Emit label name as string - DataManager will resolve at runtime
        emit(IROpcode::RESTORE, stmt->label);
    } else if (stmt->lineNumber > 0) {
        // Emit line number as int - DataManager will resolve at runtime
        emit(IROpcode::RESTORE, stmt->lineNumber);
    } else {
        // No line number or label - restore to beginning (no operand)
        emit(IROpcode::RESTORE);
    }
}

void IRGenerator::generateExit(const ExitStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Emit the appropriate EXIT opcode based on exit type
    switch (stmt->exitType) {
        case ExitStatement::ExitType::FOR_LOOP:
            emit(IROpcode::EXIT_FOR);
            break;
        case ExitStatement::ExitType::DO_LOOP:
            emit(IROpcode::EXIT_DO);
            break;
        case ExitStatement::ExitType::WHILE_LOOP:
            emit(IROpcode::EXIT_WHILE);
            break;
        case ExitStatement::ExitType::REPEAT_LOOP:
            emit(IROpcode::EXIT_REPEAT);
            break;
        case ExitStatement::ExitType::FUNCTION:
            emit(IROpcode::EXIT_FUNCTION);
            break;
        case ExitStatement::ExitType::SUB:
            emit(IROpcode::EXIT_SUB);
            break;
    }
}

void IRGenerator::generateEnd(const EndStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    emit(IROpcode::END);
}

void IRGenerator::generateRem(const RemStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // REM statements generate no code (comments)
    emit(IROpcode::NOP);
}

void IRGenerator::generateDef(const DefStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Store the function definition for inlining at call sites
    UserFunction func;
    func.name = stmt->functionName;
    func.parameters = stmt->parameters;
    func.body = stmt->body.get();

    m_userFunctions[stmt->functionName] = func;

    // No IR emitted here - function body is inlined at call sites
}

void IRGenerator::generateFunction(const FunctionStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Store function definition
    FunctionDef func;
    func.name = stmt->functionName;
    func.parameters = stmt->parameters;
    func.parameterTypes = stmt->parameterTypes;
    func.parameterIsByRef = stmt->parameterIsByRef;
    func.returnType = stmt->returnTypeSuffix;
    func.astNode = stmt;

    // Name is already mangled in parser
    m_functions[stmt->functionName] = func;

    // Emit function definition start
    emit(IROpcode::DEFINE_FUNCTION, stmt->functionName);
    emit(IROpcode::PUSH_INT, static_cast<int>(stmt->parameters.size()));

    // Emit parameter names and BYREF flags (already mangled in parser)
    for (size_t i = 0; i < stmt->parameters.size(); i++) {
        emit(IROpcode::PUSH_STRING, stmt->parameters[i]);
        if (i < stmt->parameterIsByRef.size() && stmt->parameterIsByRef[i]) {
            emit(IROpcode::PARAM_BYREF, stmt->parameters[i]);
        }
    }

    // Generate function body
    for (const auto& bodyStmt : stmt->body) {
        generateStatement(bodyStmt.get(), lineNumber);
    }

    // Emit function end
    emit(IROpcode::END_FUNCTION);
}

void IRGenerator::generateSub(const SubStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Store sub definition
    SubDef sub;
    sub.name = stmt->subName;
    sub.parameters = stmt->parameters;
    sub.parameterTypes = stmt->parameterTypes;
    sub.parameterIsByRef = stmt->parameterIsByRef;
    sub.astNode = stmt;

    m_subs[stmt->subName] = sub;

    // Emit sub definition start
    emit(IROpcode::DEFINE_SUB, stmt->subName);
    emit(IROpcode::PUSH_INT, static_cast<int>(stmt->parameters.size()));

    // Emit parameter names and BYREF flags
    for (size_t i = 0; i < stmt->parameters.size(); i++) {
        emit(IROpcode::PUSH_STRING, stmt->parameters[i]);
        if (i < stmt->parameterIsByRef.size() && stmt->parameterIsByRef[i]) {
            emit(IROpcode::PARAM_BYREF, stmt->parameters[i]);
        }
    }

    // Generate sub body
    for (const auto& bodyStmt : stmt->body) {
        generateStatement(bodyStmt.get(), lineNumber);
    }

    // Emit sub end
    emit(IROpcode::END_SUB);
}

void IRGenerator::generateCall(const CallStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Track which arguments are simple variables (for BYREF support)
    std::vector<std::string> argVarNames;
    
    // Evaluate arguments in order
    for (const auto& arg : stmt->arguments) {
        // Check if this argument is a simple variable reference
        if (auto* varExpr = dynamic_cast<const VariableExpression*>(arg.get())) {
            // Simple variable - store its name for potential BYREF unpacking
            argVarNames.push_back(varExpr->name);
        } else {
            // Complex expression - cannot be used for BYREF
            argVarNames.push_back("");
        }
        generateExpression(arg.get());
    }

    // Build comma-separated list of argument variable names for operand3
    std::string argVarList;
    for (size_t i = 0; i < argVarNames.size(); i++) {
        if (i > 0) argVarList += ",";
        argVarList += argVarNames[i];
    }

    // Emit call instruction with argument variable names in operand3
    auto instr = IRInstruction();
    instr.opcode = IROpcode::CALL_SUB;
    instr.operand1 = stmt->subName;
    instr.operand2 = static_cast<int>(stmt->arguments.size());
    instr.operand3 = argVarList;
    instr.sourceLineNumber = m_currentLineNumber;
    instr.blockId = m_currentBlockId;
    m_code->instructions.push_back(instr);
}

void IRGenerator::generateLocal(const LocalStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Emit LOCAL variable declarations and initializations
    for (const auto& var : stmt->variables) {
        emit(IROpcode::DECLARE_LOCAL, var.name);
        
        // If this is a user-defined type, initialize with constructor
        if (var.hasAsType && !var.asTypeName.empty()) {
            std::string constructorName = var.asTypeName + "_new";
            emit(IROpcode::CALL_FUNCTION, constructorName, "0");
            emit(IROpcode::STORE_VAR, var.name);
        }
        // If there's an explicit initial value, generate assignment
        else if (var.initialValue) {
            generateExpression(var.initialValue.get());
            emit(IROpcode::STORE_VAR, var.name);
        }
    }
}

void IRGenerator::generateShared(const SharedStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Emit SHARED variable declarations (marks as module-level access)
    for (const auto& var : stmt->variables) {
        emit(IROpcode::DECLARE_SHARED, var.name);
    }
}

void IRGenerator::generateRedim(const RedimStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    for (const auto& arr : stmt->arrays) {
        // Push dimension sizes
        for (const auto& dim : arr.dimensions) {
            generateExpression(dim.get());
        }

        // Emit REDIM instruction with preserve flag
        IRInstruction instr(IROpcode::REDIM_ARRAY, arr.name,
                           static_cast<int>(arr.dimensions.size()));
        instr.sourceLineNumber = m_currentLineNumber;
        instr.blockId = m_currentBlockId;
        // Use operand3 to store preserve flag
        instr.operand3 = stmt->preserve ? 1 : 0;
        m_code->instructions.push_back(instr);
    }
}

void IRGenerator::generateErase(const EraseStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    for (const auto& arrayName : stmt->arrayNames) {
        emit(IROpcode::ERASE_ARRAY, arrayName);
    }
}

void IRGenerator::generateSwap(const SwapStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    emit(IROpcode::SWAP_VAR, stmt->var1, stmt->var2);
}

void IRGenerator::generateInc(const IncStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Generate: var = var + increment (or array(i) = array(i) + increment, or record.member = record.member + increment)
    
    // Check if this is a member increment (e.g., P.X or Enemies(1).Name)
    if (!stmt->memberChain.empty()) {
        if (stmt->indices.empty()) {
            // Simple member increment: variable.member = variable.member + increment
            // Load the record variable first
            emit(IROpcode::LOAD_VAR, stmt->varName);
            
            // For each member in the chain (supports nested: A.B.C)
            // We'll emit LOAD_MEMBER for all members to get the value
            for (const auto& member : stmt->memberChain) {
                emit(IROpcode::LOAD_MEMBER, member);
            }
            
            // Push increment value
            if (stmt->incrementExpr) {
                generateExpression(stmt->incrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Add them
            emit(IROpcode::ADD);
            
            // Now store back - load record again and navigate to final member
            emit(IROpcode::LOAD_VAR, stmt->varName);
            for (size_t i = 0; i < stmt->memberChain.size(); ++i) {
                if (i < stmt->memberChain.size() - 1) {
                    // Intermediate member access
                    emit(IROpcode::LOAD_MEMBER, stmt->memberChain[i]);
                } else {
                    // Final member - store the value here
                    emit(IROpcode::STORE_MEMBER, stmt->memberChain[i]);
                }
            }
        } else {
            // Array element member increment: array(index).member = array(index).member + increment
            
            // Build member path
            std::string memberPath;
            for (size_t i = 0; i < stmt->memberChain.size(); ++i) {
                if (i > 0) memberPath += ".";
                memberPath += stmt->memberChain[i];
            }
            
            // First, load the current value: generate indices and load array member
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            emit(IROpcode::LOAD_ARRAY_MEMBER, stmt->varName, memberPath, static_cast<int>(stmt->indices.size()));
            
            // Push increment value
            if (stmt->incrementExpr) {
                generateExpression(stmt->incrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Add them
            emit(IROpcode::ADD);
            
            // Store back: generate indices again and store to array member
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            emit(IROpcode::STORE_ARRAY_MEMBER, stmt->varName, memberPath, static_cast<int>(stmt->indices.size()));
        }
    } else {
        // Regular increment (no member access)
        if (stmt->indices.empty()) {
            // Simple variable increment
            emit(IROpcode::LOAD_VAR, stmt->varName);
            
            // Push increment value
            if (stmt->incrementExpr) {
                generateExpression(stmt->incrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Add them
            emit(IROpcode::ADD);
            
            // Store result back
            emit(IROpcode::STORE_VAR, stmt->varName);
        } else {
            // Array element increment: array(i) = array(i) + increment
            
            // Load current value: generate indices and load from array
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            
            std::string typeSuffix = extractTypeSuffix(stmt->varName);
            IRInstruction loadInstr(IROpcode::LOAD_ARRAY, stmt->varName,
                                   static_cast<int>(stmt->indices.size()));
            loadInstr.arrayElementTypeSuffix = typeSuffix;
            loadInstr.sourceLineNumber = m_currentLineNumber;
            loadInstr.blockId = m_currentBlockId;
            m_code->instructions.push_back(loadInstr);
            
            // Push increment value
            if (stmt->incrementExpr) {
                generateExpression(stmt->incrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Add them
            emit(IROpcode::ADD);
            
            // Store result back: generate indices again and store to array
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            
            IRInstruction storeInstr(IROpcode::STORE_ARRAY, stmt->varName,
                                    static_cast<int>(stmt->indices.size()));
            storeInstr.arrayElementTypeSuffix = typeSuffix;
            storeInstr.sourceLineNumber = m_currentLineNumber;
            storeInstr.blockId = m_currentBlockId;
            m_code->instructions.push_back(storeInstr);
        }
    }
}

void IRGenerator::generateDec(const DecStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Generate: var = var - decrement (or array(i) = array(i) - decrement, or record.member = record.member - decrement)
    
    // Check if this is a member decrement (e.g., P.X or Enemies(1).Name)
    if (!stmt->memberChain.empty()) {
        if (stmt->indices.empty()) {
            // Simple member decrement: variable.member = variable.member - decrement
            // Load the record variable first
            emit(IROpcode::LOAD_VAR, stmt->varName);
            
            // For each member in the chain (supports nested: A.B.C)
            // We'll emit LOAD_MEMBER for all members to get the value
            for (const auto& member : stmt->memberChain) {
                emit(IROpcode::LOAD_MEMBER, member);
            }
            
            // Push decrement value
            if (stmt->decrementExpr) {
                generateExpression(stmt->decrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Subtract them
            emit(IROpcode::SUB);
            
            // Now store back - load record again and navigate to final member
            emit(IROpcode::LOAD_VAR, stmt->varName);
            for (size_t i = 0; i < stmt->memberChain.size(); ++i) {
                if (i < stmt->memberChain.size() - 1) {
                    // Intermediate member access
                    emit(IROpcode::LOAD_MEMBER, stmt->memberChain[i]);
                } else {
                    // Final member - store the value here
                    emit(IROpcode::STORE_MEMBER, stmt->memberChain[i]);
                }
            }
        } else {
            // Array element member decrement: array(index).member = array(index).member - decrement
            
            // Build member path
            std::string memberPath;
            for (size_t i = 0; i < stmt->memberChain.size(); ++i) {
                if (i > 0) memberPath += ".";
                memberPath += stmt->memberChain[i];
            }
            
            // First, load the current value: generate indices and load array member
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            emit(IROpcode::LOAD_ARRAY_MEMBER, stmt->varName, memberPath, static_cast<int>(stmt->indices.size()));
            
            // Push decrement value
            if (stmt->decrementExpr) {
                generateExpression(stmt->decrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Subtract them
            emit(IROpcode::SUB);
            
            // Store back: generate indices again and store to array member
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            emit(IROpcode::STORE_ARRAY_MEMBER, stmt->varName, memberPath, static_cast<int>(stmt->indices.size()));
        }
    } else {
        // Regular decrement (no member access)
        if (stmt->indices.empty()) {
            // Simple variable decrement
            emit(IROpcode::LOAD_VAR, stmt->varName);
            
            // Push decrement value
            if (stmt->decrementExpr) {
                generateExpression(stmt->decrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Subtract them
            emit(IROpcode::SUB);
            
            // Store result back
            emit(IROpcode::STORE_VAR, stmt->varName);
        } else {
            // Array element decrement: array(i) = array(i) - decrement
            
            // Load current value: generate indices and load from array
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            
            std::string typeSuffix = extractTypeSuffix(stmt->varName);
            IRInstruction loadInstr(IROpcode::LOAD_ARRAY, stmt->varName,
                                   static_cast<int>(stmt->indices.size()));
            loadInstr.arrayElementTypeSuffix = typeSuffix;
            loadInstr.sourceLineNumber = m_currentLineNumber;
            loadInstr.blockId = m_currentBlockId;
            m_code->instructions.push_back(loadInstr);
            
            // Push decrement value
            if (stmt->decrementExpr) {
                generateExpression(stmt->decrementExpr.get());
            } else {
                emit(IROpcode::PUSH_INT, 1);
            }
            
            // Subtract them
            emit(IROpcode::SUB);
            
            // Store result back: generate indices again and store to array
            for (const auto& index : stmt->indices) {
                generateExpression(index.get());
            }
            
            IRInstruction storeInstr(IROpcode::STORE_ARRAY, stmt->varName,
                                    static_cast<int>(stmt->indices.size()));
            storeInstr.arrayElementTypeSuffix = typeSuffix;
            storeInstr.sourceLineNumber = m_currentLineNumber;
            storeInstr.blockId = m_currentBlockId;
            m_code->instructions.push_back(storeInstr);
        }
    }
}

// =============================================================================
// Expression Code Generation
// =============================================================================

void IRGenerator::generateExpression(const Expression* expr) {
    if (!expr) {
        emit(IROpcode::PUSH_INT, 0);  // Default to 0
        return;
    }

    if (auto* e = dynamic_cast<const NumberExpression*>(expr)) {
        // Push numeric constant
        // Check if it's an integer or float by checking if it has a fractional part
        double intpart;
        if (std::modf(e->value, &intpart) == 0.0 && e->value >= INT_MIN && e->value <= INT_MAX) {
            emit(IROpcode::PUSH_INT, static_cast<int>(e->value));
        } else {
            emit(IROpcode::PUSH_DOUBLE, e->value);
        }
    }
    else if (auto* e = dynamic_cast<const StringExpression*>(expr)) {
        // Push string constant
        emit(IROpcode::PUSH_STRING, e->value);
    }
    else if (auto* e = dynamic_cast<const VariableExpression*>(expr)) {
        // Check if this is a constant first (case-insensitive lookup)
        std::string lowerName = e->name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (m_symbols && m_symbols->constants.find(lowerName) != m_symbols->constants.end()) {
            // This is a constant - load by index from ConstantsManager
            const auto& constValue = m_symbols->constants.at(lowerName);
            emit(IROpcode::LOAD_CONST, constValue.index);
        }
        // Load variable - check if it's a function parameter being inlined
        else if (m_inFunctionInlining && m_parameterMap.find(e->name) != m_parameterMap.end()) {
            // This is a function parameter - load from the temporary variable
            emit(IROpcode::LOAD_VAR, m_parameterMap[e->name]);
        } else {
            // Regular variable
            emit(IROpcode::LOAD_VAR, e->name);
        }
    }
    else if (auto* e = dynamic_cast<const ArrayAccessExpression*>(expr)) {
        // Check for LBOUND/UBOUND first - replace with compile-time constants
        std::string funcNameUpper = e->name;
        std::transform(funcNameUpper.begin(), funcNameUpper.end(), funcNameUpper.begin(), ::toupper);
        
        if (funcNameUpper == "LBOUND" || funcNameUpper == "UBOUND") {
            // LBOUND/UBOUND: Replace with compile-time constants
            if (e->indices.empty() || !m_symbols) {
                emit(IROpcode::PUSH_INT, 0);  // Error case
            } else {
                // First argument should be array name
                if (auto* varExpr = dynamic_cast<const VariableExpression*>(e->indices[0].get())) {
                    std::string arrayName = varExpr->name;
                    
                    // Look up the array in the symbol table
                    auto it = m_symbols->arrays.find(arrayName);
                    if (it == m_symbols->arrays.end()) {
                        emit(IROpcode::PUSH_INT, 0);  // Array not found
                    } else {
                        const ArraySymbol& arraySymbol = it->second;
                        
                        // Get dimension (default to 1 if not specified)
                        int dimension = 1;
                        if (e->indices.size() > 1) {
                            if (auto* numExpr = dynamic_cast<const NumberExpression*>(e->indices[1].get())) {
                                dimension = static_cast<int>(numExpr->value);
                            }
                        }
                        
                        // Validate dimension
                        if (dimension < 1 || dimension > static_cast<int>(arraySymbol.dimensions.size())) {
                            emit(IROpcode::PUSH_INT, 0);  // Invalid dimension
                        } else {
                            int value;
                            if (funcNameUpper == "LBOUND") {
                                // LBOUND always returns OPTION BASE (0 or 1)
                                value = m_code->arrayBase;
                            } else {
                                // UBOUND returns the upper bound for the specified dimension
                                // Semantic analyzer stores size+1 in dimensions (DIM A(10) stores 11)
                                // DIM A(10) means 10 elements: with BASE 1 = [1..10], with BASE 0 = [0..9]
                                // So UBOUND should return stored_dimension - 1
                                value = arraySymbol.dimensions[dimension - 1] - 1;
                            }
                            emit(IROpcode::PUSH_INT, value);
                        }
                    }
                } else {
                    emit(IROpcode::PUSH_INT, 0);  // Argument is not a variable
                }
            }
            return;  // Done handling LBOUND/UBOUND
        }
        
        // Check symbol table to determine if this is an array or function call
        // Priority: 1) Check if it's a declared array, 2) Check if it's a built-in function

        bool isArray = false;
        if (m_symbols) {
            // Check if this identifier was declared as an array (via DIM)
            auto arrayIt = m_symbols->arrays.find(e->name);
            if (arrayIt != m_symbols->arrays.end()) {
                isArray = true;
            }
        }

        if (isArray) {
            // This is a declared array - generate array access
            // Generate indices
            for (const auto& index : e->indices) {
                generateExpression(index.get());
            }

            // Extract type suffix (may be useful for future optimizations)
            std::string typeSuffix = extractTypeSuffix(e->name);

            // Load array element
            IRInstruction instr(IROpcode::LOAD_ARRAY, e->name,
                               static_cast<int>(e->indices.size()));
            instr.arrayElementTypeSuffix = typeSuffix;
            instr.sourceLineNumber = m_currentLineNumber;
            instr.blockId = m_currentBlockId;
            m_code->instructions.push_back(instr);
        } else {
            // Not a declared array - check if it's a user-defined function or FUNCTION
            if (m_userFunctions.find(e->name) != m_userFunctions.end()) {
                // DEF FN - inline it
                std::vector<const Expression*> args;
                for (const auto& idx : e->indices) {
                    args.push_back(idx.get());
                }
                generateInlinedFunction(e->name, args);
            } else if (m_functions.find(e->name) != m_functions.end()) {
                // FUNCTION - emit call
                // Track which arguments are simple variables (for BYREF support)
                std::vector<std::string> argVarNames;
                for (const auto& idx : e->indices) {
                    // Check if this argument is a simple variable reference
                    if (auto* varExpr = dynamic_cast<const VariableExpression*>(idx.get())) {
                        argVarNames.push_back(varExpr->name);
                    } else {
                        argVarNames.push_back("");
                    }
                    generateExpression(idx.get());
                }
                
                // Build comma-separated list of argument variable names
                std::string argVarList;
                for (size_t i = 0; i < argVarNames.size(); i++) {
                    if (i > 0) argVarList += ",";
                    argVarList += argVarNames[i];
                }
                
                // Emit call with argument variable names in operand3
                auto instr = IRInstruction();
                instr.opcode = IROpcode::CALL_FUNCTION;
                instr.operand1 = e->name;
                instr.operand2 = static_cast<int>(e->indices.size());
                instr.operand3 = argVarList;
                instr.sourceLineNumber = m_currentLineNumber;
                instr.blockId = m_currentBlockId;
                m_code->instructions.push_back(instr);
            } else {
                // Built-in function call
                // Built-in functions: ABS, SIN, COS, TAN, ATN, SQR, INT, SGN, LOG, EXP, RND,
                //                     LEN, ASC, CHR$, STR$, VAL, LEFT$, RIGHT$, MID$
                // Generate arguments
                for (const auto& index : e->indices) {
                    generateExpression(index.get());
                }
                // Call function
                emit(IROpcode::CALL_BUILTIN, e->name,
                     static_cast<int>(e->indices.size()));
            }
        }
    }
    else if (auto* e = dynamic_cast<const BinaryExpression*>(expr)) {
        // Generate left and right operands
        generateExpression(e->left.get());
        generateExpression(e->right.get());

        // Generate operation based on TokenType
        switch (e->op) {
            case TokenType::PLUS: {
                // Check if this is string concatenation or numeric addition
                // String concatenation uses STR_CONCAT or UNICODE_CONCAT, numeric uses ADD
                if (isStringExpression(e->left.get()) || isStringExpression(e->right.get())) {
                    // Use UNICODE_CONCAT in Unicode mode, STR_CONCAT otherwise
                    if (m_symbols->unicodeMode) {
                        emit(IROpcode::UNICODE_CONCAT);
                    } else {
                        emit(IROpcode::STR_CONCAT);
                    }
                } else {
                    emit(IROpcode::ADD);
                }
                break;
            }
            case TokenType::MINUS: emit(IROpcode::SUB); break;
            case TokenType::MULTIPLY: emit(IROpcode::MUL); break;
            case TokenType::DIVIDE: emit(IROpcode::DIV); break;
            case TokenType::INT_DIVIDE: emit(IROpcode::IDIV); break;
            case TokenType::POWER: emit(IROpcode::POW); break;
            case TokenType::MOD: emit(IROpcode::MOD); break;
            case TokenType::EQUAL: emit(IROpcode::EQ); break;
            case TokenType::NOT_EQUAL: emit(IROpcode::NE); break;
            case TokenType::LESS_THAN: emit(IROpcode::LT); break;
            case TokenType::LESS_EQUAL: emit(IROpcode::LE); break;
            case TokenType::GREATER_THAN: emit(IROpcode::GT); break;
            case TokenType::GREATER_EQUAL: emit(IROpcode::GE); break;
            case TokenType::AND: emit(IROpcode::AND); break;
            case TokenType::OR: emit(IROpcode::OR); break;
            case TokenType::XOR: emit(IROpcode::XOR); break;
            case TokenType::EQV: emit(IROpcode::EQV); break;
            case TokenType::IMP: emit(IROpcode::IMP); break;
            default:
                // Unknown operator - emit NOP as fallback
                emit(IROpcode::NOP);
                break;
        }
    }
    else if (auto* e = dynamic_cast<const UnaryExpression*>(expr)) {
        // Generate operand
        generateExpression(e->expr.get());

        // Generate operation based on TokenType
        switch (e->op) {
            case TokenType::MINUS: emit(IROpcode::NEG); break;
            case TokenType::NOT: emit(IROpcode::NOT); break;
            case TokenType::PLUS: /* no-op */ break;
            default:
                // Unknown unary operator - emit NOP as fallback
                emit(IROpcode::NOP);
                break;
        }
    }
    else if (auto* e = dynamic_cast<const FunctionCallExpression*>(expr)) {
        // Handle special array functions that need array names, not values
        // Convert name to uppercase for comparison (function names are case-insensitive)
        std::string funcNameUpper = e->name;
        std::transform(funcNameUpper.begin(), funcNameUpper.end(), funcNameUpper.begin(), ::toupper);
        
        // Check if this is a user-defined function (DEF FN or FUNCTION)
        if (m_userFunctions.find(e->name) != m_userFunctions.end()) {
            // DEF FN - inline it
            std::vector<const Expression*> args;
            for (const auto& arg : e->arguments) {
                args.push_back(arg.get());
            }
            generateInlinedFunction(e->name, args);
        } else if (m_functions.find(e->name) != m_functions.end()) {
            // FUNCTION - emit call
            // Track which arguments are simple variables (for BYREF support)
            std::vector<std::string> argVarNames;
            for (const auto& arg : e->arguments) {
                // Check if this argument is a simple variable reference
                if (auto* varExpr = dynamic_cast<const VariableExpression*>(arg.get())) {
                    argVarNames.push_back(varExpr->name);
                } else {
                    argVarNames.push_back("");
                }
                generateExpression(arg.get());
            }
            
            // Build comma-separated list of argument variable names
            std::string argVarList;
            for (size_t i = 0; i < argVarNames.size(); i++) {
                if (i > 0) argVarList += ",";
                argVarList += argVarNames[i];
            }
            
            // Emit call with argument variable names in operand3
            auto instr = IRInstruction();
            instr.opcode = IROpcode::CALL_FUNCTION;
            instr.operand1 = e->name;
            instr.operand2 = static_cast<int>(e->arguments.size());
            instr.operand3 = argVarList;
            instr.sourceLineNumber = m_currentLineNumber;
            instr.blockId = m_currentBlockId;
            m_code->instructions.push_back(instr);
        } else {
            // Built-in function call
            // Generate arguments
            for (const auto& arg : e->arguments) {
                generateExpression(arg.get());
            }
            // Call function
            emit(IROpcode::CALL_BUILTIN, e->name,
                 static_cast<int>(e->arguments.size()));
        }
    }
    else if (auto* e = dynamic_cast<const RegistryFunctionExpression*>(expr)) {
        // Registry function call - generate arguments and emit CALL_BUILTIN
        for (const auto& arg : e->arguments) {
            generateExpression(arg.get());
        }
        // Call registry function using CALL_BUILTIN (same as built-in functions)
        emit(IROpcode::CALL_BUILTIN, e->name, static_cast<int>(e->arguments.size()));
    }
    else if (auto* e = dynamic_cast<const IIFExpression*>(expr)) {
        // IIF(condition, trueValue, falseValue) - inline conditional expression
        // We'll handle this directly in Lua codegen to emit ternary expression
        // For now, generate a placeholder builtin call that codegen will recognize
        
        // Push the three expressions onto the stack in order
        generateExpression(e->condition.get());
        generateExpression(e->trueValue.get());
        generateExpression(e->falseValue.get());
        
        // Emit special builtin call for IIF with 3 arguments
        emit(IROpcode::CALL_BUILTIN, "__IIF", 3);
    }
    else if (auto* e = dynamic_cast<const MemberAccessExpression*>(expr)) {
        // Member access expression: object.member
        // Generate the object expression first (pushes record on stack)
        generateExpression(e->object.get());
        
        // Emit LOAD_MEMBER to pop record and push member value
        emit(IROpcode::LOAD_MEMBER, e->memberName);
    }
}

void IRGenerator::generateInlinedFunction(const std::string& funcName,
                                          const std::vector<const Expression*>& arguments) {
    auto it = m_userFunctions.find(funcName);
    if (it == m_userFunctions.end()) {
        // Function not found - should not happen if semantic analysis is correct
        emit(IROpcode::PUSH_INT, 0);  // Push dummy value
        return;
    }

    const UserFunction& func = it->second;

    // Strategy: Evaluate arguments and store in temporary variables,
    // then set up parameter mapping for expression generation

    // Save current parameter map
    auto savedParamMap = m_parameterMap;
    bool savedInlining = m_inFunctionInlining;

    // Evaluate all arguments and store in temporary variables
    for (size_t i = 0; i < arguments.size() && i < func.parameters.size(); i++) {
        generateExpression(arguments[i]);
        std::string tempVar = "__fn_" + funcName + "_" + func.parameters[i];
        emit(IROpcode::STORE_VAR, tempVar);

        // Map parameter name to temp variable name
        m_parameterMap[func.parameters[i]] = tempVar;
    }

    // Enable function inlining mode
    m_inFunctionInlining = true;

    // Evaluate the function body with parameter substitution
    generateExpression(func.body);

    // Restore previous state
    m_parameterMap = savedParamMap;
    m_inFunctionInlining = savedInlining;
}

// =============================================================================
// Helper Methods
// =============================================================================

int IRGenerator::getLabelForBlock(int blockId) {
    auto it = m_blockLabels.find(blockId);
    if (it != m_blockLabels.end()) {
        return it->second;
    }

    int labelId = allocateLabel();
    m_blockLabels[blockId] = labelId;
    return labelId;
}

int IRGenerator::getLabelForLineNumber(int lineNumber) {
    // Use CFG's getBlockForLineOrNext to find the block for this line
    // (or the next available line if the target doesn't exist)
    int blockId = m_cfg->getBlockForLineOrNext(lineNumber);
    
    if (blockId >= 0) {
        return getLabelForBlock(blockId);
    }

    // If not found at all (shouldn't happen with valid CFG), create a new label
    return allocateLabel();
}

int IRGenerator::allocateLabel() {
    return m_nextLabel++;
}

void IRGenerator::emit(IROpcode opcode) {
    IRInstruction instr(opcode);
    instr.sourceLineNumber = m_currentLineNumber;
    instr.blockId = m_currentBlockId;
    m_code->emit(instr);
}

void IRGenerator::emit(IROpcode opcode, IROperand op1) {
    IRInstruction instr(opcode, op1);
    instr.sourceLineNumber = m_currentLineNumber;
    instr.blockId = m_currentBlockId;
    m_code->emit(instr);
}

void IRGenerator::emit(IROpcode opcode, IROperand op1, IROperand op2) {
    IRInstruction instr(opcode, op1, op2);
    instr.sourceLineNumber = m_currentLineNumber;
    instr.blockId = m_currentBlockId;
    m_code->emit(instr);
}

void IRGenerator::emit(IROpcode opcode, IROperand op1, IROperand op2, IROperand op3) {
    IRInstruction instr(opcode, op1, op2, op3);
    instr.sourceLineNumber = m_currentLineNumber;
    instr.blockId = m_currentBlockId;
    m_code->emit(instr);
}

void IRGenerator::emitLoopJump(IROpcode opcode, IROperand op1, bool isLoop) {
    IRInstruction instr(opcode, op1);
    instr.sourceLineNumber = m_currentLineNumber;
    instr.blockId = m_currentBlockId;
    instr.isLoopJump = isLoop;
    m_code->emit(instr);
}

void IRGenerator::setSourceContext(int lineNumber, int blockId) {
    m_currentLineNumber = lineNumber;
    m_currentBlockId = blockId;
}

// =============================================================================
// Report Generation
// =============================================================================

std::string IRGenerator::generateReport(const IRCode& code) const {
    std::ostringstream oss;

    oss << "=== IR CODE GENERATION REPORT ===\n\n";

    // Statistics
    oss << "Statistics:\n";
    oss << "  Total Instructions: " << code.size() << "\n";
    oss << "  Total Labels: " << code.labelCount << "\n";
    oss << "  Total Blocks: " << code.blockCount << "\n";
    oss << "  Line Mappings: " << code.lineToAddress.size() << "\n";
    oss << "\n";

    // Line number mappings
    if (!code.lineToAddress.empty()) {
        oss << "Line Number → Address Mappings:\n";
        for (const auto& [line, addr] : code.lineToAddress) {
            oss << "  Line " << std::setw(5) << line << " → "
                << std::setw(5) << std::setfill('0') << addr
                << std::setfill(' ') << "\n";
        }
        oss << "\n";
    }

    // Instruction listing
    oss << "Instruction Listing:\n";
    oss << code.toString();

    oss << "\n=== END OF IR CODE ===\n";

    return oss.str();
}

// =============================================================================
// Type Checking Helpers
// =============================================================================

bool IRGenerator::isStringExpression(const Expression* expr) const {
    if (!expr || !m_symbols) return false;
    
    // Check string literal
    if (auto* strExpr = dynamic_cast<const StringExpression*>(expr)) {
        return true;
    }
    
    // Check variable - look up in symbol table
    if (auto* varExpr = dynamic_cast<const VariableExpression*>(expr)) {
        // Check in variables table
        auto it = m_symbols->variables.find(varExpr->name);
        if (it != m_symbols->variables.end()) {
            return it->second.type == VariableType::STRING || it->second.type == VariableType::UNICODE;
        }
        // Also check in arrays table (DIM x$ AS STRING creates arrays)
        auto arrIt = m_symbols->arrays.find(varExpr->name);
        if (arrIt != m_symbols->arrays.end()) {
            return arrIt->second.type == VariableType::STRING || arrIt->second.type == VariableType::UNICODE;
        }
        // Check suffix if not in symbol table
        return !varExpr->name.empty() && 
            (varExpr->name.back() == '$' || 
             (varExpr->name.size() > 7 && 
              varExpr->name.substr(varExpr->name.size() - 7) == "_STRING"));
    }
    
    // Check array access (which might be a function call in disguise)
    if (auto* arrExpr = dynamic_cast<const ArrayAccessExpression*>(expr)) {
        // Built-in string functions (both original and mangled names)
        if (arrExpr->name == "LEFT$" || arrExpr->name == "RIGHT$" || 
            arrExpr->name == "MID$" || arrExpr->name == "CHR$" || 
            arrExpr->name == "STR$" ||
            arrExpr->name == "LEFT_STRING" || arrExpr->name == "RIGHT_STRING" ||
            arrExpr->name == "MID_STRING" || arrExpr->name == "CHR_STRING" ||
            arrExpr->name == "STR_STRING" || arrExpr->name == "STRING_STRING" ||
            arrExpr->name == "SPACE_STRING" || arrExpr->name == "LCASE_STRING" ||
            arrExpr->name == "UCASE_STRING" || arrExpr->name == "LTRIM_STRING" ||
            arrExpr->name == "RTRIM_STRING" || arrExpr->name == "TRIM_STRING" ||
            arrExpr->name == "REVERSE_STRING") {
            return true;
        }
        // User-defined function with $ suffix or _STRING suffix
        if (!arrExpr->name.empty() && 
            (arrExpr->name.back() == '$' || 
             (arrExpr->name.size() > 7 && 
             arrExpr->name.substr(arrExpr->name.size() - 7) == "_STRING"))) {
            return true;
        }
    }
    
    // Check function call
    if (auto* callExpr = dynamic_cast<const FunctionCallExpression*>(expr)) {
        // Built-in string functions (both original and mangled names)
        if (callExpr->name == "LEFT$" || callExpr->name == "RIGHT$" || 
            callExpr->name == "MID$" || callExpr->name == "CHR$" || 
            callExpr->name == "STR$" ||
            callExpr->name == "LEFT_STRING" || callExpr->name == "RIGHT_STRING" ||
            callExpr->name == "MID_STRING" || callExpr->name == "CHR_STRING" ||
            callExpr->name == "STR_STRING" || callExpr->name == "STRING_STRING" ||
            callExpr->name == "SPACE_STRING" || callExpr->name == "LCASE_STRING" ||
            callExpr->name == "UCASE_STRING" || callExpr->name == "LTRIM_STRING" ||
            callExpr->name == "RTRIM_STRING" || callExpr->name == "TRIM_STRING" ||
            callExpr->name == "REVERSE_STRING") {
            return true;
        }
        // User-defined function with $ suffix or _STRING suffix
        if (!callExpr->name.empty() && 
            (callExpr->name.back() == '$' || 
             (callExpr->name.size() > 7 && 
             callExpr->name.substr(callExpr->name.size() - 7) == "_STRING"))) {
            return true;
        }
    }
    
    // Check binary expression - if either side is string, result is string
    if (auto* binExpr = dynamic_cast<const BinaryExpression*>(expr)) {
        if (binExpr->op == TokenType::PLUS) {
            return isStringExpression(binExpr->left.get()) || 
                   isStringExpression(binExpr->right.get());
        }
    }
    
    // Check IIF expression - return type depends on true/false values
    if (auto* iifExpr = dynamic_cast<const IIFExpression*>(expr)) {
        // If either branch returns string, IIF returns string
        return isStringExpression(iifExpr->trueValue.get()) || 
               isStringExpression(iifExpr->falseValue.get());
    }
    
    return false;
}

// =============================================================================
// Timer Statement Generation
// =============================================================================

void IRGenerator::generateAfter(const AfterStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // If this is an inline handler (using DO...DONE syntax), generate the function definition first
    if (stmt->isInlineHandler) {
        emit(IROpcode::DEFINE_SUB, stmt->handlerName);
        emit(IROpcode::PUSH_INT, 0);  // No parameters
        
        // Generate the body statements
        for (const auto& bodyStmt : stmt->inlineBody) {
            generateStatement(bodyStmt.get(), lineNumber);
        }
        
        emit(IROpcode::END_SUB);
    }

    // Generate duration expression (push to stack)
    generateExpression(stmt->duration.get());
    
    // Convert to milliseconds or use frames based on unit
    if (stmt->unit == TimeUnit::SECONDS) {
        // Multiply by 1000 to convert seconds to milliseconds
        emit(IROpcode::PUSH_INT, 1000);
        emit(IROpcode::MUL);
        emit(IROpcode::AFTER_TIMER, stmt->handlerName);
    } else if (stmt->unit == TimeUnit::FRAMES) {
        // Use frame-based timer
        emit(IROpcode::AFTER_FRAMES, stmt->handlerName);
    } else {
        // Milliseconds - direct timer
        emit(IROpcode::AFTER_TIMER, stmt->handlerName);
    }
}

void IRGenerator::generateEvery(const EveryStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // If this is an inline handler (using DO...DONE syntax), generate the function definition first
    if (stmt->isInlineHandler) {
        emit(IROpcode::DEFINE_SUB, stmt->handlerName);
        emit(IROpcode::PUSH_INT, 0);  // No parameters
        
        // Generate the body statements
        for (const auto& bodyStmt : stmt->inlineBody) {
            generateStatement(bodyStmt.get(), lineNumber);
        }
        
        emit(IROpcode::END_SUB);
    }

    // Generate duration expression (push to stack)
    generateExpression(stmt->duration.get());
    
    // Convert to milliseconds or use frames based on unit
    if (stmt->unit == TimeUnit::SECONDS) {
        // Multiply by 1000 to convert seconds to milliseconds
        emit(IROpcode::PUSH_INT, 1000);
        emit(IROpcode::MUL);
        emit(IROpcode::EVERY_TIMER, stmt->handlerName);
    } else if (stmt->unit == TimeUnit::FRAMES) {
        // Use frame-based timer
        emit(IROpcode::EVERY_FRAMES, stmt->handlerName);
    } else {
        // Milliseconds - direct timer
        emit(IROpcode::EVERY_TIMER, stmt->handlerName);
    }
}

void IRGenerator::generateAfterFrames(const AfterFramesStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate frame count expression (push to stack)
    generateExpression(stmt->frameCount.get());

    // Emit AFTER_FRAMES instruction with handler name as operand
    emit(IROpcode::AFTER_FRAMES, stmt->handlerName);
}

void IRGenerator::generateEveryFrame(const EveryFrameStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate frame count expression (push to stack)
    generateExpression(stmt->frameCount.get());

    // Emit EVERY_FRAMES instruction with handler name as operand
    emit(IROpcode::EVERY_FRAMES, stmt->handlerName);
}

void IRGenerator::generateRun(const RunStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    if (stmt->untilCondition) {
        // RUN UNTIL condition - generate as a loop
        // WHILE NOT condition
        //   WAIT_FRAMES 1
        // WEND
        
        int loopLabel = allocateLabel();
        int endLabel = allocateLabel();
        
        // Loop start label
        emit(IROpcode::LABEL, loopLabel);
        
        // Evaluate condition
        generateExpression(stmt->untilCondition.get());
        
        // If condition is true, exit loop
        emit(IROpcode::JUMP_IF_TRUE, endLabel);
        
        // Call WAIT_FRAMES(1) as a function
        emit(IROpcode::PUSH_INT, 1);
        emit(IROpcode::CALL_BUILTIN, "WAIT_FRAMES", 1);
        
        // Jump back to start
        emit(IROpcode::JUMP, loopLabel);
        
        // End label
        emit(IROpcode::LABEL, endLabel);
    } else {
        // RUN without condition - wait indefinitely (max int frames)
        emit(IROpcode::PUSH_INT, 2147483647);  // Max int32
        emit(IROpcode::CALL_BUILTIN, "WAIT_FRAMES", 1);
    }
}

void IRGenerator::generateTimerStop(const TimerStopStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    if (stmt->targetType == TimerStopStatement::StopTarget::ALL) {
        // Stop all timers
        emit(IROpcode::TIMER_STOP, std::string("ALL"));
    } else if (stmt->targetType == TimerStopStatement::StopTarget::HANDLER) {
        // Stop by handler name
        emit(IROpcode::TIMER_STOP, stmt->handlerName);
    } else if (stmt->targetType == TimerStopStatement::StopTarget::TIMER_ID) {
        // Generate timer ID expression (push to stack)
        generateExpression(stmt->timerId.get());
        // Emit TIMER_STOP with empty operand (ID comes from stack)
        emit(IROpcode::TIMER_STOP);
    }
}

void IRGenerator::generateTimerInterval(const TimerIntervalStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);

    // Generate interval expression (push to stack)
    generateExpression(stmt->interval.get());

    // Emit TIMER_INTERVAL instruction
    emit(IROpcode::TIMER_INTERVAL);
}

// =============================================================================
// Type Declaration Statement
// =============================================================================

void IRGenerator::generateTypeDeclaration(const TypeDeclarationStatement* stmt, int lineNumber) {
    setSourceContext(lineNumber, m_currentBlockId);
    
    // Emit DEFINE_TYPE instruction with type name
    // This will be used by the Lua codegen to create a constructor function
    emit(IROpcode::DEFINE_TYPE, stmt->typeName);
    
    // Note: The actual constructor generation happens in Lua codegen phase
    // The IR just marks where type definitions occur in the program flow
    // Type metadata is stored in the SymbolTable and accessed during codegen
}

// =============================================================================
// SIMD Helper: Try to emit SIMD IR opcodes for whole-array operations
// =============================================================================

bool IRGenerator::tryEmitSIMDArrayOperation(const LetStatement* stmt,
                                             const ArraySymbol& lhsArray,
                                             const TypeSymbol& lhsType) {
    // This method analyzes the RHS expression and emits SIMD IR opcodes if possible
    // Returns true if SIMD operation was emitted, false to fall back to standard codegen
    
    Expression* rhsExpr = stmt->value.get();
    if (!rhsExpr) return false;
    
    // Check what type of expression we have on the RHS
    ASTNodeType rhsNodeType = rhsExpr->getType();
    
    // Binary operations: A() = B() + C(), A() = B() - C(), A() = B() * scalar
    if (rhsNodeType == ASTNodeType::EXPR_BINARY) {
        BinaryExpression* binExpr = static_cast<BinaryExpression*>(rhsExpr);
        bool isAdd = (binExpr->op == TokenType::PLUS);
        bool isSub = (binExpr->op == TokenType::MINUS);
        bool isMul = (binExpr->op == TokenType::MULTIPLY);
        
        if (isAdd || isSub || isMul) {
            // Check left operand: should be another array of same type
            if (binExpr->left->getType() == ASTNodeType::EXPR_ARRAY_ACCESS) {
                auto* leftArray = static_cast<ArrayAccessExpression*>(binExpr->left.get());
                
                // Check if it's whole-array syntax (no indices)
                if (leftArray->indices.empty()) {
                    // Check if right operand is array or scalar
                    if (binExpr->right->getType() == ASTNodeType::EXPR_ARRAY_ACCESS) {
                        // Binary array operation: A() = B() + C()
                        auto* rightArray = static_cast<ArrayAccessExpression*>(binExpr->right.get());
                        
                        if (rightArray->indices.empty()) {
                            // Both operands are whole arrays
                            // Determine which SIMD opcode to emit based on type and operation
                            IROpcode simdOp;
                            
                            if (lhsType.simdType == TypeDeclarationStatement::SIMDType::PAIR) {
                                if (isAdd) simdOp = IROpcode::SIMD_PAIR_ARRAY_ADD;
                                else if (isSub) simdOp = IROpcode::SIMD_PAIR_ARRAY_SUB;
                                else return false; // mul not supported for arrays yet
                            } else { // QUAD
                                if (isAdd) simdOp = IROpcode::SIMD_QUAD_ARRAY_ADD;
                                else if (isSub) simdOp = IROpcode::SIMD_QUAD_ARRAY_SUB;
                                else return false;
                            }
                            
                            // Emit SIMD instruction with array names as operands
                            IRInstruction instr;
                            instr.opcode = simdOp;
                            instr.operand1 = stmt->variable; // result array
                            instr.operand2 = leftArray->name; // source array A
                            instr.operand3 = rightArray->name; // source array B
                            instr.sourceLineNumber = m_currentLineNumber;
                            instr.blockId = m_currentBlockId;
                            m_code->instructions.push_back(instr);
                            
                            return true; // SIMD operation emitted
                        }
                    } else if (isMul) {
                        // Scalar multiplication: A() = B() * scalar
                        // Generate scalar expression first (it will be on stack)
                        generateExpression(binExpr->right.get());
                        
                        IROpcode simdOp = (lhsType.simdType == TypeDeclarationStatement::SIMDType::PAIR)
                            ? IROpcode::SIMD_PAIR_ARRAY_SCALE
                            : IROpcode::SIMD_QUAD_ARRAY_SCALE;
                        
                        IRInstruction instr;
                        instr.opcode = simdOp;
                        instr.operand1 = stmt->variable; // result array
                        instr.operand2 = leftArray->name; // source array
                        instr.operand3 = ""; // scalar comes from stack
                        instr.sourceLineNumber = m_currentLineNumber;
                        instr.blockId = m_currentBlockId;
                        m_code->instructions.push_back(instr);
                        
                        return true;
                    } else if (isAdd || isSub) {
                        // Scalar add/sub: A() = B() + scalar or A() = B() - scalar
                        generateExpression(binExpr->right.get());
                        
                        IROpcode simdOp;
                        if (lhsType.simdType == TypeDeclarationStatement::SIMDType::PAIR) {
                            simdOp = isAdd ? IROpcode::SIMD_PAIR_ARRAY_ADD_SCALAR
                                           : IROpcode::SIMD_PAIR_ARRAY_SUB_SCALAR;
                        } else {
                            simdOp = isAdd ? IROpcode::SIMD_QUAD_ARRAY_ADD_SCALAR
                                           : IROpcode::SIMD_QUAD_ARRAY_SUB_SCALAR;
                        }
                        
                        IRInstruction instr;
                        instr.opcode = simdOp;
                        instr.operand1 = stmt->variable;
                        instr.operand2 = leftArray->name;
                        instr.operand3 = "";
                        instr.sourceLineNumber = m_currentLineNumber;
                        instr.blockId = m_currentBlockId;
                        m_code->instructions.push_back(instr);
                        
                        return true;
                    }
                }
            }
        }
    } else if (rhsNodeType == ASTNodeType::EXPR_ARRAY_ACCESS) {
        // Simple copy: A() = B()
        auto* rhsArray = static_cast<ArrayAccessExpression*>(rhsExpr);
        if (rhsArray->indices.empty()) {
            // For now, we don't have a dedicated SIMD_COPY opcode
            // Fall back to standard codegen which will do element-wise copy
            return false;
        }
    }
    
    // Not a SIMD-compatible pattern
    return false;
}

// =============================================================================
// Array Helper: Try to emit element-wise array operation IR opcodes
// =============================================================================

bool IRGenerator::tryEmitArrayOperation(const LetStatement* stmt,
                                        const ArraySymbol& lhsArray) {
    // This method analyzes the RHS expression for regular (non-SIMD) arrays
    // and emits element-wise operation opcodes if possible
    // Returns true if array operation was emitted, false to fall back to scalar fill
    
    Expression* rhsExpr = stmt->value.get();
    if (!rhsExpr) return false;
    
    // Check what type of expression we have on the RHS
    ASTNodeType rhsNodeType = rhsExpr->getType();
    
    // Binary operations: A() = B() + C(), A() = B() - C(), etc.
    if (rhsNodeType == ASTNodeType::EXPR_BINARY) {
        BinaryExpression* binExpr = static_cast<BinaryExpression*>(rhsExpr);
        bool isAdd = (binExpr->op == TokenType::PLUS);
        bool isSub = (binExpr->op == TokenType::MINUS);
        bool isMul = (binExpr->op == TokenType::MULTIPLY);
        bool isDiv = (binExpr->op == TokenType::DIVIDE);
        
        if (isAdd || isSub || isMul || isDiv) {
            // Check left operand: should be another array
            if (binExpr->left->getType() == ASTNodeType::EXPR_ARRAY_ACCESS) {
                auto* leftArray = static_cast<ArrayAccessExpression*>(binExpr->left.get());
                
                // Check if it's whole-array syntax (no indices)
                if (leftArray->indices.empty()) {
                    // Check if right operand is array or scalar
                    if (binExpr->right->getType() == ASTNodeType::EXPR_ARRAY_ACCESS) {
                        // Binary array operation: A() = B() + C()
                        auto* rightArray = static_cast<ArrayAccessExpression*>(binExpr->right.get());
                        
                        if (rightArray->indices.empty()) {
                            // Both operands are whole arrays
                            // Determine which opcode to emit based on operation
                            IROpcode arrayOp;
                            
                            if (isAdd) arrayOp = IROpcode::ARRAY_ADD;
                            else if (isSub) arrayOp = IROpcode::ARRAY_SUB;
                            else if (isMul) arrayOp = IROpcode::ARRAY_MUL;
                            else if (isDiv) arrayOp = IROpcode::ARRAY_DIV;
                            else return false;
                            
                            // Emit array operation instruction with array names as operands
                            IRInstruction instr;
                            instr.opcode = arrayOp;
                            instr.operand1 = stmt->variable; // result array
                            instr.operand2 = leftArray->name; // source array A
                            instr.operand3 = rightArray->name; // source array B
                            instr.sourceLineNumber = m_currentLineNumber;
                            instr.blockId = m_currentBlockId;
                            m_code->instructions.push_back(instr);
                            
                            return true; // Array operation emitted
                        }
                    } else {
                        // Scalar operation: A() = B() + scalar
                        // Generate scalar expression first (it will be on stack)
                        generateExpression(binExpr->right.get());
                        
                        IROpcode arrayOp;
                        if (isAdd) arrayOp = IROpcode::ARRAY_ADD_SCALAR;
                        else if (isSub) arrayOp = IROpcode::ARRAY_SUB_SCALAR;
                        else if (isMul) arrayOp = IROpcode::ARRAY_MUL_SCALAR;
                        else if (isDiv) arrayOp = IROpcode::ARRAY_DIV_SCALAR;
                        else return false;
                        
                        IRInstruction instr;
                        instr.opcode = arrayOp;
                        instr.operand1 = stmt->variable; // result array
                        instr.operand2 = leftArray->name; // source array
                        instr.operand3 = ""; // scalar comes from stack
                        instr.sourceLineNumber = m_currentLineNumber;
                        instr.blockId = m_currentBlockId;
                        m_code->instructions.push_back(instr);
                        
                        return true;
                    }
                }
            }
        }
    } else if (rhsNodeType == ASTNodeType::EXPR_ARRAY_ACCESS) {
        // Simple copy: A() = B()
        // We can use ARRAY_ADD with operand3 empty to indicate copy
        // Or just fall through to let standard codegen handle it
        // For now, fall through
        return false;
    }
    
    // Not an array operation pattern
    return false;
}

} // namespace FasterBASIC
