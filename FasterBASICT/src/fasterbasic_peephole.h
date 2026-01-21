//
// fasterbasic_peephole.h
// FasterBASIC - Peephole Optimizer
//
// Performs local optimizations on the IR code through pattern matching
// and instruction rewriting. This is Phase 6 of the compilation pipeline.
//

#ifndef FASTERBASIC_PEEPHOLE_H
#define FASTERBASIC_PEEPHOLE_H

#include "fasterbasic_ircode.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <sstream>

namespace FasterBASIC {

// =============================================================================
// Forward Declarations
// =============================================================================

class PeepholePass;
class PeepholeOptimizer;

// =============================================================================
// Peephole Pass Statistics
// =============================================================================

struct PeepholePassStats {
    std::string passName;
    int optimizationsApplied;
    int instructionsRemoved;
    int instructionsAdded;
    int patternsMatched;
    double executionTimeMs;
    
    PeepholePassStats()
        : optimizationsApplied(0)
        , instructionsRemoved(0)
        , instructionsAdded(0)
        , patternsMatched(0)
        , executionTimeMs(0.0)
    {}
    
    void reset() {
        optimizationsApplied = 0;
        instructionsRemoved = 0;
        instructionsAdded = 0;
        patternsMatched = 0;
        executionTimeMs = 0.0;
    }
    
    int netInstructionChange() const {
        return instructionsAdded - instructionsRemoved;
    }
};

// =============================================================================
// Optimizer Statistics
// =============================================================================

struct PeepholeOptimizerStats {
    int totalOptimizations;
    int totalInstructionsRemoved;
    int totalInstructionsAdded;
    int totalPatternsMatched;
    int totalPasses;
    int totalIterations;
    double totalExecutionTimeMs;
    
    std::map<std::string, PeepholePassStats> passStat;
    
    PeepholeOptimizerStats()
        : totalOptimizations(0)
        , totalInstructionsRemoved(0)
        , totalInstructionsAdded(0)
        , totalPatternsMatched(0)
        , totalPasses(0)
        , totalIterations(0)
        , totalExecutionTimeMs(0.0)
    {}
    
    void reset() {
        totalOptimizations = 0;
        totalInstructionsRemoved = 0;
        totalInstructionsAdded = 0;
        totalPatternsMatched = 0;
        totalPasses = 0;
        totalIterations = 0;
        totalExecutionTimeMs = 0.0;
        passStat.clear();
    }
    
    int netInstructionChange() const {
        return totalInstructionsAdded - totalInstructionsRemoved;
    }
};

// =============================================================================
// Peephole Optimization Pass (Base Class)
// =============================================================================

class PeepholePass {
public:
    virtual ~PeepholePass() = default;
    
    // Get pass name
    virtual std::string getName() const = 0;
    
    // Get pass description
    virtual std::string getDescription() const = 0;
    
    // Run the optimization pass on IR code
    // Returns true if any changes were made
    virtual bool optimize(IRCode& code) = 0;
    
    // Get statistics for this pass
    virtual PeepholePassStats getStats() const { return m_stats; }
    
    // Reset statistics
    virtual void resetStats() { m_stats.reset(); }
    
    // Enable/disable the pass
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
protected:
    PeepholePassStats m_stats;
    bool m_enabled = true;
    
    // Helper: Check if operands are equal
    bool operandsEqual(const IROperand& a, const IROperand& b) const;
    
    // Helper: Check if instruction is a label
    bool isLabel(const IRInstruction& instr) const {
        return instr.opcode == IROpcode::LABEL;
    }
    
    // Helper: Check if instruction is a jump
    bool isJump(const IRInstruction& instr) const {
        return instr.opcode == IROpcode::JUMP ||
               instr.opcode == IROpcode::JUMP_IF_TRUE ||
               instr.opcode == IROpcode::JUMP_IF_FALSE;
    }
    
    // Helper: Check if instruction is a NOP
    bool isNop(const IRInstruction& instr) const {
        return instr.opcode == IROpcode::NOP;
    }
    
    // Helper: Check if instruction has side effects
    bool hasSideEffects(const IRInstruction& instr) const;
};

// =============================================================================
// Constant Folding Pass
// =============================================================================

class PeepholeConstantFoldingPass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeConstantFolding"; }
    
    std::string getDescription() const override {
        return "Folds constant expressions at IR level (e.g., PUSH 2, PUSH 3, ADD → PUSH 5)";
    }
    
    bool optimize(IRCode& code) override;
    
private:
    // Pattern matching helpers
    bool matchPushPushOp(const IRCode& code, size_t index, 
                         double& val1, double& val2, IROpcode& op) const;
    
    bool canFold(IROpcode op) const;
    double foldOperation(IROpcode op, double a, double b) const;
};

// =============================================================================
// Dead Code Elimination Pass
// =============================================================================

class PeepholeDeadCodeEliminationPass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeDeadCodeElimination"; }
    
    std::string getDescription() const override {
        return "Removes unreachable code and unused instructions";
    }
    
    bool optimize(IRCode& code) override;
    
