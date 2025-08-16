#include <gtest/gtest.h>
#include <cmath>
#include "parser/ast.h"
#include "parser/parser.h"

using ::testing::Test;

class ParserTest : public Test {
protected:
    // ---- Safe downcasts ----
    static const Identifier* AsIdent(const Expr* e)   { return dynamic_cast<const Identifier*>(e); }
    static const Literal*    AsLiteral(const Expr* e) { return dynamic_cast<const Literal*>(e); }
    static const Unary*      AsUnary(const Expr* e)   { return dynamic_cast<const Unary*>(e); }
    static const Binary*     AsBinary(const Expr* e)  { return dynamic_cast<const Binary*>(e); }

    // ---- Leaf checks ----
    static ::testing::AssertionResult IsIdent(const Expr* e, std::string_view name) {
        auto id = AsIdent(e);
        if (!id) return ::testing::AssertionFailure() << "Expr is not Identifier";
        if (id->name != name) {
            return ::testing::AssertionFailure()
                   << "Identifier mismatch. got='" << id->name << "' expected='" << name << "'";
        }
        return ::testing::AssertionSuccess();
    }

    static ::testing::AssertionResult IsInt(const Expr* e, int64_t v) {
        auto lit = AsLiteral(e);
        if (!lit) return ::testing::AssertionFailure() << "Expr is not Literal";
        if (!std::holds_alternative<int64_t>(lit->value))
            return ::testing::AssertionFailure() << "Literal is not int64_t";
        auto got = std::get<int64_t>(lit->value);
        if (got != v)
            return ::testing::AssertionFailure() << "Int literal mismatch. got=" << got
                                                 << " expected=" << v;
        return ::testing::AssertionSuccess();
    }

    static ::testing::AssertionResult IsDouble(const Expr* e, double v, double eps=1e-9) {
        auto lit = AsLiteral(e);
        if (!lit) return ::testing::AssertionFailure() << "Expr is not Literal";
        if (!std::holds_alternative<double>(lit->value))
            return ::testing::AssertionFailure() << "Literal is not double";
        auto got = std::get<double>(lit->value);
        if (std::fabs(got - v) > eps)
            return ::testing::AssertionFailure() << "Double literal mismatch. got=" << got
                                                 << " expected=" << v;
        return ::testing::AssertionSuccess();
    }

    static ::testing::AssertionResult IsString(const Expr* e, std::string_view v) {
        auto lit = AsLiteral(e);
        if (!lit) return ::testing::AssertionFailure() << "Expr is not Literal";
        if (!std::holds_alternative<std::string>(lit->value))
            return ::testing::AssertionFailure() << "Literal is not string";
        auto& got = std::get<std::string>(lit->value);
        if (got != v)
            return ::testing::AssertionFailure() << "String literal mismatch. got='" << got
                                                 << "' expected='" << v << "'";
        return ::testing::AssertionSuccess();
    }

    // ---- Structural checks ----
    static ::testing::AssertionResult IsUnaryOp(
        const Expr* e, Unary::Op op,
        const std::function<::testing::AssertionResult(const Expr*)>& rhsCheck) {
        auto u = AsUnary(e);
        if (!u) return ::testing::AssertionFailure() << "Expr is not Unary";
        if (u->op != op) return ::testing::AssertionFailure() << "Unary op mismatch";
        return rhsCheck(u->rhs.get());
    }

    static ::testing::AssertionResult IsBinaryOp(
        const Expr* e, Binary::Op op,
        const std::function<::testing::AssertionResult(const Expr*)>& lhsCheck,
        const std::function<::testing::AssertionResult(const Expr*)>& rhsCheck) {
        auto b = AsBinary(e);
        if (!b) return ::testing::AssertionFailure() << "Expr is not Binary";
        if (b->op != op) return ::testing::AssertionFailure() << "Binary op mismatch";
        auto l = lhsCheck(b->lhs.get());
        if (!l) return l;
        return rhsCheck(b->rhs.get());
    }

