#ifndef BPLUS_TREE_H
#define BPLUS_TREE_H

#include <vector>
#include <string>
#include <functional>
#include "bnode.h"

class BPlusTree {
public:
    BPlusTree(uint64_t rootPage, std::function<BNode(uint64_t)> getter,
          std::function<uint64_t(BNode)> creator, std::function<void(uint64_t)> deleter);
    
    BNode GetRootNode() const;
    void SetRootNode(const BNode& node);
    ~BPlusTree();

    // Insert function
    BNode treeInsert(const BNode& node, const std::vector<uint8_t>& key, const std::vector<uint8_t>& val);
    
    void nodeInsert(BNode& new_node, BNode& node, uint16_t idx, 
                    const std::vector<uint8_t>& key, const std::vector<uint8_t>& val);

    std::vector<BNode> nodeSplit3(BNode& old);

    void nodeReplaceKidN(BPlusTree& tree, BNode& new_node, BNode& old_node, 
                            uint16_t idx, const std::vector<BNode>& kids);

    void nodeSplit2(BNode& left, BNode& right, const BNode& old);
    // Other function declarations for insertion, search, range queries, deletion, etc.
    
private:
    uint64_t root;
    std::function<BNode(uint64_t)> get;
    std::function<uint64_t(BNode)> newPage;
    std::function<void(uint64_t)> del;

    // Helper functions for B+ tree operations
};

#endif