private:
    // Mark reachable instructions
    void markReachable(const IRCode& code, std::vector<bool>& reachable) const;
    
    // Check if instruction is reachable from a label
    bool isReachableFrom(const IRCode& code, size_t index) const;
};

// =============================================================================
// Redundant Load/Store Elimination Pass
// =============================================================================

class PeepholeRedundantLoadStorePass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeRedundantLoadStore"; }
    
    std::string getDescription() const override {
        return "Eliminates redundant LOAD_VAR after STORE_VAR (e.g., STORE X, LOAD X → STORE X, DUP)";
    }
    
    bool optimize(IRCode& code) override;
    
private:
    // Pattern: STORE_VAR X, LOAD_VAR X → STORE_VAR X (keep value on stack)
    bool matchStoreLoad(const IRCode& code, size_t index) const;
};

// =============================================================================
// Jump Optimization Pass
// =============================================================================

class PeepholeJumpOptimizationPass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeJumpOptimization"; }
    
    std::string getDescription() const override {
        return "Optimizes jump chains and removes jumps to next instruction";
    }
    
    bool optimize(IRCode& code) override;
    
private:
    // Pattern: JUMP L1, L1: JUMP L2 → JUMP L2
    bool matchJumpChain(const IRCode& code, size_t index) const;
    
    // Pattern: JUMP L1, L1: (next instruction) → remove jump
    bool matchJumpToNext(const IRCode& code, size_t index) const;
};

// =============================================================================
// Algebraic Simplification Pass
// =============================================================================

class PeepholeAlgebraicSimplificationPass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeAlgebraicSimplification"; }
    
    std::string getDescription() const override {
        return "Applies algebraic identities (e.g., X + 0 → X, X * 1 → X, X * 0 → 0)";
    }
    
    bool optimize(IRCode& code) override;
    
private:
    // Pattern: LOAD X, PUSH 0, ADD → LOAD X
    bool matchIdentityAddition(const IRCode& code, size_t index) const;
    
    // Pattern: LOAD X, PUSH 1, MUL → LOAD X
    bool matchIdentityMultiplication(const IRCode& code, size_t index) const;
    
    // Pattern: LOAD X, PUSH 0, MUL → PUSH 0
    bool matchZeroMultiplication(const IRCode& code, size_t index) const;
};

// =============================================================================
// Strength Reduction Pass
// =============================================================================

class PeepholeStrengthReductionPass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeStrengthReduction"; }
    
    std::string getDescription() const override {
        return "Replaces expensive operations with cheaper equivalents (e.g., X * 2 → X + X)";
    }
    
    bool optimize(IRCode& code) override;
    
private:
    // Pattern: X, PUSH 2, MUL → X, DUP, ADD
    bool matchMultiplyByTwo(const IRCode& code, size_t index) const;
    
    // Pattern: X, PUSH 2, POW → X, DUP, MUL
    bool matchPowerOfTwo(const IRCode& code, size_t index) const;
};

// =============================================================================
// NOP Elimination Pass
// =============================================================================

class PeepholeNopEliminationPass : public PeepholePass {
public:
    std::string getName() const override { return "PeepholeNopElimination"; }
    
    std::string getDescription() const override {
        return "Removes NOP (no-operation) instructions";
    }
    
    bool optimize(IRCode& code) override;
};

// =============================================================================
// Peephole Optimizer (Main Class)
// =============================================================================

class PeepholeOptimizer {
public:
    PeepholeOptimizer();
    ~PeepholeOptimizer() = default;
    
    // Optimize IR code
    void optimize(IRCode& code);
    
    // Configuration
    void setOptimizationLevel(int level);
    int getOptimizationLevel() const { return m_optimizationLevel; }
    
    void setMaxIterations(int maxIter) { m_maxIterations = maxIter; }
    int getMaxIterations() const { return m_maxIterations; }
    
    void setTraceEnabled(bool enabled) { m_traceEnabled = enabled; }
    bool isTraceEnabled() const { return m_traceEnabled; }
    
    // Pass management
    void enablePass(const std::string& passName);
    void disablePass(const std::string& passName);
    void enableAllPasses();
    void disableAllPasses();
    
    // Statistics
    const PeepholeOptimizerStats& getStats() const { return m_stats; }
    void resetStats();
    
    // Reporting
    std::string generateReport() const;
    std::string generateSummary() const;
    
private:
    // Optimization passes (in order of execution)
    std::vector<std::unique_ptr<PeepholePass>> m_passes;
    
    // Configuration
    int m_optimizationLevel;  // 0 = none, 1 = basic, 2 = aggressive
    int m_maxIterations;      // Maximum number of iterations
    bool m_traceEnabled;      // Enable trace output
    
    // Statistics
    PeepholeOptimizerStats m_stats;
    
    // Pass lookup
    std::map<std::string, PeepholePass*> m_passMap;
    
    // Register optimization passes
    void registerPasses();
    
    // Run a single iteration of all passes
    bool runIteration(IRCode& code);
    
    // Helper: Get pass by name
    PeepholePass* getPass(const std::string& name);
};

} // namespace FasterBASIC

#endif // FASTERBASIC_PEEPHOLE_H