    // ---- Combinators for readable trees ----
    static auto Ident(std::string name) {
        return [n = std::move(name)](const Expr* e){ return IsIdent(e, n); };
    }
    static auto Int(int64_t v) {
        return [v](const Expr* e){ return IsInt(e, v); };
    }
    static auto Dbl(double v, double eps=1e-9) {
        return [v, eps](const Expr* e){ return IsDouble(e, v, eps); };
    }
    static auto Str(std::string s) {
        return [s = std::move(s)](const Expr* e){ return IsString(e, s); };
    }
    static auto Un(Unary::Op op, std::function<::testing::AssertionResult(const Expr*)> rhs) {
        return [op, rhs = std::move(rhs)](const Expr* e){ return IsUnaryOp(e, op, rhs); };
    }
    static auto Bin(Binary::Op op,
                    std::function<::testing::AssertionResult(const Expr*)> lhs,
                    std::function<::testing::AssertionResult(const Expr*)> rhs) {
        return [op, lhs = std::move(lhs), rhs = std::move(rhs)](const Expr* e){
            return IsBinaryOp(e, op, lhs, rhs);
        };
    }

    // ---- Parse helpers (transfer ownership cleanly) ----
    static std::unique_ptr<Select> ParseSelect(std::string_view sql) {
        Parser p(sql);
        StmtPtr s = p.parseStatement();
        auto* sel = dynamic_cast<Select*>(s.get());
        if (!sel) return nullptr;
        s.release();
        return std::unique_ptr<Select>(sel);
    }
    static std::unique_ptr<Insert> ParseInsert(std::string_view sql) {
        Parser p(sql);
        StmtPtr s = p.parseStatement();
        auto* in = dynamic_cast<Insert*>(s.get());
        if (!in) return nullptr;
        s.release();
        return std::unique_ptr<Insert>(in);
    }
    static std::unique_ptr<Update> ParseUpdate(std::string_view sql) {
        Parser p(sql);
        StmtPtr s = p.parseStatement();
        auto* up = dynamic_cast<Update*>(s.get());
        if (!up) return nullptr;
        s.release();
        return std::unique_ptr<Update>(up);
    }
};

// ---------------------------------
// SELECT: columns, star, whitespace
// ---------------------------------
TEST_F(ParserTest, Select_ColumnsAndTable_Basics) {
    auto sel = ParseSelect("SELECT id, name FROM users");
    ASSERT_TRUE(sel);
    EXPECT_FALSE(sel->select_all);
    ASSERT_EQ(sel->columns.size(), 2u);
    EXPECT_EQ(sel->columns[0], "id");
    EXPECT_EQ(sel->columns[1], "name");
    EXPECT_EQ(sel->table, "users");
    EXPECT_FALSE(sel->where.has_value());
}

TEST_F(ParserTest, Select_Star_IgnoresColumns) {
    auto sel = ParseSelect("SELECT   *   FROM   t  ");
    ASSERT_TRUE(sel);
    EXPECT_TRUE(sel->select_all);
    EXPECT_TRUE(sel->columns.empty());
    EXPECT_EQ(sel->table, "t");
}

TEST_F(ParserTest, Select_CaseInsensitiveKeywords_AndNewlines) {
    auto sel = ParseSelect("SeLeCt\n*\nFrOm\nA\nwHeRe\nNoT\tactive");
    ASSERT_TRUE(sel);
    EXPECT_EQ(sel->table, "A");
    ASSERT_TRUE(sel->where.has_value());
    auto check = Un(Unary::Op::Not, Ident("active"));
    EXPECT_TRUE(check(sel->where->get()));
}

// --------------------
// WHERE: every operator
// --------------------
TEST_F(ParserTest, Where_Eq) {
    auto sel = ParseSelect("SELECT * FROM a WHERE x = 1");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Eq, Ident("x"), Int(1));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_Ne) {
    auto sel = ParseSelect("SELECT * FROM a WHERE x != 2");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Ne, Ident("x"), Int(2));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_Lt) {
    auto sel = ParseSelect("SELECT * FROM a WHERE age < 21");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Lt, Ident("age"), Int(21));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_Le) {
    auto sel = ParseSelect("SELECT * FROM a WHERE age <= 21");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Le, Ident("age"), Int(21));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_Gt) {
    auto sel = ParseSelect("SELECT * FROM a WHERE level > 3");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Gt, Ident("level"), Int(3));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_Ge) {
    auto sel = ParseSelect("SELECT * FROM a WHERE level >= 3");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Ge, Ident("level"), Int(3));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_StringLiteral) {
    auto sel = ParseSelect("SELECT * FROM a WHERE name = 'Alice'");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Eq, Ident("name"), Str("Alice"));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_FloatLiteral) {
    auto sel = ParseSelect("SELECT * FROM a WHERE price >= 12.5");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Ge, Ident("price"), Dbl(12.5));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Where_NegativeIntLiteral) {
    // With +/− tokens commented out in lexer, negative numbers should come as signed literals.
    auto sel = ParseSelect("SELECT * FROM a WHERE delta = -42");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::Eq, Ident("delta"), Int(-42));
    EXPECT_TRUE(check(sel->where->get()));
}

