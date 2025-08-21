#include "lexer.h"
#include "ast.h"
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <iostream>

namespace Query {

class Parser {
public:
    explicit Parser(std::string_view sql);

    // Parse a complete SQL statement into AST
    StmtPtr parseStatement();

private:
    [[noreturn]] void error(const std::string& msg);

    void consume(TokenType k, const char* what);
    bool accept(TokenType k);

    // precedence: lowest .. highest
    static int precOf(TokenType k);
    ExprPtr parsePrimary();
    ExprPtr parseUnary();
    static Binary::Op binOpFrom(TokenType k);
    ExprPtr parseBinRHS(int minPrec, ExprPtr lhs);
    ExprPtr parseExpr();
    std::vector<std::string> parseIdentList();
    std::vector<ExprPtr> parseExprList();
    std::optional<ExprPtr> parseOptWhere();
    std::unique_ptr<Select> parseSelect();
    std::unique_ptr<Insert> parseInsert();
    std::vector<std::pair<std::string, ExprPtr>> parseAssignments();
    std::unique_ptr<Update> parseUpdate();

    DataType parseDataType();
    ColumnDef parseColumnDef();
    std::vector<ColumnDef> parseColumnDefList();

    std::unique_ptr<CreateTable> parseCreateTable();
    std::unique_ptr<AlterTable>  parseAlterTable();
    std::unique_ptr<DropTable>   parseDropTable();

private:
    Lexer lex; 
    Token cur;
};

} // namespace Query