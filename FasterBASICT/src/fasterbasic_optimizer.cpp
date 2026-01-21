//
// fasterbasic_optimizer.cpp
// FasterBASIC - AST Optimizer Implementation
//
// Implements multi-pass optimization framework for AST.
// Currently all passes are no-ops (placeholders for future optimizations).
// This is Phase 3.5 in the compilation pipeline.
//

#include "fasterbasic_optimizer.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

namespace FasterBASIC {

// =============================================================================
// Helper Functions for Optimization
// =============================================================================

// Forward declarations for expression optimization
static ExpressionPtr optimizeExpression(const ExpressionPtr& expr, OptimizationStats& stats);
static bool isConstantNumber(const Expression* expr, double& value);
static bool isConstantString(const Expression* expr, std::string& value);
static ExpressionPtr makeNumber(double value);
static ExpressionPtr makeString(const std::string& value);

// Check if an expression is a constant number
static bool isConstantNumber(const Expression* expr, double& value) {
    if (expr->getType() == ASTNodeType::EXPR_NUMBER) {
        const NumberExpression* numExpr = static_cast<const NumberExpression*>(expr);
        value = numExpr->value;
        return true;
    }
    return false;
}

// Check if an expression is a constant string
static bool isConstantString(const Expression* expr, std::string& value) {
    if (expr->getType() == ASTNodeType::EXPR_STRING) {
        const StringExpression* strExpr = static_cast<const StringExpression*>(expr);
        value = strExpr->value;
        return true;
    }
    return false;
}

// Create a number expression
static ExpressionPtr makeNumber(double value) {
    return std::make_unique<NumberExpression>(value);
}

// Create a string expression
static ExpressionPtr makeString(const std::string& value) {
    return std::make_unique<StringExpression>(value);
}

// Optimize a binary expression with constant operands
static ExpressionPtr optimizeBinaryExpression(BinaryExpression* binExpr, OptimizationStats& stats) {
    // First, recursively optimize children
    auto optimizedLeft = optimizeExpression(binExpr->left, stats);
    if (optimizedLeft) {
        binExpr->left = std::move(optimizedLeft);
    }
    
    auto optimizedRight = optimizeExpression(binExpr->right, stats);
    if (optimizedRight) {
        binExpr->right = std::move(optimizedRight);
    }
    
    // Safety check: ensure both operands exist after optimization
    if (!binExpr->left || !binExpr->right) {
        return nullptr;
    }
    
    double leftVal, rightVal;
    std::string leftStr, rightStr;
    
    // Try to fold numeric constants
    if (isConstantNumber(binExpr->left.get(), leftVal) && 
        isConstantNumber(binExpr->right.get(), rightVal)) {
        
        double result = 0.0;
        bool canFold = true;
        
        switch (binExpr->op) {
            case TokenType::PLUS:
                result = leftVal + rightVal;
                break;
            case TokenType::MINUS:
                result = leftVal - rightVal;
                break;
            case TokenType::MULTIPLY:
                result = leftVal * rightVal;
                break;
            case TokenType::DIVIDE:
                if (rightVal != 0.0) {
                    result = leftVal / rightVal;
                } else {
                    canFold = false;  // Don't fold division by zero
                }
                break;
            case TokenType::POWER:
                result = std::pow(leftVal, rightVal);
                break;
            case TokenType::MOD:
                if (rightVal != 0.0) {
                    result = std::fmod(leftVal, rightVal);
                } else {
                    canFold = false;
                }
                break;
            // Comparison operators
            case TokenType::EQUAL:
                result = (leftVal == rightVal) ? -1.0 : 0.0;  // BASIC uses -1 for true
                break;
            case TokenType::NOT_EQUAL:
                result = (leftVal != rightVal) ? -1.0 : 0.0;
                break;
            case TokenType::LESS_THAN:
                result = (leftVal < rightVal) ? -1.0 : 0.0;
                break;
            case TokenType::LESS_EQUAL:
                result = (leftVal <= rightVal) ? -1.0 : 0.0;
                break;
            case TokenType::GREATER_THAN:
                result = (leftVal > rightVal) ? -1.0 : 0.0;
                break;
            case TokenType::GREATER_EQUAL:
                result = (leftVal >= rightVal) ? -1.0 : 0.0;
                break;
            // Logical operators (treating numbers as booleans: 0=false, non-zero=true)
            case TokenType::AND:
                result = (leftVal != 0.0 && rightVal != 0.0) ? -1.0 : 0.0;
                break;
            case TokenType::OR:
                result = (leftVal != 0.0 || rightVal != 0.0) ? -1.0 : 0.0;
                break;
            default:
                canFold = false;
        }
        
        if (canFold) {
            stats.constantFolds++;
            stats.totalOptimizations++;
            return makeNumber(result);
        }
    }
    
    // Try to fold string concatenation
    if (binExpr->op == TokenType::PLUS) {
        if (isConstantString(binExpr->left.get(), leftStr) && 
            isConstantString(binExpr->right.get(), rightStr)) {
            stats.constantFolds++;
            stats.totalOptimizations++;
            return makeString(leftStr + rightStr);
        }
    }
    
    // Algebraic simplifications
    if (isConstantNumber(binExpr->right.get(), rightVal)) {
        switch (binExpr->op) {
            case TokenType::PLUS:
                if (rightVal == 0.0) {
                    // X + 0 -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->left);
                }
                break;
            case TokenType::MINUS:
                if (rightVal == 0.0) {
                    // X - 0 -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->left);
                }
                break;
            case TokenType::MULTIPLY:
                if (rightVal == 0.0) {
                    // X * 0 -> 0
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return makeNumber(0.0);
                } else if (rightVal == 1.0) {
                    // X * 1 -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->left);
                } else if (rightVal == 2.0) {
                    // X * 2 -> X + X (strength reduction)
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    auto leftCopy = std::make_unique<BinaryExpression>(
                        std::move(binExpr->left),
                        TokenType::PLUS,
                        nullptr
                    );
                    // We need to clone the left expression, but for now just keep multiplication
                    // (proper cloning would require a deep copy mechanism)
                    return nullptr;  // Skip this optimization for now
                }
                break;
            case TokenType::DIVIDE:
                if (rightVal == 1.0) {
                    // X / 1 -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->left);
                }
                break;
            case TokenType::POWER:
                if (rightVal == 0.0) {
                    // X ^ 0 -> 1
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return makeNumber(1.0);
                } else if (rightVal == 1.0) {
                    // X ^ 1 -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->left);
                }
                break;
        }
    }
    
    // Left side algebraic simplifications
    if (isConstantNumber(binExpr->left.get(), leftVal)) {
        switch (binExpr->op) {
            case TokenType::PLUS:
                if (leftVal == 0.0) {
                    // 0 + X -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->right);
                }
                break;
            case TokenType::MULTIPLY:
                if (leftVal == 0.0) {
                    // 0 * X -> 0
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return makeNumber(0.0);
                } else if (leftVal == 1.0) {
                    // 1 * X -> X
                    stats.strengthReductions++;
                    stats.totalOptimizations++;
                    return std::move(binExpr->right);
                }
                break;
        }
    }
    
    return nullptr;  // No optimization applied
}

// Optimize a unary expression
static ExpressionPtr optimizeUnaryExpression(UnaryExpression* unaryExpr, OptimizationStats& stats) {
    // Recursively optimize child
    auto optimized = optimizeExpression(unaryExpr->expr, stats);
    if (optimized) {
        unaryExpr->expr = std::move(optimized);
    }
    
    double value;
    if (isConstantNumber(unaryExpr->expr.get(), value)) {
        if (unaryExpr->op == TokenType::MINUS) {
            // -constant -> folded constant
            stats.constantFolds++;
            stats.totalOptimizations++;
            return makeNumber(-value);
        } else if (unaryExpr->op == TokenType::PLUS) {
            // +constant -> constant
            stats.constantFolds++;
            stats.totalOptimizations++;
            return makeNumber(value);
        } else if (unaryExpr->op == TokenType::NOT) {
            // NOT constant -> folded constant
            stats.constantFolds++;
            stats.totalOptimizations++;
            return makeNumber(value == 0.0 ? -1.0 : 0.0);
        }
    }
    
    return nullptr;  // No optimization applied
}

// Main expression optimizer
static ExpressionPtr optimizeExpression(const ExpressionPtr& expr, OptimizationStats& stats) {
    if (!expr) return nullptr;
    
    switch (expr->getType()) {
        case ASTNodeType::EXPR_BINARY: {
            BinaryExpression* binExpr = static_cast<BinaryExpression*>(expr.get());
            ExpressionPtr optimized = optimizeBinaryExpression(binExpr, stats);
            if (optimized) return optimized;
            break;
        }
        case ASTNodeType::EXPR_UNARY: {
            UnaryExpression* unaryExpr = static_cast<UnaryExpression*>(expr.get());
            ExpressionPtr optimized = optimizeUnaryExpression(unaryExpr, stats);
            if (optimized) return optimized;
            break;
        }
        default:
            // Other expression types don't need optimization
            break;
    }
    
    return nullptr;  // Return nullptr to indicate no change
}

// Optimize expressions in a statement
static bool optimizeStatement(Statement* stmt, OptimizationStats& stats) {
    bool changed = false;
    
    switch (stmt->getType()) {
        case ASTNodeType::STMT_LET: {
            LetStatement* letStmt = static_cast<LetStatement*>(stmt);
            if (letStmt->value) {
                ExpressionPtr optimized = optimizeExpression(letStmt->value, stats);
                if (optimized) {
                    letStmt->value = std::move(optimized);
                    changed = true;
                }
            }
            // Optimize array indices
            for (auto& idx : letStmt->indices) {
                ExpressionPtr optimized = optimizeExpression(idx, stats);
                if (optimized) {
                    idx = std::move(optimized);
                    changed = true;
                }
            }
            break;
        }
        case ASTNodeType::STMT_PRINT: {
            PrintStatement* printStmt = static_cast<PrintStatement*>(stmt);
            for (auto& item : printStmt->items) {
                if (item.expr) {
                    ExpressionPtr optimized = optimizeExpression(item.expr, stats);
                    if (optimized) {
                        item.expr = std::move(optimized);
                        changed = true;
                    }
                }
            }
            break;
        }
        case ASTNodeType::STMT_CONSOLE: {
            ConsoleStatement* consoleStmt = static_cast<ConsoleStatement*>(stmt);
            for (auto& item : consoleStmt->items) {
                if (item.expr) {
                    ExpressionPtr optimized = optimizeExpression(item.expr, stats);
                    if (optimized) {
                        item.expr = std::move(optimized);
                        changed = true;
                    }
                }
            }
            break;
        }
        case ASTNodeType::STMT_IF: {
            IfStatement* ifStmt = static_cast<IfStatement*>(stmt);
            if (ifStmt->condition) {
                ExpressionPtr optimized = optimizeExpression(ifStmt->condition, stats);
                if (optimized) {
                    ifStmt->condition = std::move(optimized);
                    changed = true;
                }
            }
            // Recursively optimize then/else statements
            for (auto& thenStmt : ifStmt->thenStatements) {
                if (optimizeStatement(thenStmt.get(), stats)) {
                    changed = true;
                }
            }
            for (auto& elseStmt : ifStmt->elseStatements) {
                if (optimizeStatement(elseStmt.get(), stats)) {
                    changed = true;
                }
            }
            break;
        }
        case ASTNodeType::STMT_FOR: {
            ForStatement* forStmt = static_cast<ForStatement*>(stmt);
            if (forStmt->start) {
                ExpressionPtr optimized = optimizeExpression(forStmt->start, stats);
                if (optimized) {
                    forStmt->start = std::move(optimized);
                    changed = true;
                }
            }
            if (forStmt->end) {
                ExpressionPtr optimized = optimizeExpression(forStmt->end, stats);
                if (optimized) {
                    forStmt->end = std::move(optimized);
                    changed = true;
                }
            }
            if (forStmt->step) {
                ExpressionPtr optimized = optimizeExpression(forStmt->step, stats);
                if (optimized) {
                    forStmt->step = std::move(optimized);
                    changed = true;
                }
            }
            break;
        }
        case ASTNodeType::STMT_WHILE: {
            WhileStatement* whileStmt = static_cast<WhileStatement*>(stmt);
            if (whileStmt->condition) {
                ExpressionPtr optimized = optimizeExpression(whileStmt->condition, stats);
                if (optimized) {
                    whileStmt->condition = std::move(optimized);
                    changed = true;
                }
            }
            break;
        }
        case ASTNodeType::STMT_UNTIL: {
            UntilStatement* untilStmt = static_cast<UntilStatement*>(stmt);
            if (untilStmt->condition) {
                ExpressionPtr optimized = optimizeExpression(untilStmt->condition, stats);
                if (optimized) {
                    untilStmt->condition = std::move(optimized);
                    changed = true;
                }
            }
            break;
        }
        case ASTNodeType::STMT_DIM: {
            DimStatement* dimStmt = static_cast<DimStatement*>(stmt);
            for (auto& array : dimStmt->arrays) {
                for (auto& dim : array.dimensions) {
                    ExpressionPtr optimized = optimizeExpression(dim, stats);
                    if (optimized) {
                        dim = std::move(optimized);
                        changed = true;
                    }
                }
            }
            break;
        }
        default:
            // Other statement types don't have expressions to optimize
            break;
    }
    
    return changed;
}

// =============================================================================
// Optimization Pass Implementations
// =============================================================================

bool ConstantFoldingPass::run(Program& program, const SymbolTable& symbols,
                              OptimizationStats& stats) {
    bool changed = false;
    
    // Walk through all program lines and statements
    for (auto& line : program.lines) {
        for (auto& stmt : line->statements) {
            if (optimizeStatement(stmt.get(), stats)) {
                changed = true;
            }
        }
    }
    
    return changed;
}

bool DeadCodeEliminationPass::run(Program& program, const SymbolTable& symbols,
                                  OptimizationStats& stats) {
    bool changed = false;
    
    // Walk through all program lines
    for (auto& line : program.lines) {
        if (line->statements.empty()) continue;
        
        // Find first statement that unconditionally terminates
        size_t terminatorIndex = line->statements.size();
        bool foundTerminator = false;
        
        for (size_t i = 0; i < line->statements.size(); i++) {
            const auto& stmt = line->statements[i];
            ASTNodeType type = stmt->getType();
            
            // Check for unconditional terminators
            if (type == ASTNodeType::STMT_GOTO ||
                type == ASTNodeType::STMT_RETURN ||
                type == ASTNodeType::STMT_END) {
                terminatorIndex = i;
                foundTerminator = true;
                break;
            }
            
            // IF with unconditional GOTO in both branches could also be a terminator,
            // but that's more complex - skip for now
        }
        
        // If we found a terminator and there are statements after it, remove them
        if (foundTerminator && terminatorIndex + 1 < line->statements.size()) {
            size_t numRemoved = line->statements.size() - (terminatorIndex + 1);
            line->statements.erase(
                line->statements.begin() + terminatorIndex + 1,
                line->statements.end()
            );
            
            stats.deadCodeEliminations += static_cast<int>(numRemoved);
            stats.totalOptimizations += static_cast<int>(numRemoved);
            changed = true;
        }
    }
    
    return changed;
}

bool CommonSubexpressionPass::run(Program& program, const SymbolTable& symbols,
                                  OptimizationStats& stats) {
    // Common subexpression elimination is complex and requires:
    // 1. Expression hashing/comparison
    // 2. Tracking variable definitions and uses
    // 3. Dataflow analysis to ensure safety
    // 
    // This is a more advanced optimization that would require significant
    // infrastructure. For now, we skip it as constant folding and strength
    // reduction provide most of the benefit.
    
    // No-op for now
    return false;  // No changes made
}

bool StrengthReductionPass::run(Program& program, const SymbolTable& symbols,
                                OptimizationStats& stats) {
    // Most strength reduction is already handled in the constant folding pass
    // as algebraic simplifications (X*1->X, X+0->X, etc.)
    // 
    // More complex strength reductions like X*2->X+X would require expression
    // cloning infrastructure, which we don't have yet.
    // 
    // Additional strength reductions (like replacing X^2 with X*X) could be
    // added here in the future.
    
    // No-op for now (handled in ConstantFoldingPass)
    return false;  // No changes made
}

// =============================================================================
// ASTOptimizer Implementation
// =============================================================================

ASTOptimizer::ASTOptimizer()
    : m_optimizationLevel(1)
    , m_maxIterations(10)
    , m_iterationCount(0)
{
    registerPasses();
}

ASTOptimizer::~ASTOptimizer() = default;

void ASTOptimizer::registerPasses() {
    // Register optimization passes in order
    // These will be run in sequence until no more changes occur
    
    if (m_optimizationLevel >= 1) {
        // Basic optimizations
        m_passes.push_back(std::make_unique<ConstantFoldingPass>());
        m_passes.push_back(std::make_unique<DeadCodeEliminationPass>());
        // Note: ForLoopIndexExitPass is registered but currently does not perform transformations
        // It serves as documentation of the limitation and may be enhanced in the future
        m_passes.push_back(std::make_unique<ForLoopIndexExitPass>());
    }
    
    if (m_optimizationLevel >= 2) {
        // Aggressive optimizations
        m_passes.push_back(std::make_unique<CommonSubexpressionPass>());
        m_passes.push_back(std::make_unique<StrengthReductionPass>());
    }
}

void ASTOptimizer::clearPasses() {
    m_passes.clear();
}

bool ASTOptimizer::optimize(Program& program, const SymbolTable& symbols) {
    m_stats.reset();
    m_iterationCount = 0;
    
    // Rebuild passes based on current optimization level
    clearPasses();
    registerPasses();
    
    // Run optimization passes iteratively until no changes occur
    // or max iterations reached
    bool changed = true;
    while (changed && m_iterationCount < m_maxIterations) {
        changed = runSingleIteration(program, symbols);
        m_iterationCount++;
        
        if (!changed) {
            break;  // Converged
        }
    }
    
    // Always succeeds (optimizations are optional)
    return true;
}

bool ASTOptimizer::runSingleIteration(Program& program, const SymbolTable& symbols) {
    bool anyChanges = false;
    
    for (const auto& pass : m_passes) {
        // Check if pass is disabled
        bool disabled = std::find(m_disabledPasses.begin(), m_disabledPasses.end(),
                                 pass->getName()) != m_disabledPasses.end();
        if (disabled) {
            continue;
        }
        
        // Run the pass
        bool passChanged = pass->run(program, symbols, m_stats);
        if (passChanged) {
            anyChanges = true;
        }
    }
    
    return anyChanges;
}

void ASTOptimizer::enablePass(const std::string& passName) {
    auto it = std::find(m_disabledPasses.begin(), m_disabledPasses.end(), passName);
    if (it != m_disabledPasses.end()) {
        m_disabledPasses.erase(it);
    }
}

void ASTOptimizer::disablePass(const std::string& passName) {
    auto it = std::find(m_disabledPasses.begin(), m_disabledPasses.end(), passName);
    if (it == m_disabledPasses.end()) {
        m_disabledPasses.push_back(passName);
    }
}

std::string ASTOptimizer::generateReport() const {
    std::ostringstream oss;
    
    oss << "=== AST OPTIMIZER REPORT ===\n\n";
    
    // Configuration
    oss << "Configuration:\n";
    oss << "  Optimization Level: " << m_optimizationLevel << "\n";
    oss << "  Max Iterations: " << m_maxIterations << "\n";
    oss << "  Actual Iterations: " << m_iterationCount << "\n";
    oss << "  Active Passes: " << m_passes.size() << "\n";
    oss << "\n";
    
    // List passes
    oss << "Optimization Passes:\n";
    for (const auto& pass : m_passes) {
        bool disabled = std::find(m_disabledPasses.begin(), m_disabledPasses.end(),
                                 pass->getName()) != m_disabledPasses.end();
        oss << "  " << pass->getName();
        if (disabled) {
            oss << " [DISABLED]";
        }
        oss << "\n";
    }
    oss << "\n";
    
    // Statistics
    oss << m_stats.toString();
    oss << "\n";
    
    // Summary
    if (m_stats.totalOptimizations == 0) {
        oss << "Status: No optimizations applied (all passes are currently no-op)\n";
    } else {
        oss << "Status: " << m_stats.totalOptimizations << " optimization(s) applied\n";
    }
    
    oss << "\n=== END AST OPTIMIZER REPORT ===\n";
    
    return oss.str();
}

// =============================================================================
// FOR Loop Index Exit Pass Implementation
// =============================================================================

bool ForLoopIndexExitPass::run(Program& program, const SymbolTable& symbols, OptimizationStats& stats) {
    bool changed = false;
    
    // Track active FOR loop variables
    std::vector<std::string> activeLoopVars;
    
    // Walk through all program lines and their statements
    for (auto& linePtr : program.lines) {
        if (!linePtr) continue;
        
        for (auto& stmtPtr : linePtr->statements) {
            if (!stmtPtr) continue;
            
            Statement* stmt = stmtPtr.get();
            
            // Track FOR statements - add loop variable to active list
            if (stmt->getType() == ASTNodeType::STMT_FOR) {
                ForStatement* forStmt = static_cast<ForStatement*>(stmt);
                activeLoopVars.push_back(forStmt->variable);
            }
            
            // Track NEXT statements - remove loop variable from active list
            else if (stmt->getType() == ASTNodeType::STMT_NEXT) {
                NextStatement* nextStmt = static_cast<NextStatement*>(stmt);
                if (!nextStmt->variable.empty()) {
                    // Remove specified variable
                    auto it = std::find(activeLoopVars.begin(), activeLoopVars.end(), nextStmt->variable);
                    if (it != activeLoopVars.end()) {
                        activeLoopVars.erase(it);
                    }
                } else if (!activeLoopVars.empty()) {
                    // NEXT without variable - remove last loop variable
                    activeLoopVars.pop_back();
                }
            }
            
            // Check LET statements for assignment to loop index
            else if (stmt->getType() == ASTNodeType::STMT_LET && !activeLoopVars.empty()) {
                LetStatement* letStmt = static_cast<LetStatement*>(stmt);
                
                // Check if assigning to any active loop variable
                for (const auto& loopVar : activeLoopVars) {
                    if (letStmt->variable == loopVar) {
                        // Found assignment to loop index!
                        std::cerr << "\n";
                        std::cerr << "WARNING: Assignment to FOR loop index variable detected!\n";
                        std::cerr << "  Line: " << linePtr->lineNumber << "\n";
                        std::cerr << "  Variable: " << loopVar << "\n";
                        std::cerr << "  Pattern: " << loopVar << " = <expression>\n";
                        std::cerr << "\n";
                        std::cerr << "  This pattern does NOT work for early loop exit in compiled loops.\n";
                        std::cerr << "  The loop will continue to its original limit.\n";
                        std::cerr << "\n";
                        std::cerr << "  SOLUTION: Use 'EXIT FOR' instead:\n";
                        std::cerr << "    Before: IF condition THEN " << loopVar << " = limit\n";
                        std::cerr << "    After:  IF condition THEN EXIT FOR\n";
                        std::cerr << "\n";
                        
                        stats.forLoopIndexExits++;
                        // Note: We don't set changed=true because we're not modifying the AST
                        // We're just warning the user
                    }
                }
            }
        }
    }
    
    return changed;
}

} // namespace FasterBASIC