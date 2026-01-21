//
// fasterbasic_peephole.cpp
// FasterBASIC - Peephole Optimizer Implementation
//
// Implements the peephole optimization framework with placeholder passes.
// This is Phase 6 of the compilation pipeline.
//

#include "fasterbasic_peephole.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cmath>

namespace FasterBASIC {

// =============================================================================
// Helper Methods (Base Class)
// =============================================================================

bool PeepholePass::operandsEqual(const IROperand& a, const IROperand& b) const {
    if (a.index() != b.index()) {
        return false;
    }
    
    if (std::holds_alternative<std::monostate>(a)) {
        return true;
    } else if (std::holds_alternative<int>(a)) {
        return std::get<int>(a) == std::get<int>(b);
    } else if (std::holds_alternative<double>(a)) {
        return std::get<double>(a) == std::get<double>(b);
    } else if (std::holds_alternative<std::string>(a)) {
        return std::get<std::string>(a) == std::get<std::string>(b);
    }
    
    return false;
}

bool PeepholePass::hasSideEffects(const IRInstruction& instr) const {
    switch (instr.opcode) {
        // Pure stack operations (no side effects)
        case IROpcode::PUSH_INT:
        case IROpcode::PUSH_FLOAT:
        case IROpcode::PUSH_DOUBLE:
        case IROpcode::PUSH_STRING:
        case IROpcode::POP:
        case IROpcode::DUP:
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::IDIV:
        case IROpcode::MOD:
        case IROpcode::POW:
        case IROpcode::NEG:
        case IROpcode::NOT:
        case IROpcode::EQ:
        case IROpcode::NE:
        case IROpcode::LT:
        case IROpcode::LE:
        case IROpcode::GT:
        case IROpcode::GE:
        case IROpcode::AND:
        case IROpcode::OR:
        case IROpcode::LOAD_VAR:
        case IROpcode::LOAD_ARRAY:
        case IROpcode::NOP:
            return false;
        
        // Everything else has side effects (I/O, control flow, stores, etc.)
        default:
            return true;
    }
}

// =============================================================================
// Constant Folding Pass (NO-OP for now)
// =============================================================================

bool PeepholeConstantFoldingPass::optimize(IRCode& code) {
    // Pattern: PUSH const1, PUSH const2, OP → PUSH result
    
    m_stats.passName = getName();
    m_stats.reset();
    
    bool changed = false;
    
    // Scan for PUSH, PUSH, OP patterns
    for (size_t i = 0; i + 2 < code.instructions.size(); ) {
        double val1, val2;
        IROpcode op;
        
        if (matchPushPushOp(code, i, val1, val2, op)) {
            if (canFold(op)) {
                // Check for division/modulo by zero
                if ((op == IROpcode::DIV || op == IROpcode::IDIV || op == IROpcode::MOD) && val2 == 0.0) {
                    i++;
                    continue;  // Don't fold division by zero
                }
                
                // Fold the operation
                double result = foldOperation(op, val1, val2);
                
                // Replace PUSH, PUSH, OP with single PUSH result
                code.instructions[i] = IRInstruction(IROpcode::PUSH_DOUBLE, result);
                code.instructions[i].sourceLineNumber = code.instructions[i + 2].sourceLineNumber;
                
                // Mark next two instructions as NOP
                code.instructions[i + 1].opcode = IROpcode::NOP;
                code.instructions[i + 2].opcode = IROpcode::NOP;
                
                m_stats.optimizationsApplied++;
                m_stats.instructionsRemoved += 2;
                m_stats.patternsMatched++;
                changed = true;
                
                i++; // Move past the folded instruction
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
    
    return changed;
}

bool PeepholeConstantFoldingPass::matchPushPushOp(const IRCode& code, size_t index,
                                          double& val1, double& val2, IROpcode& op) const {
    if (index + 2 >= code.instructions.size()) {
        return false;
    }
    
    const auto& instr1 = code.instructions[index];
    const auto& instr2 = code.instructions[index + 1];
    const auto& instr3 = code.instructions[index + 2];
    
    // Check for PUSH (int, float, or double)
    bool isPush1 = (instr1.opcode == IROpcode::PUSH_INT ||
                    instr1.opcode == IROpcode::PUSH_FLOAT ||
                    instr1.opcode == IROpcode::PUSH_DOUBLE);
    
    bool isPush2 = (instr2.opcode == IROpcode::PUSH_INT ||
                    instr2.opcode == IROpcode::PUSH_FLOAT ||
                    instr2.opcode == IROpcode::PUSH_DOUBLE);
    
    if (!isPush1 || !isPush2) {
        return false;
    }
    
    // Extract values
    if (std::holds_alternative<int>(instr1.operand1)) {
        val1 = static_cast<double>(std::get<int>(instr1.operand1));
    } else if (std::holds_alternative<double>(instr1.operand1)) {
        val1 = std::get<double>(instr1.operand1);
    } else {
        return false;
    }
    
    if (std::holds_alternative<int>(instr2.operand1)) {
        val2 = static_cast<double>(std::get<int>(instr2.operand1));
    } else if (std::holds_alternative<double>(instr2.operand1)) {
        val2 = std::get<double>(instr2.operand1);
    } else {
        return false;
    }
    
    // Check if third instruction is a foldable operation
    op = instr3.opcode;
    return canFold(op);
}

bool PeepholeConstantFoldingPass::canFold(IROpcode op) const {
    switch (op) {
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::IDIV:
        case IROpcode::MOD:
        case IROpcode::POW:
        case IROpcode::EQ:
        case IROpcode::NE:
        case IROpcode::LT:
        case IROpcode::LE:
        case IROpcode::GT:
        case IROpcode::GE:
        case IROpcode::AND:
        case IROpcode::OR:
            return true;
        default:
            return false;
    }
}

double PeepholeConstantFoldingPass::foldOperation(IROpcode op, double a, double b) const {
    switch (op) {
        case IROpcode::ADD: return a + b;
        case IROpcode::SUB: return a - b;
        case IROpcode::MUL: return a * b;
        case IROpcode::DIV: return a / b;
        case IROpcode::IDIV: return static_cast<int>(a) / static_cast<int>(b);
        case IROpcode::MOD: return std::fmod(a, b);
        case IROpcode::POW: return std::pow(a, b);
        case IROpcode::EQ: return (a == b) ? -1.0 : 0.0;
        case IROpcode::NE: return (a != b) ? -1.0 : 0.0;
        case IROpcode::LT: return (a < b) ? -1.0 : 0.0;
        case IROpcode::LE: return (a <= b) ? -1.0 : 0.0;
        case IROpcode::GT: return (a > b) ? -1.0 : 0.0;
        case IROpcode::GE: return (a >= b) ? -1.0 : 0.0;
        case IROpcode::AND: return (a != 0.0 && b != 0.0) ? -1.0 : 0.0;
        case IROpcode::OR: return (a != 0.0 || b != 0.0) ? -1.0 : 0.0;
        default: return 0.0;
    }
}

// =============================================================================
// Dead Code Elimination Pass (NO-OP for now)
// =============================================================================

bool PeepholeDeadCodeEliminationPass::optimize(IRCode& code) {
    // TODO: Implement dead code elimination
    // Remove unreachable code after unconditional jumps, etc.
    
    m_stats.passName = getName();
    m_stats.reset();
    
    // NO-OP: Return false to indicate no changes made
    return false;
}

void PeepholeDeadCodeEliminationPass::markReachable(const IRCode& code,
                                           std::vector<bool>& reachable) const {
    // TODO: Mark all reachable instructions starting from entry point
}

bool PeepholeDeadCodeEliminationPass::isReachableFrom(const IRCode& code, size_t index) const {
    // TODO: Check if instruction is reachable from any label
    return true;
}

// =============================================================================
// Redundant Load/Store Elimination Pass (NO-OP for now)
// =============================================================================

bool PeepholeRedundantLoadStorePass::optimize(IRCode& code) {
    // TODO: Implement redundant load/store elimination
    // Pattern: STORE_VAR X, LOAD_VAR X → STORE_VAR X, DUP
    
    m_stats.passName = getName();
    m_stats.reset();
    
    // NO-OP: Return false to indicate no changes made
    return false;
}

bool PeepholeRedundantLoadStorePass::matchStoreLoad(const IRCode& code, size_t index) const {
    // TODO: Match STORE/LOAD pattern
    return false;
}

// =============================================================================
// Jump Optimization Pass (NO-OP for now)
// =============================================================================

bool PeepholeJumpOptimizationPass::optimize(IRCode& code) {
    // TODO: Implement jump optimization
    // - Jump to next instruction elimination
    // - Jump chain threading
    
    m_stats.passName = getName();
    m_stats.reset();
    
    // NO-OP: Return false to indicate no changes made
    return false;
}

bool PeepholeJumpOptimizationPass::matchJumpChain(const IRCode& code, size_t index) const {
    // TODO: Match jump chain pattern
    return false;
}

bool PeepholeJumpOptimizationPass::matchJumpToNext(const IRCode& code, size_t index) const {
    // TODO: Match jump to next instruction
    return false;
}

// =============================================================================
// Algebraic Simplification Pass (NO-OP for now)
// =============================================================================

bool PeepholeAlgebraicSimplificationPass::optimize(IRCode& code) {
    // TODO: Implement algebraic simplifications
    // X + 0 → X, X * 1 → X, X * 0 → 0, etc.
    
    m_stats.passName = getName();
    m_stats.reset();
    
    // NO-OP: Return false to indicate no changes made
    return false;
}

bool PeepholeAlgebraicSimplificationPass::matchIdentityAddition(const IRCode& code,
                                                        size_t index) const {
    // TODO: Match X + 0 or 0 + X
    return false;
}

bool PeepholeAlgebraicSimplificationPass::matchIdentityMultiplication(const IRCode& code,
                                                              size_t index) const {
    // TODO: Match X * 1 or 1 * X
    return false;
}

bool PeepholeAlgebraicSimplificationPass::matchZeroMultiplication(const IRCode& code,
                                                          size_t index) const {
    // TODO: Match X * 0 or 0 * X
    return false;
}

// =============================================================================
// Strength Reduction Pass (NO-OP for now)
// =============================================================================

bool PeepholeStrengthReductionPass::optimize(IRCode& code) {
    // TODO: Implement strength reduction
    // X * 2 → X + X, X ^ 2 → X * X, etc.
    
    m_stats.passName = getName();
    m_stats.reset();
    
    // NO-OP: Return false to indicate no changes made
    return false;
}

bool PeepholeStrengthReductionPass::matchMultiplyByTwo(const IRCode& code, size_t index) const {
    // TODO: Match X * 2
    return false;
}

bool PeepholeStrengthReductionPass::matchPowerOfTwo(const IRCode& code, size_t index) const {
    // TODO: Match X ^ 2
    return false;
}

// =============================================================================
// NOP Elimination Pass (NO-OP for now)
// =============================================================================

bool PeepholeNopEliminationPass::optimize(IRCode& code) {
    // Remove all NOP instructions
    
    m_stats.passName = getName();
    m_stats.reset();
    
    bool changed = false;
    
    // Remove NOPs (iterate backwards to avoid index issues)
    // IMPORTANT: Don't remove LABEL instructions even though they're NOPs,
    // because they're referenced by the label map
    for (int i = static_cast<int>(code.instructions.size()) - 1; i >= 0; i--) {
        if (code.instructions[i].opcode == IROpcode::NOP) {
            code.instructions.erase(code.instructions.begin() + i);
            m_stats.instructionsRemoved++;
            m_stats.optimizationsApplied++;
            changed = true;
        }
    }
    
    // Rebuild label map after removing instructions
    if (changed) {
        code.labelToAddress.clear();
        for (size_t i = 0; i < code.instructions.size(); i++) {
            if (code.instructions[i].opcode == IROpcode::LABEL) {
                int labelId = std::get<int>(code.instructions[i].operand1);
                code.labelToAddress[labelId] = static_cast<int>(i);
            }
        }
    }
    
    return changed;
}

// =============================================================================
// Peephole Optimizer (Main Class)
// =============================================================================

PeepholeOptimizer::PeepholeOptimizer()
    : m_optimizationLevel(1)
    , m_maxIterations(10)
    , m_traceEnabled(false)
{
    registerPasses();
}

void PeepholeOptimizer::registerPasses() {
    // Register all optimization passes in order
    
    // Basic optimizations (O1+)
    m_passes.push_back(std::make_unique<PeepholeNopEliminationPass>());
    m_passes.push_back(std::make_unique<PeepholeConstantFoldingPass>());
    m_passes.push_back(std::make_unique<PeepholeRedundantLoadStorePass>());
    m_passes.push_back(std::make_unique<PeepholeJumpOptimizationPass>());
    
    // Aggressive optimizations (O2+)
    m_passes.push_back(std::make_unique<PeepholeDeadCodeEliminationPass>());
    m_passes.push_back(std::make_unique<PeepholeAlgebraicSimplificationPass>());
    m_passes.push_back(std::make_unique<PeepholeStrengthReductionPass>());
    
    // Build pass lookup map
    for (auto& pass : m_passes) {
        m_passMap[pass->getName()] = pass.get();
    }
}

void PeepholeOptimizer::optimize(IRCode& code) {
    // Run peephole optimizations at all levels (even O0)
    // At O0, we'll just run the most basic passes
    
    resetStats();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Run optimization passes iteratively until no more changes or max iterations
    for (int iter = 0; iter < m_maxIterations; iter++) {
        bool changed = runIteration(code);
        m_stats.totalIterations++;
        
        if (!changed) {
            // No more optimizations possible
            break;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_stats.totalExecutionTimeMs = duration.count() / 1000.0;
}

bool PeepholeOptimizer::runIteration(IRCode& code) {
    bool anyChanges = false;
    
    for (auto& pass : m_passes) {
        if (!pass->isEnabled()) {
            continue;
        }
        
        // Check optimization level requirements
        if (m_optimizationLevel == 1) {
            // O1: Basic optimizations only
            if (pass->getName() == "PeepholeDeadCodeElimination" ||
                pass->getName() == "PeepholeAlgebraicSimplification" ||
                pass->getName() == "PeepholeStrengthReduction") {
                continue;  // Skip aggressive optimizations
            }
        }
        
        auto passStartTime = std::chrono::high_resolution_clock::now();
        
        bool changed = pass->optimize(code);
        
        auto passEndTime = std::chrono::high_resolution_clock::now();
        auto passDuration = std::chrono::duration_cast<std::chrono::microseconds>(
            passEndTime - passStartTime);
        
        if (changed) {
            anyChanges = true;
            
            // Update statistics
            const auto& passStats = pass->getStats();
            m_stats.totalOptimizations += passStats.optimizationsApplied;
            m_stats.totalInstructionsRemoved += passStats.instructionsRemoved;
            m_stats.totalInstructionsAdded += passStats.instructionsAdded;
            m_stats.totalPatternsMatched += passStats.patternsMatched;
            m_stats.totalPasses++;
            
            // Store per-pass statistics
            auto& storedStats = m_stats.passStat[pass->getName()];
            storedStats.passName = pass->getName();
            storedStats.optimizationsApplied += passStats.optimizationsApplied;
            storedStats.instructionsRemoved += passStats.instructionsRemoved;
            storedStats.instructionsAdded += passStats.instructionsAdded;
            storedStats.patternsMatched += passStats.patternsMatched;
            storedStats.executionTimeMs += passDuration.count() / 1000.0;
        }
    }
    
    return anyChanges;
}

void PeepholeOptimizer::setOptimizationLevel(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    m_optimizationLevel = level;
}

void PeepholeOptimizer::enablePass(const std::string& passName) {
    auto* pass = getPass(passName);
    if (pass) {
        pass->setEnabled(true);
    }
}

void PeepholeOptimizer::disablePass(const std::string& passName) {
    auto* pass = getPass(passName);
    if (pass) {
        pass->setEnabled(false);
    }
}

void PeepholeOptimizer::enableAllPasses() {
    for (auto& pass : m_passes) {
        pass->setEnabled(true);
    }
}

void PeepholeOptimizer::disableAllPasses() {
    for (auto& pass : m_passes) {
        pass->setEnabled(false);
    }
}

PeepholePass* PeepholeOptimizer::getPass(const std::string& name) {
    auto it = m_passMap.find(name);
    if (it != m_passMap.end()) {
        return it->second;
    }
    return nullptr;
}

void PeepholeOptimizer::resetStats() {
    m_stats.reset();
    for (auto& pass : m_passes) {
        pass->resetStats();
    }
}

// =============================================================================
// Reporting
// =============================================================================

std::string PeepholeOptimizer::generateReport() const {
    std::ostringstream oss;
    
    oss << "=== PEEPHOLE OPTIMIZER REPORT ===\n\n";
    
    // Configuration
    oss << "Configuration:\n";
    oss << "  Optimization Level: O" << m_optimizationLevel << "\n";
    oss << "  Max Iterations: " << m_maxIterations << "\n";
    oss << "  Total Iterations: " << m_stats.totalIterations << "\n";
    oss << "\n";
    
    // Summary statistics
    oss << "Summary:\n";
    oss << "  Total Optimizations: " << m_stats.totalOptimizations << "\n";
    oss << "  Total Patterns Matched: " << m_stats.totalPatternsMatched << "\n";
    oss << "  Instructions Removed: " << m_stats.totalInstructionsRemoved << "\n";
    oss << "  Instructions Added: " << m_stats.totalInstructionsAdded << "\n";
    oss << "  Net Change: " << m_stats.netInstructionChange() 
        << " instructions\n";
    oss << "  Execution Time: " << std::fixed << std::setprecision(3) 
        << m_stats.totalExecutionTimeMs << " ms\n";
    oss << "\n";
    
    // Per-pass statistics
    if (!m_stats.passStat.empty()) {
        oss << "Pass Statistics:\n";
        oss << "  " << std::left << std::setw(30) << "Pass Name"
            << std::right << std::setw(12) << "Optimizations"
            << std::setw(12) << "Patterns"
            << std::setw(12) << "Removed"
            << std::setw(12) << "Added"
            << std::setw(12) << "Time (ms)"
            << "\n";
        oss << "  " << std::string(90, '-') << "\n";
        
        for (const auto& [name, stats] : m_stats.passStat) {
            oss << "  " << std::left << std::setw(30) << name
                << std::right << std::setw(12) << stats.optimizationsApplied
                << std::setw(12) << stats.patternsMatched
                << std::setw(12) << stats.instructionsRemoved
                << std::setw(12) << stats.instructionsAdded
                << std::setw(12) << std::fixed << std::setprecision(2) 
                << stats.executionTimeMs
                << "\n";
        }
        oss << "\n";
    }
    
    // Pass descriptions
    oss << "Available Passes:\n";
    for (const auto& pass : m_passes) {
        oss << "  - " << pass->getName() << ": "
            << (pass->isEnabled() ? "[ENABLED]" : "[DISABLED]") << "\n";
        oss << "    " << pass->getDescription() << "\n";
    }
    oss << "\n";
    
    oss << "=== END PEEPHOLE OPTIMIZER REPORT ===\n";
    
    return oss.str();
}

std::string PeepholeOptimizer::generateSummary() const {
    std::ostringstream oss;
    
    if (m_stats.totalOptimizations == 0) {
        oss << "Peephole Optimizer: No optimizations applied (O" 
            << m_optimizationLevel << ")";
    } else {
        oss << "Peephole Optimizer: " << m_stats.totalOptimizations 
            << " optimization(s) applied";
        
        if (m_stats.netInstructionChange() < 0) {
            oss << ", reduced by " << (-m_stats.netInstructionChange()) 
                << " instruction(s)";
        } else if (m_stats.netInstructionChange() > 0) {
            oss << ", increased by " << m_stats.netInstructionChange() 
                << " instruction(s)";
        }
        
        oss << " (O" << m_optimizationLevel << ")";
    }
    
    return oss.str();
}

} // namespace FasterBASIC