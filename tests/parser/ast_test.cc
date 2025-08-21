#include <gtest/gtest.h>
#include "parser/ast.h"

using namespace Query;

static std::unique_ptr<Literal> I(long long v) {
    return std::make_unique<Literal>(static_cast<int64_t>(v));
}
static std::unique_ptr<Literal> D(double v) {
    return std::make_unique<Literal>(v);
}
static std::unique_ptr<Literal> S(const std::string& v) {
    return std::make_unique<Literal>(v);
}
static std::unique_ptr<Identifier> Id(const std::string& n) {
    return std::make_unique<Identifier>(n);
}
static std::unique_ptr<Unary> U(Unary::Op op, std::unique_ptr<Expr> rhs) {
    return std::make_unique<Unary>(op, std::move(rhs));
}
static std::unique_ptr<Binary> B(Binary::Op op,
                                 std::unique_ptr<Expr> lhs,
                                 std::unique_ptr<Expr> rhs) {
    return std::make_unique<Binary>(op, std::move(lhs), std::move(rhs));
}

// -------------------------
// Literal::to_string tests
// -------------------------
TEST(AST_ToString, Literal_Int_Positive) {
    auto e = I(42);
    EXPECT_EQ(e->to_string(), "42");
}

TEST(AST_ToString, Literal_Int_Negative) {
    auto e = I(-17);
    EXPECT_EQ(e->to_string(), "-17");
}

TEST(AST_ToString, Literal_String_Simple) {
    auto e = S("hello");
    EXPECT_EQ(e->to_string(), "'hello'");
}

// ----------------------------
// Identifier::to_string tests
// ----------------------------
TEST(AST_ToString, Identifier_Simple) {
    auto e = Id("user_id");
    EXPECT_EQ(e->to_string(), "user_id");
}

// -----------------------
// Unary::to_string tests
// -----------------------
TEST(AST_ToString, Unary_Plus_OnIdentifier) {
    auto e = U(Unary::Op::Plus, Id("x"));
    EXPECT_EQ(e->to_string(), "+(x)");
}

TEST(AST_ToString, Unary_Minus_OnIdentifier) {
    auto e = U(Unary::Op::Minus, Id("x"));
    EXPECT_EQ(e->to_string(), "-(x)");
}

TEST(AST_ToString, Unary_Not_OnIdentifier) {
    auto e = U(Unary::Op::Not, Id("banned"));
    EXPECT_EQ(e->to_string(), "NOT (banned)");
}

TEST(AST_ToString, Unary_Nested_NotMinusLiteral) {
    // NOT (-(5))
    auto inner = U(Unary::Op::Minus, I(5));
    auto e = U(Unary::Op::Not, std::move(inner));
    EXPECT_EQ(e->to_string(), "NOT (-(5))");
}

TEST(AST_ToString, Unary_Plus_OnNegativeLiteral) {
    // +(-3) (unary plus applied to a negative literal)
    auto e = U(Unary::Op::Plus, I(-3));
    EXPECT_EQ(e->to_string(), "+(-3)");
}

// ------------------------
// Binary::to_string tests
// ------------------------
TEST(AST_ToString, Binary_Eq) {
    auto e = B(Binary::Op::Eq, Id("a"), I(1));
    EXPECT_EQ(e->to_string(), "(a = 1)");
}

TEST(AST_ToString, Binary_Ne_BangEq) {
    auto e = B(Binary::Op::Ne, Id("a"), I(2));
    EXPECT_EQ(e->to_string(), "(a != 2)");
}

TEST(AST_ToString, Binary_Lt) {
    auto e = B(Binary::Op::Lt, Id("age"), I(21));
    EXPECT_EQ(e->to_string(), "(age < 21)");
}

TEST(AST_ToString, Binary_Le) {
    auto e = B(Binary::Op::Le, Id("age"), I(21));
    EXPECT_EQ(e->to_string(), "(age <= 21)");
}

TEST(AST_ToString, Binary_Gt) {
    auto e = B(Binary::Op::Gt, Id("level"), I(3));
    EXPECT_EQ(e->to_string(), "(level > 3)");
}

TEST(AST_ToString, Binary_Ge) {
    auto e = B(Binary::Op::Ge, Id("level"), I(3));
    EXPECT_EQ(e->to_string(), "(level >= 3)");
}

TEST(AST_ToString, Binary_WithStringLiteral) {
    auto e = B(Binary::Op::Eq, Id("name"), S("Alice"));
    EXPECT_EQ(e->to_string(), "(name = 'Alice')");
}

// ---------------------------------------------
// Complex combinations / parentheses & precedence
// ---------------------------------------------
TEST(AST_ToString, And_Or_PrecedenceShape) {
    // (a = 1) OR ( (b = 2) AND NOT (c) )
    auto andPart = B(Binary::Op::And,
                     B(Binary::Op::Eq, Id("b"), I(2)),
                     U(Unary::Op::Not, Id("c")));
    auto root = B(Binary::Op::Or,
                  B(Binary::Op::Eq, Id("a"), I(1)),
                  std::move(andPart));
    EXPECT_EQ(root->to_string(), "((a = 1) OR ((b = 2) AND NOT (c)))");
}

TEST(AST_ToString, ParenthesesOverride) {
    // ((x = 1) OR (y = 2)) AND z
    auto orPart = B(Binary::Op::Or,
                    B(Binary::Op::Eq, Id("x"), I(1)),
                    B(Binary::Op::Eq, Id("y"), I(2)));
    auto root = B(Binary::Op::And, std::move(orPart), Id("z"));
    EXPECT_EQ(root->to_string(), "(((x = 1) OR (y = 2)) AND z)");
}

TEST(AST_ToString, DeepNest_AllOps) {
    // ((x < 10 AND y >= 2) OR (name != 'bob')) AND NOT (z = 0)
    auto conj = B(Binary::Op::And,
                  B(Binary::Op::Lt, Id("x"), I(10)),
                  B(Binary::Op::Ge, Id("y"), I(2)));
    auto disj = B(Binary::Op::Or,
                  std::move(conj),
                  B(Binary::Op::Ne, Id("name"), S("bob")));
    auto root = B(Binary::Op::And,
                  std::move(disj),
                  U(Unary::Op::Not, B(Binary::Op::Eq, Id("z"), I(0))));
    
    EXPECT_EQ(root->to_string(),
              "((((x < 10) AND (y >= 2)) OR (name != 'bob')) AND NOT ((z = 0)))");
}

// Ensure Identifier and Literal compose as children cleanly
TEST(AST_ToString, Compose_UnaryInsideBinary) {
    // (-(x) = -5)
    auto left = U(Unary::Op::Minus, Id("x"));
    auto right = I(-5);
    auto root = B(Binary::Op::Eq, std::move(left), std::move(right));
    EXPECT_EQ(root->to_string(), "(-(x) = -5)");
}