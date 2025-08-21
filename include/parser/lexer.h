#pragma once
#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace Query {

struct Loc { 
    size_t pos = 0;
    size_t line = 1;
    size_t col = 1;
};

enum class TokenType {
    End,
    // Keywords
    KwSelect, KwFrom, KwWhere, KwInsert, KwInto, KwValues, KwUpdate, KwSet, KwAnd, KwOr, KwNot,
    // DDL
    KwCreate, KwTable, KwAlter, KwAdd, KwDrop, KwColumn,
    // Simple data types
    KwInt16, KwInt32, KwInt64, KwDouble, KwText, KwBool,
    // Identifiers & literals
    Ident, Int, Float, Str,
    // Punctuation / ops
    Star, Comma, LParen, RParen,
    Eq, Ne, Lt, Le, Gt, Ge,
    // Unary ops
    Plus, Minus,
};

struct Token {
    TokenType kind{TokenType::End};
    std::string text; // raw spelling (for Ident/Str)

    // Literal storage
    int64_t i64{}; 
    double f64{};

    Loc loc{};
};

class Lexer {
public:
    explicit Lexer(std::string_view input): sv(input) {}

    // Get the next token
    Token next();

private:
    // Check if a character can start an identifier
    static bool isIdentStart(char c);

    // Check if a character can be part of an identifier
    static bool isIdentCont(char c);

    // Convert a character to lowercase
    static char lower(char c);

    // Consume whitespace and update location
    void bump();

    // Peek at the next character without consuming it
    char peek(size_t k = 0) const;

    // Case-insensitive string comparison
    static bool iequals(std::string_view a, std::string_view b);

    // Parse a keyword from an identifier
    TokenType parseKw(std::string_view s);

private:
    std::string_view sv;
    size_t i = 0;
    Loc loc{};
};

} // namespace Query