// -----------------------------
// Logical precedence & grouping
// -----------------------------
TEST_F(ParserTest, Logical_AND_BindsTighterThan_OR) {
    auto sel = ParseSelect("SELECT * FROM t WHERE a = 1 OR b = 2 AND NOT c");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    // Expect: (a=1) OR ( (b=2) AND (NOT c) )
    auto check = Bin(Binary::Op::Or,
                     Bin(Binary::Op::Eq, Ident("a"), Int(1)),
                     Bin(Binary::Op::And,
                         Bin(Binary::Op::Eq, Ident("b"), Int(2)),
                         Un(Unary::Op::Not, Ident("c"))));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Logical_Parentheses_Override) {
    auto sel = ParseSelect("SELECT * FROM t WHERE (a = 1 OR b = 2) AND c");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::And,
                     Bin(Binary::Op::Or,
                         Bin(Binary::Op::Eq, Ident("a"), Int(1)),
                         Bin(Binary::Op::Eq, Ident("b"), Int(2))),
                     Ident("c"));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Logical_SimpleNOT) {
    auto sel = ParseSelect("SELECT * FROM t WHERE NOT enabled");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Un(Unary::Op::Not, Ident("enabled"));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Logical_DoubleNOT_WithGrouping) {
    auto sel = ParseSelect("SELECT * FROM t WHERE NOT (NOT flagged)");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Un(Unary::Op::Not, Un(Unary::Op::Not, Ident("flagged")));
    EXPECT_TRUE(check(sel->where->get()));
}

TEST_F(ParserTest, Logical_DeepNest_AllOperators) {
    auto sel = ParseSelect(
        "SELECT * FROM t WHERE ((x < 10 AND y >= 2) OR (name != 'bob')) AND NOT (z = 0)");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto leftConj = Bin(Binary::Op::And,
                        Bin(Binary::Op::Lt, Ident("x"), Int(10)),
                        Bin(Binary::Op::Ge, Ident("y"), Int(2)));
    auto leftDisj = Bin(Binary::Op::Or,
                        leftConj,
                        Bin(Binary::Op::Ne, Ident("name"), Str("bob")));
    auto check = Bin(Binary::Op::And,
                     leftDisj,
                     Un(Unary::Op::Not, Bin(Binary::Op::Eq, Ident("z"), Int(0))));
    EXPECT_TRUE(check(sel->where->get()));
}

// ---------------------------------------------
// INSERT: explicit / implicit & literal variety
// ---------------------------------------------
TEST_F(ParserTest, Insert_ExplicitColumns_MixedLiterals) {
    auto ins = ParseInsert("INSERT INTO users (id, name, score, ratio) VALUES (1, 'bob', -3, 2.5)");
    ASSERT_TRUE(ins);
    EXPECT_EQ(ins->table, "users");
    ASSERT_EQ(ins->columns.size(), 4u);
    EXPECT_EQ(ins->columns[0], "id");
    EXPECT_EQ(ins->columns[1], "name");
    EXPECT_EQ(ins->columns[2], "score");
    EXPECT_EQ(ins->columns[3], "ratio");
    ASSERT_EQ(ins->values.size(), 4u);
    EXPECT_TRUE(IsInt(ins->values[0].get(), 1));
    EXPECT_TRUE(IsString(ins->values[1].get(), "bob"));
    EXPECT_TRUE(IsInt(ins->values[2].get(), -3));
    EXPECT_TRUE(IsDouble(ins->values[3].get(), 2.5));
}

TEST_F(ParserTest, Insert_ImplicitColumns_ThreeValues) {
    auto ins = ParseInsert("INSERT INTO users VALUES (1, 2, 3)");
    ASSERT_TRUE(ins);
    EXPECT_EQ(ins->table, "users");
    EXPECT_TRUE(ins->columns.empty());
    ASSERT_EQ(ins->values.size(), 3u);
    EXPECT_TRUE(IsInt(ins->values[0].get(), 1));
    EXPECT_TRUE(IsInt(ins->values[1].get(), 2));
    EXPECT_TRUE(IsInt(ins->values[2].get(), 3));
}

