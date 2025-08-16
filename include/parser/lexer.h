#pragma once
#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>

struct Loc { size_t pos=0, line=1, col=1; };

enum class TokenType {
    End,
    // Keywords
    KwSelect, KwFrom, KwWhere, KwInsert, KwInto, KwValues, KwUpdate, KwSet, KwAnd, KwOr, KwNot,
    // Identifiers & literals
    Ident, Int, Float, Str,
    // Punctuation / ops
    Star, Comma, LParen, RParen,
    Eq, Ne, Lt, Le, Gt, Ge,
    // Math ops
    Plus, Minus, 
    // Mul, Div
};

struct Token {
    TokenType kind{TokenType::End};
    std::string text; // raw spelling (for Ident/Str)
    int64_t i64{}; double f64{}; // literal storage
    Loc loc{};
};

class Lexer {
public:
    explicit Lexer(std::string_view input): sv(input) {}

    Token next();

private:
    std::string_view sv; 
    size_t i = 0; 
    Loc loc;

    static bool isIdentStart(char c);
    static bool isIdentCont(char c);
    static char lower(char c);
    void bump();
    char peek(size_t k=0) const;
    static bool ieq(std::string_view a, std::string_view b);
    TokenType parseKw(std::string_view s);
};
