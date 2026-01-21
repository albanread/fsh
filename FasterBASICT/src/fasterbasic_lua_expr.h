//
// fasterbasic_lua_expr.h
// FasterBASIC - Lua Expression Optimizer
//
// Tracks symbolic expressions to emit direct Lua assignments
// instead of stack-based push/pop operations.
//
// Example:
//   Before: push(5); push(10); b=pop(); a=pop(); push(a+b); var_C=pop()
//   After:  var_C = 5 + 10
//

#ifndef FASTERBASIC_LUA_EXPR_H
#define FASTERBASIC_LUA_EXPR_H

#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace FasterBASIC {

// =============================================================================
// Expression Types
// =============================================================================

enum class ExprType {
    LITERAL,        // Constant value (5, "hello", 3.14)
    VARIABLE,       // Variable reference (var_A)
    ARRAY_ACCESS,   // Array access (arr_P[idx])
    BINARY_OP,      // Binary operation (a + b)
    UNARY_OP,       // Unary operation (-a, NOT a)
    CALL,           // Function call (math.sin(x))
    STACK_REF       // Reference to stack position (for complex cases)
};

enum class BinaryOp {
    ADD, SUB, MUL, DIV, IDIV, MOD, POW,
    EQ, NE, LT, LE, GT, GE,
    AND, OR, XOR, EQV, IMP,
    CONCAT  // String concatenation (Lua's ..)
};

enum class UnaryOp {
    NEG, NOT, ABS
};

// =============================================================================
// Expression Node
// =============================================================================

struct Expr {
    ExprType type;
    
    // For literals
    std::string literal;
    
    // For variables
    std::string varName;
    
    // For array access
    std::string arrayName;
    std::shared_ptr<Expr> arrayIndex;
    
    // For binary operations
    BinaryOp binaryOp;
    std::shared_ptr<Expr> left;
    std::shared_ptr<Expr> right;
    
    // For unary operations
    UnaryOp unaryOp;
    std::shared_ptr<Expr> operand;
    
    // For function calls
    std::string funcName;
    std::vector<std::shared_ptr<Expr>> args;
    
    // For stack references
    int stackPos;
    
    Expr() : type(ExprType::LITERAL), binaryOp(BinaryOp::ADD), 
             unaryOp(UnaryOp::NEG), stackPos(-1) {}
    
    static std::shared_ptr<Expr> makeLiteral(const std::string& value) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::LITERAL;
        e->literal = value;
        return e;
    }
    
    static std::shared_ptr<Expr> makeVariable(const std::string& name) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::VARIABLE;
        e->varName = name;
        return e;
    }
    
    static std::shared_ptr<Expr> makeArrayAccess(const std::string& name, 
                                                   std::shared_ptr<Expr> index) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::ARRAY_ACCESS;
        e->arrayName = name;
        e->arrayIndex = index;
        return e;
    }
    
    static std::shared_ptr<Expr> makeBinaryOp(BinaryOp op, 
                                                std::shared_ptr<Expr> l, 
                                                std::shared_ptr<Expr> r) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::BINARY_OP;
        e->binaryOp = op;
        e->left = l;
        e->right = r;
        return e;
    }
    
    static std::shared_ptr<Expr> makeUnaryOp(UnaryOp op, 
                                               std::shared_ptr<Expr> operand) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::UNARY_OP;
        e->unaryOp = op;
        e->operand = operand;
        return e;
    }
    
    static std::shared_ptr<Expr> makeCall(const std::string& name, 
                                            const std::vector<std::shared_ptr<Expr>>& args) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::CALL;
        e->funcName = name;
        e->args = args;
        return e;
    }
    
    static std::shared_ptr<Expr> makeStackRef(int pos) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::STACK_REF;
        e->stackPos = pos;
        return e;
    }
};

// =============================================================================
// Expression Optimizer
// =============================================================================

class ExpressionOptimizer {
public:
    ExpressionOptimizer();
    ~ExpressionOptimizer();
    
    // Push an expression onto the symbolic stack
    void pushLiteral(const std::string& value);
    void pushVariable(const std::string& name);
    void pushArrayAccess(const std::string& arrayName, std::shared_ptr<Expr> index);
    
    // Pop expression from stack
    std::shared_ptr<Expr> pop();
    
    // Check if stack is empty
    bool isEmpty() const { return m_stack.empty(); }
    
    // Get stack size
    size_t size() const { return m_stack.size(); }
    
