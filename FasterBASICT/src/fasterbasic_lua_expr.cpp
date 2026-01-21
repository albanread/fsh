//
// fasterbasic_lua_expr.cpp
// FasterBASIC - Lua Expression Optimizer Implementation
//

#include "fasterbasic_lua_expr.h"
#include <sstream>

namespace FasterBASIC {

// =============================================================================
// Expression to String Conversion
// =============================================================================

std::string ExpressionOptimizer::getBinaryOpStr(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD:  return "+";
        case BinaryOp::SUB:  return "-";
        case BinaryOp::MUL:  return "*";
        case BinaryOp::DIV:  return "/";
        case BinaryOp::IDIV: return "//";  // Lua 5.3+ integer division
        case BinaryOp::MOD:  return "%";
        case BinaryOp::POW:  return "^";
        case BinaryOp::EQ:   return "==";
        case BinaryOp::NE:   return "~=";
        case BinaryOp::LT:   return "<";
        case BinaryOp::LE:   return "<=";
        case BinaryOp::GT:   return ">";
        case BinaryOp::GE:   return ">=";
        case BinaryOp::AND:  return "and";
        case BinaryOp::OR:   return "or";
        case BinaryOp::CONCAT: return "..";  // String concatenation
        default: return "+";
    }
}

std::string ExpressionOptimizer::getUnaryOpStr(UnaryOp op) const {
    switch (op) {
        case UnaryOp::NEG: return "-";
        case UnaryOp::NOT: return "not ";
        case UnaryOp::ABS: return "math.abs";
        default: return "-";
    }
}

int ExpressionOptimizer::getPrecedence(BinaryOp op) {
    switch (op) {
        case BinaryOp::IMP:  return 1;  // Lowest precedence
        case BinaryOp::EQV:  return 2;
        case BinaryOp::EQ:
        case BinaryOp::NE:
        case BinaryOp::LT:
        case BinaryOp::LE:
        case BinaryOp::GT:
        case BinaryOp::GE:   return 3;
        case BinaryOp::OR:   return 4;
        case BinaryOp::XOR:  return 5;
        case BinaryOp::AND:  return 6;
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::CONCAT: return 7;  // String concat has same precedence as add
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::IDIV:
        case BinaryOp::MOD:  return 8;
        case BinaryOp::POW:  return 9;
        default: return 0;
    }
}

std::string ExpressionOptimizer::maybeParenthesize(std::shared_ptr<Expr> expr,
                                                     int parentPrecedence) const {
    if (!expr) return "";

    if (expr->type == ExprType::BINARY_OP) {
        int exprPrecedence = getPrecedence(expr->binaryOp);
        if (exprPrecedence < parentPrecedence) {
            return "(" + toString(expr) + ")";
        }
    }

    return toString(expr);
}

