#include <cassert>
#include <iostream>
#include "../hdrs/b_plus_tree.h"

class BPlusTreeTest {
public:
    static void TestInsertionNoSplit() {
        constexpr int ORDER = 4;
        BPlusTree<int, std::string> tree("Test_Database.db", ORDER);

        // Insert keys fewer than order to avoid splitting.
        tree.Insert(10, "ten");
        tree.Insert(20, "twenty");
        tree.Insert(30, "thirty");

        // Root page should be a leaf node.
        std::vector<std::byte> page(4096);
        bool ok = tree.os_.ReadPage(tree.root_page_, page);
        assert(ok);

        // Check node type
        assert(static_cast<int>(page[0]) == static_cast<int>(NodeType::Leaf));

        // Deserialize to check contents
        LeafNode<int, std::string> node;
        tree.DeserializeLeaf(page, node);

        // Check keys count
        assert(node.keys_.size() == 3);

        // Check keys order
        assert(node.keys_[0] == 10);
        assert(node.keys_[1] == 20);
        assert(node.keys_[2] == 30);

        // Check values
        assert(node.values_[0].size() == 1 && node.values_[0][0] == "ten");
        assert(node.values_[1].size() == 1 && node.values_[1][0] == "twenty");
        assert(node.values_[2].size() == 1 && node.values_[2][0] == "thirty");

        // Check next leaf pointer is zero (no next leaf)
        assert(node.next_leaf_page_ == 0);

        std::cout << "TestInsertionNoSplit passed!\n";
    }
};

int main() {
    BPlusTreeTest::TestInsertionNoSplit();
    return 0;
}