    // Peek at top of stack
    std::shared_ptr<Expr> peek() const;
    
    // Apply operations
    void applyBinaryOp(BinaryOp op);
    void applyUnaryOp(UnaryOp op);
    void applyCall(const std::string& funcName, int argCount);
    
    // Convert expression to Lua code
    std::string toString(std::shared_ptr<Expr> expr) const;
    
    // Check if expression is simple enough to inline
    bool isSimple(std::shared_ptr<Expr> expr) const;
    
    // Check if expression has side effects
    bool hasSideEffects(std::shared_ptr<Expr> expr) const;
    
    // Reset the optimizer state
    void reset();
    
    // Get stack depth (for debugging)
    int getStackDepth() const { return m_stack.size(); }
    
    // Set Unicode mode (for proper string comparison)
    void setUnicodeMode(bool enabled) { m_unicodeMode = enabled; }
    bool isUnicodeMode() const { return m_unicodeMode; }

private:
    std::vector<std::shared_ptr<Expr>> m_stack;
    bool m_unicodeMode = false;
    
    // Helper to get operator string
    static std::string getBinaryOpStr(BinaryOp op);
    std::string getUnaryOpStr(UnaryOp op) const;
    
    // Helper to determine operator precedence
    static int getPrecedence(BinaryOp op);
    
    // Helper to add parentheses if needed
    std::string maybeParenthesize(std::shared_ptr<Expr> expr, int parentPrecedence) const;
};

// =============================================================================
// Inline Implementations
// =============================================================================

inline ExpressionOptimizer::ExpressionOptimizer() : m_unicodeMode(false) {
}

inline ExpressionOptimizer::~ExpressionOptimizer() {
}

inline void ExpressionOptimizer::reset() {
    m_stack.clear();
    // Note: m_unicodeMode is NOT reset - it's set once per code generation
}

inline void ExpressionOptimizer::pushLiteral(const std::string& value) {
    m_stack.push_back(Expr::makeLiteral(value));
}

inline void ExpressionOptimizer::pushVariable(const std::string& name) {
    m_stack.push_back(Expr::makeVariable(name));
}

inline void ExpressionOptimizer::pushArrayAccess(const std::string& arrayName, 
                                                   std::shared_ptr<Expr> index) {
    m_stack.push_back(Expr::makeArrayAccess(arrayName, index));
}

inline std::shared_ptr<Expr> ExpressionOptimizer::pop() {
    if (m_stack.empty()) {
        return nullptr;
    }
    auto expr = m_stack.back();
    m_stack.pop_back();
    return expr;
}

inline std::shared_ptr<Expr> ExpressionOptimizer::peek() const {
    if (m_stack.empty()) {
        return nullptr;
    }
    return m_stack.back();
}

inline void ExpressionOptimizer::applyBinaryOp(BinaryOp op) {
    if (m_stack.size() < 2) return;
    
    auto right = pop();
    auto left = pop();
    m_stack.push_back(Expr::makeBinaryOp(op, left, right));
}

inline void ExpressionOptimizer::applyUnaryOp(UnaryOp op) {
    if (m_stack.empty()) return;
    
    auto operand = pop();
    m_stack.push_back(Expr::makeUnaryOp(op, operand));
}

inline void ExpressionOptimizer::applyCall(const std::string& funcName, int argCount) {
    if (m_stack.size() < static_cast<size_t>(argCount)) return;
    
    std::vector<std::shared_ptr<Expr>> args;
    for (int i = 0; i < argCount; i++) {
        args.insert(args.begin(), pop());
    }
    m_stack.push_back(Expr::makeCall(funcName, args));
}

inline bool ExpressionOptimizer::isSimple(std::shared_ptr<Expr> expr) const {
    if (!expr) return false;
    
    switch (expr->type) {
        case ExprType::LITERAL:
        case ExprType::VARIABLE:
            return true;
        case ExprType::ARRAY_ACCESS:
            return isSimple(expr->arrayIndex);
        case ExprType::BINARY_OP:
            return isSimple(expr->left) && isSimple(expr->right);
        case ExprType::UNARY_OP:
            return isSimple(expr->operand);
        default:
            return false;
    }
}

inline bool ExpressionOptimizer::hasSideEffects(std::shared_ptr<Expr> expr) const {
    // Currently our expressions don't have side effects
    // This would change if we add function calls with side effects
    return false;
}

} // namespace FasterBASIC

#endif // FASTERBASIC_LUA_EXPR_H