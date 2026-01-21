//
// fasterbasic_optimizer.h
// FasterBASIC - AST Optimizer
//
// Performs optimization passes on the validated AST before CFG construction.
// Currently a no-op framework ready for future optimization passes.
// This is Phase 3.5 in the compilation pipeline.
//

#ifndef FASTERBASIC_OPTIMIZER_H
#define FASTERBASIC_OPTIMIZER_H

#include "fasterbasic_ast.h"
#include "fasterbasic_semantic.h"
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace FasterBASIC {

// =============================================================================
// Optimization Statistics
// =============================================================================

struct OptimizationStats {
    int constantFolds = 0;
    int deadCodeEliminations = 0;
    int commonSubexpressions = 0;
    int strengthReductions = 0;
    int forLoopIndexExits = 0;
    int totalOptimizations = 0;
    
    void reset() {
        constantFolds = 0;
        deadCodeEliminations = 0;
        commonSubexpressions = 0;
        strengthReductions = 0;
        forLoopIndexExits = 0;
        totalOptimizations = 0;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Optimization Statistics:\n";
        oss << "  Constant Folds: " << constantFolds << "\n";
        oss << "  Dead Code Eliminations: " << deadCodeEliminations << "\n";
        oss << "  Common Subexpressions: " << commonSubexpressions << "\n";
        oss << "  Strength Reductions: " << strengthReductions << "\n";
        oss << "  FOR Loop Index Exits: " << forLoopIndexExits << "\n";
        oss << "  Total Optimizations: " << totalOptimizations << "\n";
        return oss.str();
    }
};

// =============================================================================
// Optimization Pass Base Class
// =============================================================================

class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    
    // Pass name for reporting
    virtual std::string getName() const = 0;
    
    // Run the pass on the program
    virtual bool run(Program& program, const SymbolTable& symbols, 
                     OptimizationStats& stats) = 0;
    
    // Whether this pass modifies the AST
    virtual bool isTransformPass() const { return true; }
    
    // Whether this pass requires symbol table
    virtual bool requiresSymbols() const { return false; }
};

// =============================================================================
// Individual Optimization Passes (Placeholder implementations)
// =============================================================================

// Pass 1: Constant Folding
// Evaluates constant expressions at compile time (e.g., 2 + 3 -> 5)
class ConstantFoldingPass : public OptimizationPass {
public:
    std::string getName() const override { return "Constant Folding"; }
    bool run(Program& program, const SymbolTable& symbols, 
             OptimizationStats& stats) override;
};

// Pass 2: Dead Code Elimination
// Removes unreachable statements (e.g., after GOTO, after RETURN)
class DeadCodeEliminationPass : public OptimizationPass {
public:
    std::string getName() const override { return "Dead Code Elimination"; }
    bool run(Program& program, const SymbolTable& symbols, 
             OptimizationStats& stats) override;
    bool requiresSymbols() const override { return true; }
};

// Pass 3: Common Subexpression Elimination
// Identifies and eliminates redundant calculations
class CommonSubexpressionPass : public OptimizationPass {
public:
    std::string getName() const override { return "Common Subexpression Elimination"; }
    bool run(Program& program, const SymbolTable& symbols, 
             OptimizationStats& stats) override;
};

// Pass 4: Strength Reduction
// Replaces expensive operations with cheaper ones (e.g., X * 2 -> X + X)
class StrengthReductionPass : public OptimizationPass {
public:
    std::string getName() const override { return "Strength Reduction"; }
    bool run(Program& program, const SymbolTable& symbols, 
             OptimizationStats& stats) override;
};

// Pass 5: FOR Loop Index Exit Detection
// Detects pattern "index = limit" and replaces with "EXIT FOR"
class ForLoopIndexExitPass : public OptimizationPass {
public:
    std::string getName() const override { return "FOR Loop Index Exit Detection"; }
    bool run(Program& program, const SymbolTable& symbols, 
             OptimizationStats& stats) override;
    bool requiresSymbols() const override { return false; }
};

// =============================================================================
// AST Optimizer
// =============================================================================

class ASTOptimizer {
public:
    ASTOptimizer();
    ~ASTOptimizer();
    
    // Main optimization entry point
    bool optimize(Program& program, const SymbolTable& symbols);
    
    // Configuration
    void setOptimizationLevel(int level) { m_optimizationLevel = level; }
    void enablePass(const std::string& passName);
    void disablePass(const std::string& passName);
    void setMaxIterations(int iterations) { m_maxIterations = iterations; }
    
    // Results
    const OptimizationStats& getStats() const { return m_stats; }
    int getPassCount() const { return static_cast<int>(m_passes.size()); }
    int getIterationCount() const { return m_iterationCount; }
    
    // Report generation
    std::string generateReport() const;
    
private:
    // Pass management
    void registerPasses();
    void clearPasses();
    
    // Optimization execution
    bool runSingleIteration(Program& program, const SymbolTable& symbols);
    
    // Data
    std::vector<std::unique_ptr<OptimizationPass>> m_passes;
    OptimizationStats m_stats;
    
    // Configuration
    int m_optimizationLevel;  // 0=none, 1=basic, 2=aggressive
    int m_maxIterations;      // Max iterations for iterative passes
    int m_iterationCount;     // Actual iterations run
    
    // Pass enable/disable tracking
    std::vector<std::string> m_disabledPasses;
};

} // namespace FasterBASIC

#endif // FASTERBASIC_OPTIMIZER_H