TEST_F(ParserTest, Insert_Whitespace_Newlines_Case) {
    auto ins = ParseInsert("insert  into  T\n(v)\nvalues\n('hi')");
    ASSERT_TRUE(ins);
    EXPECT_EQ(ins->table, "T");
    ASSERT_EQ(ins->columns.size(), 1u);
    EXPECT_EQ(ins->columns[0], "v");
    ASSERT_EQ(ins->values.size(), 1u);
    EXPECT_TRUE(IsString(ins->values[0].get(), "hi"));
}

// ------------------------------
// UPDATE: assignments and WHEREs
// ------------------------------
TEST_F(ParserTest, Update_SingleAssignment_NoWhere) {
    auto up = ParseUpdate("UPDATE users SET score = 99");
    ASSERT_TRUE(up);
    EXPECT_EQ(up->table, "users");
    ASSERT_EQ(up->assignments.size(), 1u);
    EXPECT_EQ(up->assignments[0].first, "score");
    EXPECT_TRUE(IsInt(up->assignments[0].second.get(), 99));
    EXPECT_FALSE(up->where.has_value());
}

TEST_F(ParserTest, Update_MultiAssignment_WithEqGeStringWhere) {
    auto up = ParseUpdate("UPDATE users SET age = 30, name = 'Alice' WHERE id >= 7");
    ASSERT_TRUE(up);
    EXPECT_EQ(up->table, "users");
    ASSERT_EQ(up->assignments.size(), 2u);
    EXPECT_EQ(up->assignments[0].first, "age");
    EXPECT_TRUE(IsInt(up->assignments[0].second.get(), 30));
    EXPECT_EQ(up->assignments[1].first, "name");
    EXPECT_TRUE(IsString(up->assignments[1].second.get(), "Alice"));
    ASSERT_TRUE(up->where.has_value());
    auto check = Bin(Binary::Op::Ge, Ident("id"), Int(7));
    EXPECT_TRUE(check(up->where->get()));
}

TEST_F(ParserTest, Update_WithComplexWhere_AllLogicalOperators) {
    auto up = ParseUpdate("UPDATE items SET qty = 0 WHERE (sku = 'A' OR sku = 'B') AND NOT discontinued");
    ASSERT_TRUE(up);
    ASSERT_TRUE(up->where.has_value());
    auto whereCheck = Bin(Binary::Op::And,
                          Bin(Binary::Op::Or,
                              Bin(Binary::Op::Eq, Ident("sku"), Str("A")),
                              Bin(Binary::Op::Eq, Ident("sku"), Str("B"))),
                          Un(Unary::Op::Not, Ident("discontinued")));
    EXPECT_TRUE(whereCheck(up->where->get()));
}

// ----------------------
// Additional edge cases
// ----------------------
TEST_F(ParserTest, Select_ParensAroundLeaf) {
    auto sel = ParseSelect("SELECT * FROM t WHERE (enabled)");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    EXPECT_TRUE(IsIdent(sel->where->get(), "enabled"));
}

TEST_F(ParserTest, Select_SpacingAroundOps) {
    auto sel = ParseSelect("SELECT * FROM t WHERE a   <=   5   AND   b   !=   'x'");
    ASSERT_TRUE(sel); ASSERT_TRUE(sel->where.has_value());
    auto check = Bin(Binary::Op::And,
                     Bin(Binary::Op::Le, Ident("a"), Int(5)),
                     Bin(Binary::Op::Ne, Ident("b"), Str("x")));
    EXPECT_TRUE(check(sel->where->get()));
}

// ----------------------
// Negative/error cases
// ----------------------
TEST_F(ParserTest, Error_Select_MissingFrom) {
    Parser p("SELECT id WHERE x = 1");
    EXPECT_THROW({ (void)p.parseStatement(); }, std::runtime_error);
}

TEST_F(ParserTest, Error_Insert_MissingInto) {
    Parser p("INSERT users (id) VALUES (1)");
    EXPECT_THROW({ (void)p.parseStatement(); }, std::runtime_error);
}

TEST_F(ParserTest, Error_Update_MissingSet) {
    Parser p("UPDATE users name = 'x' WHERE id = 1"); // no SET
    EXPECT_THROW({ (void)p.parseStatement(); }, std::runtime_error);
}

TEST_F(ParserTest, Error_Where_EmptyAfterKeyword) {
    Parser p("SELECT * FROM t WHERE");
    EXPECT_THROW({ (void)p.parseStatement(); }, std::runtime_error);
}
