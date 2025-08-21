#include "parser/lexer.h"

namespace Query {

bool Lexer::isIdentStart(char c) { 
    return std::isalpha((unsigned char)c) || c=='_'; 
}

bool Lexer::isIdentCont(char c) { 
    return std::isalnum((unsigned char)c) || c=='_'; 
}

char Lexer::lower(char c) { 
    return (char)std::tolower((unsigned char)c); 
}

void Lexer::bump() { 
    if (i < sv.size()) { 
        if (sv[i] == '\n') { 
            loc.line++; 
            loc.col = 1; 
        } else { 
            loc.col++; 
        } 
        i++; 
        loc.pos++; 
    } 
}

char Lexer::peek(size_t k) const { 
    return (i + k < sv.size()) ? sv[i + k] : '\0';
}

bool Lexer::iequals(std::string_view a, std::string_view b) { 
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t k = 0; k < a.size(); k++) { 
        if (std::tolower((unsigned char)a[k]) != std::tolower((unsigned char)b[k])) {
            return false; 
        }
    } 
    return true; 
}

TokenType Lexer::parseKw(std::string_view s) {
    if (iequals(s,"select")) return TokenType::KwSelect;
    if (iequals(s,"from"))   return TokenType::KwFrom;
    if (iequals(s,"where"))  return TokenType::KwWhere;
    if (iequals(s,"insert")) return TokenType::KwInsert;
    if (iequals(s,"into"))   return TokenType::KwInto;
    if (iequals(s,"values")) return TokenType::KwValues;
    if (iequals(s,"update")) return TokenType::KwUpdate;
    if (iequals(s,"set"))    return TokenType::KwSet;
    if (iequals(s,"and"))    return TokenType::KwAnd;
    if (iequals(s,"or"))     return TokenType::KwOr;
    if (iequals(s,"not"))    return TokenType::KwNot;
    if (iequals(s,"create")) return TokenType::KwCreate;
    if (iequals(s,"table"))  return TokenType::KwTable;
    if (iequals(s,"alter"))  return TokenType::KwAlter;
    if (iequals(s,"add"))    return TokenType::KwAdd;
    if (iequals(s,"drop"))   return TokenType::KwDrop;
    if (iequals(s,"column")) return TokenType::KwColumn;
    if (iequals(s, "int16")) return TokenType::KwInt16;
    if (iequals(s, "int32")) return TokenType::KwInt32;
    if (iequals(s, "int64")) return TokenType::KwInt64;
    if (iequals(s,"double")) return TokenType::KwDouble;
    if (iequals(s,"text"))   return TokenType::KwText;
    if (iequals(s,"bool"))   return TokenType::KwBool;
    return TokenType::Ident;
}

Token Lexer::next() {
    while (std::isspace((unsigned char)peek())) {
        bump();
    }

    Token t;
    t.loc = loc;

    char c = peek();
    if (!c) {
        t.kind = TokenType::End;
        return t;
    }

    switch (c) {
        case '*': 
            bump();
            t.kind = TokenType::Star;
            return t;
        case ',':
            bump();
            t.kind = TokenType::Comma;
            return t;
        case '(': 
            bump();
            t.kind = TokenType::LParen;
            return t;
        case ')': 
            bump();
            t.kind = TokenType::RParen;
            return t;
        case '=': 
            bump();
            t.kind = TokenType::Eq;
            return t;
        case '!':
            if (peek(1) == '=') { 
                bump(); 
                bump(); 
                t.kind = TokenType::Ne; 
                return t; 
            }
            break;
        case '<':
            if (peek(1) == '=') { 
                bump(); 
                bump(); 
                t.kind = TokenType::Le; 
                return t; 
            }
            if (peek(1) == '>') { 
                bump(); 
                bump(); 
                t.kind = TokenType::Ne; 
                return t; 
            }
            bump(); 
            t.kind = TokenType::Lt; 
            return t;
        case '>':
            if (peek(1) == '=') { 
                bump(); 
                bump(); 
                t.kind = TokenType::Ge; 
                return t; 
            }
            bump(); 
            t.kind = TokenType::Gt; 
            return t;
        case '\'': {
            bump();
            std::string out;
            while (true) {
                char d = peek();
                if (!d) throw std::runtime_error("unterminated string");
                bump();
                if (d == '\'') {
                    if (peek() == '\'') { // escaped quote: ''
                        bump();
                        out.push_back('\'');
                    } else {
                        break;
                    }
                } else {
                    out.push_back(d);
                }
            }
            t.kind = TokenType::Str;
            t.text = std::move(out);
            return t;
        }
        default:
            break;
    }

    auto isDigit = [&](char ch){ return std::isdigit((unsigned char)ch); };
    bool startsUnsignedNumber = isDigit(c) || (c == '.' && isDigit(peek(1)));
    bool startsSignedNumber =
        (c == '+' || c == '-') &&
        (isDigit(peek(1)) || (peek(1) == '.' && isDigit(peek(2))));

    if (startsUnsignedNumber || startsSignedNumber) {
        size_t start = i;

        // Consume optional sign
        if (startsSignedNumber) bump();

        bool seenDot = false;

        if (peek() == '.') {
            seenDot = true;
            bump();
        }

        while (isDigit(peek())) bump();

        if (!seenDot && peek() == '.') {
            seenDot = true;
            bump();
            while (isDigit(peek())) bump();
        }

        std::string_view num = sv.substr(start, i - start);
        if (seenDot) {
            t.kind = TokenType::Float;
            t.f64 = std::strtod(std::string(num).c_str(), nullptr);
        } else {
            t.kind = TokenType::Int;
            t.i64 = std::strtoll(std::string(num).c_str(), nullptr, 10);
        }
        return t;
    }

    if (isIdentStart(c)) {
        size_t start = i; bump();
        while (isIdentCont(peek())) bump();
        std::string_view id = sv.substr(start, i - start);
        TokenType k = parseKw(id);
        t.kind = k;
        t.text = std::string(id);
        return t;
    }

    throw std::runtime_error("Unexpected character at line " + std::to_string(loc.line) +
                             ", col " + std::to_string(loc.col));
}

} // namespace Query