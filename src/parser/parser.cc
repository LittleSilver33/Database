#include "parser/parser.h"

Parser::Parser(std::string_view sql)
    : lex(sql)
{
    cur = lex.next();
}

StmtPtr Parser::parseStatement() {
    switch (cur.kind) {
        case TokenType::KwSelect: return parseSelect();
        case TokenType::KwInsert: return parseInsert();
        case TokenType::KwUpdate: return parseUpdate();
        default: error("expected SELECT, INSERT, or UPDATE");
    }
}

[[noreturn]] void Parser::error(const std::string& msg) {
    throw std::runtime_error("Parse error at line " + std::to_string(cur.loc.line) + ", col " + std::to_string(cur.loc.col) + ": " + msg);
}

void Parser::consume(TokenType k, const char* what) { 
    if (cur.kind != k) {
        error(std::string("expected ") + what); 
    }
    cur = lex.next();
}

bool Parser::accept(TokenType k) {
    if (cur.kind == k) {
        cur = lex.next();
        return true;
    }
    return false;
}

int Parser::precOf(TokenType k) {
    switch (k) {
        case TokenType::KwOr: return 1;
        case TokenType::KwAnd: return 2;
        case TokenType::Eq: case TokenType::Ne: case TokenType::Lt: case TokenType::Le: case TokenType::Gt: case TokenType::Ge: return 3;
        default: return 0;
    }
}

ExprPtr Parser::parsePrimary(){
    if (cur.kind==TokenType::Ident) { 
        auto id = std::make_unique<Identifier>(cur.text); 
        cur = lex.next(); 
        return id; 
    }
    if (cur.kind == TokenType::Int) {
        auto v = std::make_unique<Literal>((int64_t)cur.i64);
        cur = lex.next();
        return v;
    }
    if (cur.kind == TokenType::Float) {
        auto v = std::make_unique<Literal>((double)cur.f64);
        cur = lex.next();
        return v;
    }
    if (cur.kind == TokenType::Str) {
        auto v = std::make_unique<Literal>(cur.text);
        cur = lex.next();
        return v;
    }
    if (accept(TokenType::LParen)) {
        auto e = parseExpr();
        consume(TokenType::RParen, ")");
        return e;
    }
    error("expected primary expression");
}

ExprPtr Parser::parseUnary() {
    if (accept(TokenType::KwNot))
        return std::make_unique<Unary>(Unary::Op::Not, parseUnary());
    if (accept(TokenType::Minus))
        return std::make_unique<Unary>(Unary::Op::Minus, parseUnary());
    if (accept(TokenType::Plus))
        return std::make_unique<Unary>(Unary::Op::Plus, parseUnary());
    return parsePrimary();
}

Binary::Op Parser::binOpFrom(TokenType k) {
    switch (k) {
        case TokenType::Eq: return Binary::Op::Eq;
        case TokenType::Ne: return Binary::Op::Ne;
        case TokenType::Lt: return Binary::Op::Lt;
        case TokenType::Le: return Binary::Op::Le;
        case TokenType::Gt: return Binary::Op::Gt;
        case TokenType::Ge: return Binary::Op::Ge;
        case TokenType::KwAnd: return Binary::Op::And;
        case TokenType::KwOr: return Binary::Op::Or;
        default: 
            throw std::runtime_error("Unexpected binary operator: " + std::to_string(static_cast<int>(k)));
    }
}

ExprPtr Parser::parseBinRHS(int minPrec, ExprPtr lhs){
    while (true){
    int prec = precOf(cur.kind);
    if (prec < minPrec) {
        return lhs;
    }
    TokenType opTok = cur.kind; cur = lex.next();
    auto rhs = parseUnary();
    // right-associativity: none here; NOT is unary handled above
    int nextPrec = precOf(cur.kind);
    if (nextPrec > prec) rhs = parseBinRHS(prec+1, std::move(rhs));
    lhs = std::make_unique<Binary>(binOpFrom(opTok), std::move(lhs), std::move(rhs));
    }
}

ExprPtr Parser::parseExpr() {
    auto lhs = parseUnary();
    return parseBinRHS(1, std::move(lhs));
}

std::vector<std::string> Parser::parseIdentList() {
    std::vector<std::string> out;
    if (cur.kind != TokenType::Ident) error("expected identifier");
    out.push_back(cur.text);
    cur = lex.next();
    while (accept(TokenType::Comma)) {
        if (cur.kind != TokenType::Ident) error("expected identifier after ','");
        out.push_back(cur.text);
        cur = lex.next();
    }
    return out;
}

std::vector<ExprPtr> Parser::parseExprList() {
    std::vector<ExprPtr> out;
    out.push_back(parseExpr());
    while (accept(TokenType::Comma)) {
        out.push_back(parseExpr());
    }
    return out;
}

std::optional<ExprPtr> Parser::parseOptWhere() {
    if (accept(TokenType::KwWhere)) return std::optional<ExprPtr>(parseExpr());
    return std::nullopt;
}

std::unique_ptr<Select> Parser::parseSelect() {
    consume(TokenType::KwSelect, "SELECT");

    auto sel = std::make_unique<Select>();
    if (accept(TokenType::Star)) {
        sel->select_all = true;
    } else {
        sel->columns = parseIdentList();
    }

    consume(TokenType::KwFrom, "FROM");

    if (cur.kind != TokenType::Ident) {
        error("expected table name after FROM");
    }

    sel->table = cur.text;
    cur = lex.next();

    if (auto w = parseOptWhere()) {
        sel->where = std::move(*w);
    }

    return sel;
}

std::unique_ptr<Insert> Parser::parseInsert() {
    consume(TokenType::KwInsert, "INSERT");
    consume(TokenType::KwInto, "INTO");
    if (cur.kind != TokenType::Ident) {
        error("expected table name after INTO");
    }
    auto ins = std::make_unique<Insert>();
    ins->table = cur.text;
    cur = lex.next();
    if (accept(TokenType::LParen)) {
        ins->columns = parseIdentList();
        consume(TokenType::RParen, ")");
    }
    consume(TokenType::KwValues, "VALUES");
    consume(TokenType::LParen, "(");
    ins->values = parseExprList();
    consume(TokenType::RParen, ")");
    return ins;
}

std::vector<std::pair<std::string, ExprPtr>> Parser::parseAssignments() {
    std::vector<std::pair<std::string, ExprPtr>> out;

    if (cur.kind != TokenType::Ident) {
        error("expected column name in SET");
    }
    std::string col = cur.text;
    cur = lex.next();
    consume(TokenType::Eq, "=");
    auto e = parseExpr();
    out.emplace_back(std::move(col), std::move(e));

    while (accept(TokenType::Comma)) {
        if (cur.kind != TokenType::Ident) {
            error("expected column name in SET");
        }
        col = cur.text;
        cur = lex.next();
        consume(TokenType::Eq, "=");
        e = parseExpr();
        out.emplace_back(std::move(col), std::move(e));
    }
    return out;
}

std::unique_ptr<Update> Parser::parseUpdate() {
    consume(TokenType::KwUpdate, "UPDATE");
    if (cur.kind != TokenType::Ident) {
        error("expected table name after UPDATE");
    }
    auto up = std::make_unique<Update>();
    up->table = cur.text;
    cur = lex.next();
    consume(TokenType::KwSet, "SET");
    up->assignments = parseAssignments();
    if (auto w = parseOptWhere()) {
        up->where = std::move(*w);
    }
    return up;
}