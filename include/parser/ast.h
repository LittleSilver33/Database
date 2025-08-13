#pragma once
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <cstdint>

struct Expr {
  virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct Literal : Expr {
  using Value = std::variant<int64_t, double, std::string>;
  Value value;
  explicit Literal(Value v) : value(std::move(v)) {}
};

struct Identifier : Expr {
  std::string name;
  explicit Identifier(std::string n) : name(std::move(n)) {}
};

struct Unary : Expr {
  enum class Op { Plus, Minus, Not };
  Op op;
  ExprPtr rhs;
  Unary(Op o, ExprPtr r) : op(o), rhs(std::move(r)) {}
};

struct Binary : Expr {
  enum class Op { Add, Sub, Mul, Div, Eq, Ne, Lt, Le, Gt, Ge, And, Or };
  Op op;
  ExprPtr lhs, rhs;
  Binary(Op o, ExprPtr l, ExprPtr r)
    : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct Stmt {
  virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct Select : Stmt {
  // If select_all == true, ignore columns.
  bool select_all = false;
  std::vector<std::string> columns;
  std::string table;
  std::optional<ExprPtr> where;
};

struct Insert : Stmt {
  std::string table;
  std::vector<std::string> columns; // empty => implicit (table order)
  std::vector<ExprPtr> values;
};

struct Update : Stmt {
  std::string table;
  std::vector<std::pair<std::string, ExprPtr>> assignments;
  std::optional<ExprPtr> where;
};

// A tiny printer (useful for sanity checks)
inline std::string to_string(const Expr&);

inline std::string to_string(const Literal::Value& v) {
  if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
  if (std::holds_alternative<double>(v))  return std::to_string(std::get<double>(v));
  return "'" + std::get<std::string>(v) + "'";
}

inline std::string to_string(const Expr& e) {
  if (auto p = dynamic_cast<const Literal*>(&e)) return to_string(p->value);
  if (auto p = dynamic_cast<const Identifier*>(&e)) return p->name;
  if (auto p = dynamic_cast<const Unary*>(&e)) {
    std::string op = (p->op==Unary::Op::Plus?"+":p->op==Unary::Op::Minus?"-":"NOT ");
    return op + "(" + to_string(*p->rhs) + ")";
  }
  if (auto p = dynamic_cast<const Binary*>(&e)) {
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
    return "(" + to_string(*p->lhs) + " " + opToStr(p->op) + " " + to_string(*p->rhs) + ")";
  }
  return "<?expr?>";
}
