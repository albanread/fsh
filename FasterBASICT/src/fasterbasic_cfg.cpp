//
// fasterbasic_cfg.cpp
// FasterBASIC - Control Flow Graph Builder Implementation
//
// Implements CFG construction from validated AST.
// Converts the tree structure into basic blocks connected by edges.
// This is Phase 4 of the compilation pipeline.
//

#include "fasterbasic_cfg.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <functional>

namespace FasterBASIC {

// =============================================================================
// Constructor/Destructor
// =============================================================================

CFGBuilder::CFGBuilder()
    : m_program(nullptr)
    , m_symbols(nullptr)
    , m_currentBlock(nullptr)
    , m_nextBlockId(0)
    , m_createExitBlock(true)
    , m_mergeBlocks(false)
    , m_blocksCreated(0)
    , m_edgesCreated(0)
{
}

CFGBuilder::~CFGBuilder() = default;

// =============================================================================
// Main Build Entry Point
// =============================================================================

std::unique_ptr<ControlFlowGraph> CFGBuilder::build(const Program& program,
                                                     const SymbolTable& symbols) {
    m_program = &program;
    m_symbols = &symbols;
    m_cfg = std::make_unique<ControlFlowGraph>();
    m_blocksCreated = 0;
    m_edgesCreated = 0;
    m_loopStack.clear();
    
    // Phase 0: Pre-scan to collect jump targets
    std::set<int> jumpTargets = collectJumpTargets(program);
    
    // Phase 1: Build basic blocks
    buildBlocks(program, jumpTargets);
    
    // Phase 2: Build control flow edges
    buildEdges();
    
    // Phase 3: Identify loop structures
    identifyLoops();
    
    // Phase 4: Identify subroutines
    identifySubroutines();
    
    // Phase 5: Optimize CFG (optional)
    if (m_mergeBlocks) {
        optimizeCFG();
    }
    
    return std::move(m_cfg);
}

// =============================================================================
// Phase 0: Pre-scan to collect jump targets
// =============================================================================

// Helper function to recursively collect jump targets from statements
void collectJumpTargetsFromStatements(const std::vector<std::unique_ptr<Statement>>& statements, 
                                      std::set<int>& targets) {
    for (const auto& stmt : statements) {
        ASTNodeType type = stmt->getType();
        
        switch (type) {
            case ASTNodeType::STMT_GOTO: {
                const auto& gotoStmt = static_cast<const GotoStatement&>(*stmt);
                targets.insert(gotoStmt.lineNumber);
                break;
            }
            
            case ASTNodeType::STMT_GOSUB: {
                const auto& gosubStmt = static_cast<const GosubStatement&>(*stmt);
                targets.insert(gosubStmt.lineNumber);
                break;
            }
            
            case ASTNodeType::STMT_ON_EVENT: {
                const auto& onEventStmt = static_cast<const OnEventStatement&>(*stmt);
                std::cout << "DEBUG: Found ON_EVENT statement, event=" << onEventStmt.eventName 
                          << ", handlerType=" << static_cast<int>(onEventStmt.handlerType)
                          << ", target=" << onEventStmt.target 
                          << ", isLineNumber=" << onEventStmt.isLineNumber << std::endl;
                if ((onEventStmt.handlerType == EventHandlerType::GOSUB || 
                     onEventStmt.handlerType == EventHandlerType::GOTO) && 
                    onEventStmt.isLineNumber) {
                    int lineNum = std::stoi(onEventStmt.target);
                    std::cout << "DEBUG: Adding event GOSUB/GOTO target: " << lineNum << std::endl;
                    targets.insert(lineNum);
                }
                break;
            }
            
            case ASTNodeType::STMT_IF: {
                const auto& ifStmt = static_cast<const IfStatement&>(*stmt);
                if (ifStmt.hasGoto) {
                    targets.insert(ifStmt.gotoLine);
                }
                // Recursively scan THEN and ELSE blocks
                collectJumpTargetsFromStatements(ifStmt.thenStatements, targets);
                collectJumpTargetsFromStatements(ifStmt.elseStatements, targets);
                break;
            }
            
            default:
                break;
        }
    }
}

std::set<int> CFGBuilder::collectJumpTargets(const Program& program) {
    std::set<int> targets;
    
    for (const auto& line : program.lines) {
        collectJumpTargetsFromStatements(line->statements, targets);
    }
    
    return targets;
}

// =============================================================================
// Phase 1: Build Basic Blocks
// =============================================================================

void CFGBuilder::buildBlocks(const Program& program, const std::set<int>& jumpTargets) {
    // Create entry block
    BasicBlock* entryBlock = createNewBlock("Entry");
    m_cfg->entryBlock = entryBlock->id;
    m_currentBlock = entryBlock;
    
    // Process each program line
    for (const auto& line : program.lines) {
        int lineNumber = line->lineNumber;
        
        // If this line is a jump target, start a new block
        if (lineNumber > 0 && jumpTargets.count(lineNumber) > 0) {
            // Only create new block if current block is not empty
            if (!m_currentBlock->statements.empty() || !m_currentBlock->lineNumbers.empty()) {
                BasicBlock* targetBlock = createNewBlock("Target_" + std::to_string(lineNumber));
                // Add fallthrough edge from previous block if it doesn't end with a jump
                if (!m_currentBlock->statements.empty()) {
                    const Statement* lastStmt = m_currentBlock->statements.back();
                    ASTNodeType lastType = lastStmt->getType();
                    if (lastType != ASTNodeType::STMT_GOTO && 
                        lastType != ASTNodeType::STMT_END &&
                        lastType != ASTNodeType::STMT_RETURN &&
                        lastType != ASTNodeType::STMT_EXIT) {
                        // Fallthrough will be added in buildEdges phase
                    }
                }
                m_currentBlock = targetBlock;
            }
        }
        
        // Map line number to current block
        if (lineNumber > 0) {
            m_cfg->mapLineToBlock(lineNumber, m_currentBlock->id);
            m_currentBlock->addLineNumber(lineNumber);
        }
        
        // Process each statement in the line
        for (const auto& stmt : line->statements) {
            processStatement(*stmt, m_currentBlock, lineNumber);
        }
    }
    
    // Create exit block if requested
    if (m_createExitBlock) {
        BasicBlock* exitBlock = createNewBlock("Exit");
        exitBlock->isTerminator = true;
        m_cfg->exitBlock = exitBlock->id;
        
        // Connect last block to exit
        if (m_currentBlock && m_currentBlock->id != exitBlock->id) {
            addFallthroughEdge(m_currentBlock->id, exitBlock->id);
        }
    }
}

// =============================================================================
// Statement Processing
// =============================================================================

void CFGBuilder::processStatement(const Statement& stmt, BasicBlock* currentBlock, int lineNumber) {
    // Add statement to current block with its line number
    currentBlock->addStatement(&stmt, lineNumber);
    
    // Handle control flow statements
    ASTNodeType type = stmt.getType();
    
    switch (type) {
        case ASTNodeType::STMT_GOTO:
            processGotoStatement(static_cast<const GotoStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_GOSUB:
            processGosubStatement(static_cast<const GosubStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_IF:
            processIfStatement(static_cast<const IfStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_FOR:
            processForStatement(static_cast<const ForStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_FOR_IN:
            processForInStatement(static_cast<const ForInStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_WHILE:
            processWhileStatement(static_cast<const WhileStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_REPEAT:
            processRepeatStatement(static_cast<const RepeatStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_DO:
            processDoStatement(static_cast<const DoStatement&>(stmt), currentBlock);
            break;
            
        case ASTNodeType::STMT_NEXT:
        case ASTNodeType::STMT_WEND:
        case ASTNodeType::STMT_UNTIL:
        case ASTNodeType::STMT_LOOP:
            // Loop back edges handled in buildEdges phase
            break;
            
        case ASTNodeType::STMT_RETURN:
        case ASTNodeType::STMT_END:
        case ASTNodeType::STMT_EXIT:
            currentBlock->isTerminator = true;
            break;
            
        default:
            // Regular statements don't affect control flow
            break;
    }
}

void CFGBuilder::processGotoStatement(const GotoStatement& stmt, BasicBlock* currentBlock) {
    // GOTO creates unconditional jump - start new block after this
    BasicBlock* nextBlock = createNewBlock();
    m_currentBlock = nextBlock;
    
    // Edge will be added in buildEdges phase when we know target block IDs
}

void CFGBuilder::processGosubStatement(const GosubStatement& stmt, BasicBlock* currentBlock) {
    // GOSUB is like a call - execution continues after it
    // Edge will be added in buildEdges phase
}

void CFGBuilder::processIfStatement(const IfStatement& stmt, BasicBlock* currentBlock) {
    // IF creates conditional branch
    
    if (stmt.hasGoto) {
        // IF ... THEN GOTO creates two-way branch
        BasicBlock* nextBlock = createNewBlock();
        m_currentBlock = nextBlock;
    } else if (!stmt.thenStatements.empty() || !stmt.elseIfClauses.empty() || !stmt.elseStatements.empty()) {
        // IF with structured statements - handled by structured IR opcodes
        // Don't create separate blocks; the IR generator will emit structured IF opcodes
        // Just continue with the current block
    }
}

void CFGBuilder::processForStatement(const ForStatement& stmt, BasicBlock* currentBlock) {
    // FOR creates loop header
    BasicBlock* loopHeader = createNewBlock("FOR Loop Header");
    loopHeader->isLoopHeader = true;
    
    BasicBlock* loopBody = createNewBlock("FOR Loop Body");
    BasicBlock* loopExit = createNewBlock("After FOR");
    loopExit->isLoopExit = true;
    
    // Track loop context
    LoopContext ctx;
    ctx.headerBlock = loopHeader->id;
    ctx.exitBlock = loopExit->id;
    ctx.variable = stmt.variable;
    m_loopStack.push_back(ctx);
    
    // Remember this FOR loop
    m_cfg->forLoopHeaders[loopHeader->id] = loopHeader->id;
    
    m_currentBlock = loopBody;
}

void CFGBuilder::processForInStatement(const ForInStatement& stmt, BasicBlock* currentBlock) {
    // FOR...IN creates loop header similar to FOR
    BasicBlock* loopHeader = createNewBlock("FOR...IN Loop Header");
    loopHeader->isLoopHeader = true;
    
    BasicBlock* loopBody = createNewBlock("FOR...IN Loop Body");
    BasicBlock* loopExit = createNewBlock("After FOR...IN");
    loopExit->isLoopExit = true;
    
    // Track loop context
    LoopContext ctx;
    ctx.headerBlock = loopHeader->id;
    ctx.exitBlock = loopExit->id;
    ctx.variable = stmt.variable;
    m_loopStack.push_back(ctx);
    
    // Remember this FOR...IN loop
    m_cfg->forLoopHeaders[loopHeader->id] = loopHeader->id;
    
    m_currentBlock = loopBody;
}

void CFGBuilder::processWhileStatement(const WhileStatement& stmt, BasicBlock* currentBlock) {
    // WHILE creates loop header with condition
    BasicBlock* loopHeader = createNewBlock("WHILE Loop Header");
    loopHeader->isLoopHeader = true;
    
    BasicBlock* loopBody = createNewBlock("WHILE Loop Body");
    BasicBlock* loopExit = createNewBlock("After WHILE");
    loopExit->isLoopExit = true;
    
    // Track loop context
    LoopContext ctx;
    ctx.headerBlock = loopHeader->id;
    ctx.exitBlock = loopExit->id;
    m_loopStack.push_back(ctx);
    
    m_cfg->whileLoopHeaders[loopHeader->id] = loopHeader->id;
    
    m_currentBlock = loopBody;
}

void CFGBuilder::processRepeatStatement(const RepeatStatement& stmt, BasicBlock* currentBlock) {
    // REPEAT creates loop body
    BasicBlock* loopBody = createNewBlock("REPEAT Loop Body");
    loopBody->isLoopHeader = true;
    
    BasicBlock* loopExit = createNewBlock("After REPEAT");
    loopExit->isLoopExit = true;
    
    // Track loop context
    LoopContext ctx;
    ctx.headerBlock = loopBody->id;
    ctx.exitBlock = loopExit->id;
    m_loopStack.push_back(ctx);
    
    m_cfg->repeatLoopHeaders[loopBody->id] = loopBody->id;
    
    m_currentBlock = loopBody;
}

void CFGBuilder::processDoStatement(const DoStatement& stmt, BasicBlock* currentBlock) {
    // DO creates loop structure - behavior depends on condition type
    BasicBlock* loopHeader = createNewBlock("DO Loop Header");
    loopHeader->isLoopHeader = true;
    
    BasicBlock* loopBody = createNewBlock("DO Loop Body");
    BasicBlock* loopExit = createNewBlock("After DO");
    loopExit->isLoopExit = true;
    
    // Track loop context
    LoopContext ctx;
    ctx.headerBlock = loopHeader->id;
    ctx.exitBlock = loopExit->id;
    m_loopStack.push_back(ctx);
    
    m_cfg->doLoopHeaders[loopHeader->id] = loopHeader->id;
    
    m_currentBlock = loopBody;
}

// =============================================================================
// Phase 2: Build Control Flow Edges
// =============================================================================

void CFGBuilder::buildEdges() {
    // Walk through blocks and create edges based on statements
    for (const auto& block : m_cfg->blocks) {
        if (block->statements.empty()) {
            // Empty block - fallthrough to next
            if (block->id + 1 < static_cast<int>(m_cfg->blocks.size())) {
                addFallthroughEdge(block->id, block->id + 1);
            }
            continue;
        }
        
        // Check last statement in block for control flow
        const Statement* lastStmt = block->statements.back();
        ASTNodeType type = lastStmt->getType();
        
        switch (type) {
            case ASTNodeType::STMT_GOTO: {
                // Unconditional jump to target line (or next available line)
                const auto& gotoStmt = static_cast<const GotoStatement&>(*lastStmt);
                int targetBlock = m_cfg->getBlockForLineOrNext(gotoStmt.lineNumber);
                if (targetBlock >= 0) {
                    addUnconditionalEdge(block->id, targetBlock);
                }
                break;
            }
            
            case ASTNodeType::STMT_GOSUB: {
                // Call to subroutine (or next available line), then continue
                const auto& gosubStmt = static_cast<const GosubStatement&>(*lastStmt);
                int targetBlock = m_cfg->getBlockForLineOrNext(gosubStmt.lineNumber);
                if (targetBlock >= 0) {
                    addCallEdge(block->id, targetBlock);
                }
                // Also continue to next block
                if (block->id + 1 < static_cast<int>(m_cfg->blocks.size())) {
                    addFallthroughEdge(block->id, block->id + 1);
                }
                break;
            }
            
            case ASTNodeType::STMT_IF: {
                // Conditional branch
                const auto& ifStmt = static_cast<const IfStatement&>(*lastStmt);
                if (ifStmt.hasGoto) {
                    // Branch to line (or next available line) or continue
                    int targetBlock = m_cfg->getBlockForLineOrNext(ifStmt.gotoLine);
                    if (targetBlock >= 0) {
                        addConditionalEdge(block->id, targetBlock, "true");
                    }
                    if (block->id + 1 < static_cast<int>(m_cfg->blocks.size())) {
                        addConditionalEdge(block->id, block->id + 1, "false");
                    }
                }
                break;
            }
            
            case ASTNodeType::STMT_RETURN:
            case ASTNodeType::STMT_END:
            case ASTNodeType::STMT_EXIT:
                // Terminators - no outgoing edges (or return edge)
                if (m_cfg->exitBlock >= 0) {
                    addReturnEdge(block->id, m_cfg->exitBlock);
                }
                break;
                
            default:
                // Regular statement - fallthrough to next block
                if (block->id + 1 < static_cast<int>(m_cfg->blocks.size())) {
                    addFallthroughEdge(block->id, block->id + 1);
                }
                break;
        }
    }
}

// =============================================================================
// Phase 3: Identify Loop Structures
// =============================================================================

void CFGBuilder::identifyLoops() {
    // Implement back-edge detection to identify GOTO-based loops
    // A back edge is an edge from block A to block B where B dominates A
    // or in simpler terms, B appears earlier in program order
    
    // For each edge, check if it's a back edge (target block has lower ID than source)
    for (const auto& edge : m_cfg->edges) {
        if (edge.type == EdgeType::UNCONDITIONAL && 
            edge.targetBlock < edge.sourceBlock) {
            // This is likely a back edge (GOTO to earlier line)
            BasicBlock* targetBlock = m_cfg->getBlock(edge.targetBlock);
            BasicBlock* sourceBlock = m_cfg->getBlock(edge.sourceBlock);
            
            if (targetBlock && sourceBlock) {
                // Mark the target as a loop header
                targetBlock->isLoopHeader = true;
                
                // Mark blocks in the loop body between target and source
                for (int blockId = edge.targetBlock; blockId <= edge.sourceBlock; blockId++) {
                    BasicBlock* loopBlock = m_cfg->getBlock(blockId);
                    if (loopBlock) {
                        // This block is part of a potential loop
                        // We'll use this information during code generation
                    }
                }
            }
        }
    }
    
    // Also detect cycles using simple DFS
    std::set<int> visited;
    std::set<int> recursionStack;
    
    std::function<void(int)> detectCycles = [&](int blockId) {
        if (recursionStack.count(blockId)) {
            // Found a cycle - mark the target block as a loop header
            BasicBlock* loopHeader = m_cfg->getBlock(blockId);
            if (loopHeader) {
                loopHeader->isLoopHeader = true;
            }
            return;
        }
        
        if (visited.count(blockId)) {
            return;
        }
        
        visited.insert(blockId);
        recursionStack.insert(blockId);
        
        BasicBlock* block = m_cfg->getBlock(blockId);
        if (block) {
            for (int successor : block->successors) {
                detectCycles(successor);
            }
        }
        
        recursionStack.erase(blockId);
    };
    
    // Start cycle detection from entry block
    if (m_cfg->entryBlock >= 0) {
        detectCycles(m_cfg->entryBlock);
    }
}

// =============================================================================
// Phase 4: Identify Subroutines
// =============================================================================

void CFGBuilder::identifySubroutines() {
    // Mark blocks that are GOSUB targets as subroutines
    for (const auto& edge : m_cfg->edges) {
        if (edge.type == EdgeType::CALL) {
            BasicBlock* target = m_cfg->getBlock(edge.targetBlock);
            if (target) {
                target->isSubroutine = true;
            }
        }
    }
}

// =============================================================================
// Phase 5: Optimize CFG
// =============================================================================

void CFGBuilder::optimizeCFG() {
    // Potential optimizations:
    // - Merge sequential blocks with single predecessor/successor
    // - Remove empty blocks
    // - Simplify edges
    // Not implemented yet
}

// =============================================================================
// Block Management
// =============================================================================

BasicBlock* CFGBuilder::createNewBlock(const std::string& label) {
    BasicBlock* block = m_cfg->createBlock(label);
    m_blocksCreated++;
    return block;
}

void CFGBuilder::finalizeBlock(BasicBlock* block) {
    // Any finalization needed for a block
}

// =============================================================================
// Edge Creation Helpers
// =============================================================================

void CFGBuilder::addFallthroughEdge(int source, int target) {
    m_cfg->addEdge(source, target, EdgeType::FALLTHROUGH);
    m_edgesCreated++;
}

void CFGBuilder::addConditionalEdge(int source, int target, const std::string& condition) {
    m_cfg->addEdge(source, target, EdgeType::CONDITIONAL, condition);
    m_edgesCreated++;
}

void CFGBuilder::addUnconditionalEdge(int source, int target) {
    m_cfg->addEdge(source, target, EdgeType::UNCONDITIONAL);
    m_edgesCreated++;
}

void CFGBuilder::addCallEdge(int source, int target) {
    m_cfg->addEdge(source, target, EdgeType::CALL);
    m_edgesCreated++;
}

void CFGBuilder::addReturnEdge(int source, int target) {
    m_cfg->addEdge(source, target, EdgeType::RETURN);
    m_edgesCreated++;
}

// =============================================================================
// Report Generation
// =============================================================================

std::string CFGBuilder::generateReport(const ControlFlowGraph& cfg) const {
    std::ostringstream oss;
    
    oss << "=== CFG BUILDER REPORT ===\n\n";
    
    // Build statistics
    oss << "Build Statistics:\n";
    oss << "  Blocks Created: " << m_blocksCreated << "\n";
    oss << "  Edges Created: " << m_edgesCreated << "\n";
    oss << "  Loop Headers: " << cfg.getLoopCount() << "\n";
    oss << "\n";
    
    // CFG summary
    oss << "CFG Summary:\n";
    oss << "  Total Blocks: " << cfg.getBlockCount() << "\n";
    oss << "  Total Edges: " << cfg.getEdgeCount() << "\n";
    oss << "  Entry Block: " << cfg.entryBlock << "\n";
    oss << "  Exit Block: " << cfg.exitBlock << "\n";
    oss << "\n";
    
    // Block types
    int loopHeaders = 0;
    int loopExits = 0;
    int subroutines = 0;
    int terminators = 0;
    
    for (const auto& block : cfg.blocks) {
        if (block->isLoopHeader) loopHeaders++;
        if (block->isLoopExit) loopExits++;
        if (block->isSubroutine) subroutines++;
        if (block->isTerminator) terminators++;
    }
    
    oss << "Block Analysis:\n";
    oss << "  Loop Headers: " << loopHeaders << "\n";
    oss << "  Loop Exits: " << loopExits << "\n";
    oss << "  Subroutines: " << subroutines << "\n";
    oss << "  Terminators: " << terminators << "\n";
    oss << "\n";
    
    // Edge types
    int fallthroughEdges = 0;
    int conditionalEdges = 0;
    int unconditionalEdges = 0;
    int callEdges = 0;
    int returnEdges = 0;
    
    for (const auto& edge : cfg.edges) {
        switch (edge.type) {
            case EdgeType::FALLTHROUGH: fallthroughEdges++; break;
            case EdgeType::CONDITIONAL: conditionalEdges++; break;
            case EdgeType::UNCONDITIONAL: unconditionalEdges++; break;
            case EdgeType::CALL: callEdges++; break;
            case EdgeType::RETURN: returnEdges++; break;
        }
    }
    
    oss << "Edge Analysis:\n";
    oss << "  Fallthrough: " << fallthroughEdges << "\n";
    oss << "  Conditional: " << conditionalEdges << "\n";
    oss << "  Unconditional: " << unconditionalEdges << "\n";
    oss << "  Call: " << callEdges << "\n";
    oss << "  Return: " << returnEdges << "\n";
    oss << "\n";
    
    // Full CFG details
    oss << cfg.toString();
    
    oss << "=== END CFG BUILDER REPORT ===\n";
    
    return oss.str();
}

} // namespace FasterBASIC