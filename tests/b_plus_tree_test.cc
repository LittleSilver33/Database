#include <cassert>
#include <iostream>
#include <gtest/gtest.h>
#include "b_plus_tree.h"
#include <memory>

class BPlusTreeTest : public ::testing::Test {
protected:
    static constexpr int ORDER = 4;
    std::string filename = "Test_Database.db";
    std::unique_ptr<BPlusTree<int, int>> tree;

    void ClearDatabase() {
        // Remove the old file if it exists
        std::remove(filename.c_str());

        // Recreate a fresh BPlusTree
        tree = std::make_unique<BPlusTree<int, int>>(filename, ORDER);
    }

    void SetUp() override {
        ClearDatabase();
    }

    void TearDown() override {
        // Optionally cleanup at the end of each test
        tree.reset();
        std::remove(filename.c_str());
    }
};

TEST_F(BPlusTreeTest, InsertionNoSplit) {
    // Insert fewer than ORDER keys so leaf doesn't split (max keys = order - 1 = 3)
    tree->Insert(10, 100);
    tree->Insert(20, 200);
    tree->Insert(30, 300);

    // Read root page
    std::vector<std::byte> page;
    ASSERT_TRUE(tree->DebugReadPage(tree->DebugRootPage(), page));

    // Check node type byte
    ASSERT_EQ(static_cast<int>(page[0]), static_cast<int>(NodeType::Leaf));

    // Deserialize and validate contents
    LeafNode<int, int> node;
    tree->DebugDeserializeLeaf(page, node);

    ASSERT_EQ(node.keys_.size(), 3u);
    EXPECT_EQ(node.keys_[0], 10);
    EXPECT_EQ(node.keys_[1], 20);
    EXPECT_EQ(node.keys_[2], 30);

    ASSERT_EQ(node.values_.size(), 3u);
    ASSERT_EQ(node.values_[0].size(), 1u);
    ASSERT_EQ(node.values_[1].size(), 1u);
    ASSERT_EQ(node.values_[2].size(), 1u);

    EXPECT_EQ(node.values_[0][0], 100);
    EXPECT_EQ(node.values_[1][0], 200);
    EXPECT_EQ(node.values_[2][0], 300);

    // Next leaf pointer should be 0 (none)
    EXPECT_EQ(node.next_leaf_page_, 0u);
}

TEST_F(BPlusTreeTest, InsertionWithSplit) {
    // Insert ORDER (4) or more keys → triggers split
    tree->Insert(10, 100);
    tree->Insert(20, 200);
    tree->Insert(30, 300);
    tree->Insert(40, 400); // this should cause a split

    // Read root page
    std::vector<std::byte> root_page;
    ASSERT_TRUE(tree->DebugReadPage(tree->DebugRootPage(), root_page));

    // Root should now be internal
    ASSERT_EQ(static_cast<int>(root_page[0]), static_cast<int>(NodeType::Internal));

    // Deserialize root
    InternalNode<int> root;
    tree->DebugDeserializeInternal(root_page, root);

    ASSERT_EQ(root.keys_.size(), 1u);      // One separator key
    ASSERT_EQ(root.children_.size(), 2u);  // Two children

    // Left child
    std::vector<std::byte> left_page;
    ASSERT_TRUE(tree->DebugReadPage(root.children_[0], left_page));
    LeafNode<int, int> left;
    tree->DebugDeserializeLeaf(left_page, left);

    // Right child
    std::vector<std::byte> right_page;
    ASSERT_TRUE(tree->DebugReadPage(root.children_[1], right_page));
    LeafNode<int, int> right;
    tree->DebugDeserializeLeaf(right_page, right);

    // Verify keys distributed
    EXPECT_EQ(left.keys_[0], 10);
    EXPECT_EQ(left.keys_[1], 20);

    EXPECT_EQ(right.keys_[0], 30);
    EXPECT_EQ(right.keys_[1], 40);

    // Check values
    EXPECT_EQ(left.values_[0][0], 100);
    EXPECT_EQ(left.values_[1][0], 200);
    EXPECT_EQ(right.values_[0][0], 300);
    EXPECT_EQ(right.values_[1][0], 400);

    // Check leaf link
    EXPECT_EQ(left.next_leaf_page_, root.children_[1]);
    EXPECT_EQ(right.next_leaf_page_, 0u);
    tree->DebugPrintTree();
}
