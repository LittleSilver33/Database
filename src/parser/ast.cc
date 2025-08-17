#include "parser/ast.h"

std::string Expr::to_string() const {
    if (auto p = dynamic_cast<const Literal*>(this)) {
        return p->to_string();
    }

    if (auto p = dynamic_cast<const Identifier*>(this)) {
        return p->name;
    }

    if (auto p = dynamic_cast<const Unary*>(this)) {
        std::string op = (p->op==Unary::Op::Plus?"+":p->op==Unary::Op::Minus?"-":"NOT ");
        return op + "(" + (*p->rhs).to_string() + ")";
    }

    if (auto p = dynamic_cast<const Binary*>(this)) {
        auto opToStr = [](Binary::Op o)->const char*{
            switch(o){
                case Binary::Op::Eq:  return "=";
                case Binary::Op::Ne:  return "!=";
                case Binary::Op::Lt:  return "<";
                case Binary::Op::Le:  return "<=";
                case Binary::Op::Gt:  return ">";
                case Binary::Op::Ge:  return ">=";
                case Binary::Op::And: return "AND";
                case Binary::Op::Or:  return "OR";
            }
            return "?";
        };
        return "(" + (*p->lhs).to_string() + " " + opToStr(p->op) + " " + (*p->rhs).to_string() + ")";
    }
    return "<?expr?>";
}

std::string Literal::to_string() const {
    if (std::holds_alternative<int64_t>(value)) return std::to_string(std::get<int64_t>(value));
    if (std::holds_alternative<double>(value))  return std::to_string(std::get<double>(value));
    return "'" + std::get<std::string>(value) + "'";
}