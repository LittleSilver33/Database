/*
    Abstract Syntax Tree (AST) definitions for a simple SQL-like language.
*/

#pragma once
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <cstdint>

// Base class for all expressions
struct Expr {
    virtual ~Expr() = default;

    std::string to_string() const;
};

using ExprPtr = std::unique_ptr<Expr>;

// Represents a literal value (e.g. 42, 3.14, "hello")
struct Literal : Expr {
    using Value = std::variant<int64_t, double, std::string>;
    Value value;
    explicit Literal(Value v) : value(std::move(v)) {}

    std::string to_string() const;
};

// Represents an identifier (e.g. column name)
struct Identifier : Expr {
    std::string name;
    explicit Identifier(std::string n) : name(std::move(n)) {}
};

// Represents a unary operation (e.g. NOT expr, -expr)
struct Unary : Expr {
    enum class Op { 
        Plus, 
        Minus, 
        Not 
    };

    Op op;
    ExprPtr rhs;
    Unary(Op o, ExprPtr r) : op(o), rhs(std::move(r)) {}
};

// Represents a binary operation (e.g. expr1 AND expr2, expr1 = expr2)
struct Binary : Expr {
    enum class Op { 
      Eq, // Equal
      Ne, // Not Equal
      Lt, // Less Than
      Le, // Less Than or Equal
      Gt, // Greater Than
      Ge, // Greater Than or Equal
      And,
      Or
    };

    Op op;
    ExprPtr lhs, rhs;
    Binary(Op o, ExprPtr l, ExprPtr r)
      : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

// Base class for all statements
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
