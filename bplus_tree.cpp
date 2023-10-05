#include "bplus_tree.h"

BNode treeInsert(BTree& tree, const BNode& node, const std::string& key, const std::string& val) {
    BNode new_node;

    size_t idx = nodeLookupLE(node, key); // Assuming this function exists

    switch(node.btype()) {
        case BNODE_LEAF:
            if (key == node.getKey(idx)) {
                leafUpdate(new_node, node, idx, key, val); // Assuming this function exists
            } else {
                leafInsert(new_node, node, idx+1, key, val); // Assuming this function exists
            }
            break;

        case BNODE_NODE:
            nodeInsert(tree, new_node, node, idx, key, val); // Assuming this function exists
            break;

        default:
            throw std::runtime_error("Bad node!");
    }
    
    return new_node;
}

void nodeInsert(BNode& new_node, BNode& node, uint16_t idx, 
                           const std::vector<uint8_t>& key, const std::vector<uint8_t>& val) {
    // Get and deallocate the kid node
    uint64_t kptr = node.getPtr(idx);
    BNode knode = get(kptr); // Assuming a 'get' method that retrieves a node
    del(kptr); // Assuming a 'del' method that deallocates a node

    // Recursive insertion to the kid node
    knode = treeInsert(knode, key, val); // Assuming 'treeInsert' is defined

    // Split the result
    // Assuming 'nodeSplit3' returns a pair with the number of splits and a vector of split nodes
    auto [nsplit, splited] = nodeSplit3(knode); 

    // Update the kid links
    // Assuming 'nodeReplaceKidN' updates child nodes in 'node' starting from 'idx' with nodes in 'splited'
    nodeReplaceKidN(new_node, node, idx, splited, nsplit);
}

std::vector<BNode> nodeSplit3(BNode& old) {
    std::vector<BNode> result;

    if (old.nbytes() <= BTREE_PAGE_SIZE) {
        old.data.resize(BTREE_PAGE_SIZE);
        result.push_back(old);
        return result;
    }

    BNode left, right;
    left.data.resize(2 * BTREE_PAGE_SIZE); // Might be split later
    right.data.resize(BTREE_PAGE_SIZE);
    nodeSplit2(left, right, old);

    if (left.nbytes() <= BTREE_PAGE_SIZE) {
        left.data.resize(BTREE_PAGE_SIZE);
        result.push_back(left);
        result.push_back(right);
        return result;
    }

    // The left node is still too large
    BNode leftleft, middle;
    leftleft.data.resize(BTREE_PAGE_SIZE);
    middle.data.resize(BTREE_PAGE_SIZE);
    nodeSplit2(leftleft, middle, left);
    assert(leftleft.nbytes() <= BTREE_PAGE_SIZE);

    result.push_back(leftleft);
    result.push_back(middle);
    result.push_back(right);
    return result;
}

void nodeReplaceKidN(BPlusTree& tree, BNode& new_node, BNode& old_node, 
                            uint16_t idx, const std::vector<BNode>& kids) {
    uint16_t inc = static_cast<uint16_t>(kids.size());
    new_node.setHeader(BNODE_NODE, old_node.nkeys() + inc - 1);
    new_node.nodeAppendRange(old_node, 0, idx);
    
    for (size_t i = 0; i < kids.size(); ++i) {
        new_node.nodeAppendKV(idx + static_cast<uint16_t>(i), 
                              tree.newPage(kids[i]), // Assuming 'newPage' creates a new page for a node
                              kids[i].getKey(0), 
                              {}); // Empty value
    }
    
    new_node.nodeAppendRange(old_node, idx + inc, old_node.nkeys() - (idx + 1));
}

void nodeSplit2(BNode& left, BNode& right, const BNode& old) {
    // Determine the index at which to split the old node
    size_t mid_idx = old.nkeys() / 2;

    // Initialize the left and right nodes
    left.setHeader(old.btype(), mid_idx);
    right.setHeader(old.btype(), old.nkeys() - mid_idx);

    // Copy keys and values from the old node to the left and right nodes
    for (size_t i = 0; i < old.nkeys(); ++i) {
        if (i < mid_idx) {
            // Copy to the left node
            left.insertKeyVal(i, old.getKey(i), old.getVal(i)); // Assuming insertKeyVal inserts a key-value pair
        } else {
            // Copy to the right node
            right.insertKeyVal(i - mid_idx, old.getKey(i), old.getVal(i));
        }
    }

    // If it's an internal node, copy child pointers
    if (old.btype() == BNODE_NODE) {
        for (size_t i = 0; i <= old.nkeys(); ++i) {
            if (i <= mid_idx) {
                left.setPtr(i, old.getPtr(i)); // Assuming setPtr sets a child pointer
            } else {
                right.setPtr(i - mid_idx - 1, old.getPtr(i));
            }
        }
    }
}

// ... definitions for other functions ...