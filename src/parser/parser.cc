#include "parser/parser.h"

namespace Query {

Parser::Parser(std::string_view sql)
    : lex(sql) {
    cur = lex.next();
}

StmtPtr Parser::parseStatement() {
    switch (cur.kind) {
        case TokenType::KwSelect:
            return parseSelect();
        case TokenType::KwInsert:
            return parseInsert();
        case TokenType::KwUpdate:
            return parseUpdate();
        case TokenType::KwCreate:
            return parseCreateTable();
        case TokenType::KwAlter:
            return parseAlterTable();
        case TokenType::KwDrop:
            return parseDropTable();
        default:
        error("expected SELECT, INSERT, UPDATE, CREATE, ALTER, or DROP");
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
        case TokenType::KwOr: 
            return 1;
        case TokenType::KwAnd: 
            return 2;
        case TokenType::Eq: case TokenType::Ne: case TokenType::Lt:
        case TokenType::Le: case TokenType::Gt: case TokenType::Ge:
            return 3;
        default:
            return 0;
    }
}

ExprPtr Parser::parsePrimary() {
    switch (cur.kind) {
        case TokenType::Ident: {
            auto id = std::make_unique<Identifier>(cur.text); 
            cur = lex.next(); 
            return id;
        }
        case TokenType::Int: {
            auto v = std::make_unique<Literal>((int64_t)cur.i64);
            cur = lex.next();
            return v;
        }
        case TokenType::Float: {
            auto v = std::make_unique<Literal>((double)cur.f64);
            cur = lex.next();
            return v;
        }
        case TokenType::Str: {
            auto v = std::make_unique<Literal>(cur.text);
            cur = lex.next();
            return v;
        }
        default:
            if (accept(TokenType::LParen)) {
                auto e = parseExpr();
                consume(TokenType::RParen, ")");
                return e;
            }
            error("expected primary expression");
    }
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
        case TokenType::Eq:
            return Binary::Op::Eq;
        case TokenType::Ne:
            return Binary::Op::Ne;
        case TokenType::Lt:
            return Binary::Op::Lt;
        case TokenType::Le:
            return Binary::Op::Le;
        case TokenType::Gt:
            return Binary::Op::Gt;
        case TokenType::Ge:
            return Binary::Op::Ge;
        case TokenType::KwAnd:
            return Binary::Op::And;
        case TokenType::KwOr:
            return Binary::Op::Or;
        default: 
            throw std::runtime_error("Unexpected binary operator: " + std::to_string(static_cast<int>(k)));
    }
}

ExprPtr Parser::parseBinRHS(int minPrec, ExprPtr lhs){
    while (true) {
        int prec = precOf(cur.kind);
        if (prec < minPrec) {
            return lhs;
        }

        TokenType opTok = cur.kind;
        cur = lex.next();
        auto rhs = parseUnary();
        
        // right-associativity: none here; NOT is unary handled above
        int nextPrec = precOf(cur.kind);
        if (nextPrec > prec) rhs = parseBinRHS(prec + 1, std::move(rhs));
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

DataType Parser::parseDataType() {
    switch (cur.kind) {
        case TokenType::KwInt16:
            cur = lex.next();
            return DataType::Int16;
        case TokenType::KwInt32:
            cur = lex.next();
            return DataType::Int32;
        case TokenType::KwInt64:
            cur = lex.next();
            return DataType::Int64;
        case TokenType::KwDouble:
            cur = lex.next();
            return DataType::Double;
        case TokenType::KwText:
            cur = lex.next();
            return DataType::Text;
        case TokenType::KwBool:
            cur = lex.next();
            return DataType::Bool;
        default:
            error("expected data type (INT16, INT32, INT64, DOUBLE, TEXT, BOOL)");
    }
}

ColumnDef Parser::parseColumnDef() {
    if (cur.kind != TokenType::Ident) error("expected column name");
    ColumnDef c;
    c.name = cur.text;
    cur = lex.next();
    c.type = parseDataType();
    return c;
}

std::vector<ColumnDef> Parser::parseColumnDefList() {
    std::vector<ColumnDef> cols;
    cols.push_back(parseColumnDef());
    while (accept(TokenType::Comma)) {
            cols.push_back(parseColumnDef());
    }
    return cols;
}

// CREATE TABLE ...
std::unique_ptr<CreateTable> Parser::parseCreateTable() {
    consume(TokenType::KwCreate, "CREATE");
    consume(TokenType::KwTable, "TABLE");
    if (cur.kind != TokenType::Ident) error("expected table name after CREATE TABLE");
    auto ct = std::make_unique<CreateTable>();
    ct->table = cur.text;
    cur = lex.next();
    consume(TokenType::LParen, "(");
    ct->columns = parseColumnDefList();
    consume(TokenType::RParen, ")");
    return ct;
}

// DROP TABLE ...
std::unique_ptr<DropTable> Parser::parseDropTable() {
    consume(TokenType::KwDrop, "DROP");
    consume(TokenType::KwTable, "TABLE");
    if (cur.kind != TokenType::Ident) error("expected table name after DROP TABLE");
    auto dt = std::make_unique<DropTable>();
    dt->table = cur.text;
    cur = lex.next();
    return dt;
}

// ALTER TABLE ...
std::unique_ptr<AlterTable> Parser::parseAlterTable() {
    consume(TokenType::KwAlter, "ALTER");
    consume(TokenType::KwTable, "TABLE");
    if (cur.kind != TokenType::Ident) error("expected table name after ALTER TABLE");
    auto at = std::make_unique<AlterTable>();
    at->table = cur.text;
    cur = lex.next();

    // ADD COLUMN <name> <type>
    if (accept(TokenType::KwAdd)) {
        // optional COLUMN
        accept(TokenType::KwColumn);
        ColumnDef c = parseColumnDef();
        at->op = AlterTable::AddColumn{ std::move(c) };
        return at;
    }

    // DROP COLUMN <name>
    if (accept(TokenType::KwDrop)) {
        accept(TokenType::KwColumn);
        if (cur.kind != TokenType::Ident) {
            error("expected column name to drop");
        }

        std::string name = cur.text;
        cur = lex.next();
        at->op = AlterTable::DropColumn{ std::move(name) };
        return at;
    }

    // ALTER COLUMN <name> <type>
    if (accept(TokenType::KwAlter)) {
        consume(TokenType::KwColumn, "COLUMN");
        if (cur.kind != TokenType::Ident) {
            error("expected column name to alter");
        }

        std::string name = cur.text;
        cur = lex.next();
        DataType datatype = parseDataType();
        cur = lex.next();
        at->op = AlterTable::AlterColumn{ std::move(name), datatype };
        return at;
    }

    error("expected ADD, DROP, or ALTER after ALTER TABLE");
}

} // namespace Query