std::string ExpressionOptimizer::toString(std::shared_ptr<Expr> expr) const {
    if (!expr) return "nil";

    std::ostringstream oss;

    switch (expr->type) {
        case ExprType::LITERAL:
            return expr->literal;

        case ExprType::VARIABLE:
            return expr->varName;

        case ExprType::ARRAY_ACCESS:
            oss << expr->arrayName << "[" << toString(expr->arrayIndex) << "]";
            return oss.str();

        case ExprType::BINARY_OP: {
            int precedence = getPrecedence(expr->binaryOp);

            // Special handling for integer division - use math.floor for LuaJIT compatibility
            if (expr->binaryOp == BinaryOp::IDIV) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                oss << "math.floor(" << leftStr << " / " << rightStr << ")";
                return oss.str();
            }

            // Special handling for comparisons to return 1/0 instead of true/false
            bool isComparison = (expr->binaryOp == BinaryOp::EQ ||
                                expr->binaryOp == BinaryOp::NE ||
                                expr->binaryOp == BinaryOp::LT ||
                                expr->binaryOp == BinaryOp::LE ||
                                expr->binaryOp == BinaryOp::GT ||
                                expr->binaryOp == BinaryOp::GE);

            // In Unicode mode, use unicode_string_equal for EQ and NE comparisons
            // (Unicode strings are tables, so == compares references, not content)
            if (m_unicodeMode && (expr->binaryOp == BinaryOp::EQ || expr->binaryOp == BinaryOp::NE)) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                
                if (expr->binaryOp == BinaryOp::EQ) {
                    oss << "(unicode_string_equal(" << leftStr << ", " << rightStr << ") and -1 or 0)";
                } else {  // NE
                    oss << "((not unicode_string_equal(" << leftStr << ", " << rightStr << ")) and -1 or 0)";
                }
                return oss.str();
            }

            // In Unicode mode, use unicode_string_compare for ordered comparisons
            if (m_unicodeMode && (expr->binaryOp == BinaryOp::LT || 
                                 expr->binaryOp == BinaryOp::LE || 
                                 expr->binaryOp == BinaryOp::GT || 
                                 expr->binaryOp == BinaryOp::GE)) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                
                if (expr->binaryOp == BinaryOp::LT) {
                    oss << "((unicode_string_compare(" << leftStr << ", " << rightStr << ") < 0) and -1 or 0)";
                } else if (expr->binaryOp == BinaryOp::LE) {
                    oss << "((unicode_string_compare(" << leftStr << ", " << rightStr << ") <= 0) and -1 or 0)";
                } else if (expr->binaryOp == BinaryOp::GT) {
                    oss << "((unicode_string_compare(" << leftStr << ", " << rightStr << ") > 0) and -1 or 0)";
                } else {  // GE
                    oss << "((unicode_string_compare(" << leftStr << ", " << rightStr << ") >= 0) and -1 or 0)";
                }
                return oss.str();
            }

            // Use bitwise FFI functions for AND, OR, XOR, EQV, IMP (BASIC compatibility)
            if (expr->binaryOp == BinaryOp::AND) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                oss << "bitwise.band(" << leftStr << ", " << rightStr << ")";
                return oss.str();
            }
            
            if (expr->binaryOp == BinaryOp::OR) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                oss << "bitwise.bor(" << leftStr << ", " << rightStr << ")";
                return oss.str();
            }
            
            if (expr->binaryOp == BinaryOp::XOR) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                oss << "bitwise.bxor(" << leftStr << ", " << rightStr << ")";
                return oss.str();
            }
            
            if (expr->binaryOp == BinaryOp::EQV) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                oss << "bitwise.beqv(" << leftStr << ", " << rightStr << ")";
                return oss.str();
            }
            
            if (expr->binaryOp == BinaryOp::IMP) {
                std::string leftStr = maybeParenthesize(expr->left, precedence);
                std::string rightStr = maybeParenthesize(expr->right, precedence);
                oss << "bitwise.bimp(" << leftStr << ", " << rightStr << ")";
                return oss.str();
            }

            std::string leftStr = maybeParenthesize(expr->left, precedence);
            std::string rightStr = maybeParenthesize(expr->right, precedence);
            std::string opStr = getBinaryOpStr(expr->binaryOp);

            // Check if this is string concatenation (ADD with _STRING operands)
            if (expr->binaryOp == BinaryOp::ADD) {
                // Check if either operand has _STRING suffix (indicating string type)
                bool leftIsString = (leftStr.find("_STRING") != std::string::npos) ||
                                   (leftStr[0] == '"');  // String literal
                bool rightIsString = (rightStr.find("_STRING") != std::string::npos) ||
                                    (rightStr[0] == '"');  // String literal

                if (leftIsString || rightIsString) {
                    opStr = "..";  // Use Lua string concatenation operator
                }
            }

            if (isComparison) {
                // Wrap comparison in ternary to return -1/0 for BASIC compatibility
                oss << "((" << leftStr << " " << opStr << " " << rightStr << ") and -1 or 0)";
            } else {
                oss << leftStr << " " << opStr << " " << rightStr;
            }

            return oss.str();
        }

        case ExprType::UNARY_OP: {
            if (expr->unaryOp == UnaryOp::ABS) {
                // Function-style
                return "math.abs(" + toString(expr->operand) + ")";
            } else if (expr->unaryOp == UnaryOp::NOT) {
                // Use bitwise NOT for BASIC compatibility
                return "bitwise.bnot(" + toString(expr->operand) + ")";
            } else {
                // Prefix operator
                return getUnaryOpStr(expr->unaryOp) + toString(expr->operand);
            }
        }

        case ExprType::CALL:
            oss << expr->funcName << "(";
            for (size_t i = 0; i < expr->args.size(); i++) {
                if (i > 0) oss << ", ";
                oss << toString(expr->args[i]);
            }
            oss << ")";
            return oss.str();

        case ExprType::STACK_REF:
            // Fallback to stack reference
            oss << "stack[" << expr->stackPos << "]";
            return oss.str();

        default:
            return "nil";
    }
}

} // namespace FasterBASIC
