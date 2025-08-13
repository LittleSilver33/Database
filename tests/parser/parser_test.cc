#include "parser/parser.h"
#include <iostream>
#include <gtest/gtest.h>

TEST(ParserTest, SelectStatement) {
    Parser p("SELECT id, name FROM users WHERE age >= 18 AND NOT banned");
    auto stmt = p.parseStatement();
    ASSERT_TRUE(stmt);
    
    auto select = dynamic_cast<Select*>(stmt.get());
    ASSERT_TRUE(select);
    
    EXPECT_EQ(select->table, "users");
    EXPECT_FALSE(select->select_all);
    EXPECT_EQ(select->columns.size(), 2);
    EXPECT_EQ(select->columns[0], "id");
    EXPECT_EQ(select->columns[1], "name");
    
    ASSERT_TRUE(select->where.has_value());
    ASSERT_TRUE(*select->where);
    
    std::string where_str = to_string(**select->where);
    EXPECT_EQ(where_str, "((age >= 18) AND NOT (banned))");
}
