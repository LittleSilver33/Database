#ifndef B_PLUS_TREE_H
#define B_PLUS_TREE_H

#include "os_interface.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <optional>
#include <algorithm>
#include <cstring>
#include <span>

static constexpr uint32_t INVALID_PAGE = 0;

// ----- Node Headers -----
enum class NodeType : uint8_t {
    Invalid = 0,
    Leaf = 1,
    Internal = 2
};

// ----- Node Structures -----
template <typename Key, typename Value>
class LeafNode {
public:
    LeafNode() : next_leaf_page_(0) {}

    std::vector<Key> keys_;
    std::vector<std::vector<Value>> values_;
    uint32_t next_leaf_page_;
};

template <typename Key>
class InternalNode {
public:
    InternalNode(bool is_root = false) : is_root_(is_root) {}

    std::vector<Key> keys_;
    std::vector<uint32_t> children_; // length = keys + 1
    bool is_root_;
};

// ----- B+ Tree -----
constexpr size_t PAGE_SIZE = 4096;

template <typename Key, typename Value>
class BPlusTree {
public:
    BPlusTree(const std::string& filename, uint32_t order);
    ~BPlusTree();

    bool Insert(const Key& key, const Value& value);

    // --- debug / test visibility helpers ---
    uint32_t DebugRootPage() const { return root_page_; }

    bool DebugReadPage(uint32_t page_num, std::vector<std::byte>& out) {
        out.resize(PAGE_SIZE);
        return os_.ReadPage(page_num, std::span<std::byte>(out.data(), out.size()));
    }

    void DebugDeserializeLeaf(std::span<const std::byte> page, LeafNode<Key, Value>& node) {
        DeserializeLeaf(page, node);
    }

private:
    OSInterface os_;
    uint32_t root_page_;
    uint32_t next_page_;
    uint32_t order_;

    // Core
    void InsertRecursive(uint32_t page, const Key& key, const Value& value,
                         std::optional<std::pair<Key, uint32_t>>& promoted);

    void SplitLeaf(uint32_t page, LeafNode<Key, Value>& node,
                   std::optional<std::pair<Key, uint32_t>>& promoted);

    void SplitInternal(uint32_t page, InternalNode<Key>& node,
                       std::optional<std::pair<Key, uint32_t>>& promoted);

    // Page helpers
    uint32_t AllocatePage();
    bool IsLeaf(std::span<const std::byte> page);

    // I/O
    void SerializeLeaf(const LeafNode<Key, Value>& node, std::span<std::byte> page);
    void DeserializeLeaf(std::span<const std::byte> page, LeafNode<Key, Value>& node);

    void SerializeInternal(const InternalNode<Key>& node, std::span<std::byte> page);
    void DeserializeInternal(std::span<const std::byte> page, InternalNode<Key>& node);

    bool ReadPage(uint32_t page, std::span<std::byte> buf);
    bool WritePage(uint32_t page, std::span<const std::byte> buf);
};

// ----- Implementation -----

template <typename KeyType, typename ValueType>
BPlusTree<KeyType, ValueType>::BPlusTree(const std::string& filename, uint32_t order)
    : root_page_(INVALID_PAGE), order_(order) {
    next_page_ = 1;
    bool opened = os_.Open(filename);
    assert(opened);  // Or handle error as needed
}

template <typename Key, typename Value>
BPlusTree<Key, Value>::~BPlusTree() {
    os_.Sync();
    os_.Close();
}

template <typename Key, typename Value>
bool BPlusTree<Key, Value>::Insert(const Key& key, const Value& value) {
    if (root_page_ == INVALID_PAGE) {
        LeafNode<Key, Value> root;
        root.next_leaf_page_ = 0;  // no next leaf
        root.keys_.push_back(key);
        root.values_.push_back({ value });  // initialize vector of values for the key
    
        root_page_ = AllocatePage();
        std::vector<std::byte> page(PAGE_SIZE);
        SerializeLeaf(root, page);
        WritePage(root_page_, page);
        return true;
    }
    
    std::optional<std::pair<Key, uint32_t>> promoted;
    InsertRecursive(root_page_, key, value, promoted);

    if (promoted.has_value()) {
        InternalNode<Key> new_root(true);
        new_root.keys_.push_back(promoted->first);
        new_root.children_.push_back(root_page_);
        new_root.children_.push_back(promoted->second);

        std::vector<std::byte> page(PAGE_SIZE);
        uint32_t new_root_page = AllocatePage();
        SerializeInternal(new_root, page);
        WritePage(new_root_page, page);

        root_page_ = new_root_page;
    }

    return true;
}

template <typename Key, typename Value>
void BPlusTree<Key, Value>::InsertRecursive(uint32_t page_num, const Key& key, const Value& value,
                                            std::optional<std::pair<Key, uint32_t>>& promoted) {
    std::vector<std::byte> page(PAGE_SIZE);
    ReadPage(page_num, page);

    if (IsLeaf(page)) {
        LeafNode<Key, Value> node;
        DeserializeLeaf(page, node);

        auto it = std::lower_bound(node.keys_.begin(), node.keys_.end(), key);
        size_t idx = it - node.keys_.begin();

        if (it != node.keys_.end() && *it == key) {
            node.values_[idx].push_back(value);
        } else {
            node.keys_.insert(it, key);
            node.values_.insert(node.values_.begin() + idx, { value });
        }

        if (node.keys_.size() > order_ - 1) {
            SplitLeaf(page_num, node, promoted);
        } else {
            SerializeLeaf(node, page);
            WritePage(page_num, page);
        }
    } else {
        InternalNode<Key> node;
        DeserializeInternal(page, node);

        size_t i = std::upper_bound(node.keys_.begin(), node.keys_.end(), key) - node.keys_.begin();
        uint32_t child_page = node.children_[i];

        std::optional<std::pair<Key, uint32_t>> child_promoted;
        InsertRecursive(child_page, key, value, child_promoted);

        if (child_promoted.has_value()) {
            auto [k_prime, new_page] = *child_promoted;
            auto it = std::upper_bound(node.keys_.begin(), node.keys_.end(), k_prime);
            size_t pos = it - node.keys_.begin();

            node.keys_.insert(it, k_prime);
            node.children_.insert(node.children_.begin() + pos + 1, new_page);

            if (node.children_.size() > order_) {
                SplitInternal(page_num, node, promoted);
            } else {
                SerializeInternal(node, page);
                WritePage(page_num, page);
            }
        }
    }
}

template <typename Key, typename Value>
void BPlusTree<Key, Value>::SplitLeaf(uint32_t page_num, LeafNode<Key, Value>& node,
                                      std::optional<std::pair<Key, uint32_t>>& promoted) {
    size_t mid = node.keys_.size() / 2;

    LeafNode<Key, Value> new_leaf;
    new_leaf.keys_.assign(node.keys_.begin() + mid, node.keys_.end());
    new_leaf.values_.assign(node.values_.begin() + mid, node.values_.end());

    node.keys_.resize(mid);
    node.values_.resize(mid);

    new_leaf.next_leaf_page_ = node.next_leaf_page_;
    uint32_t new_page = AllocatePage();
    node.next_leaf_page_ = new_page;

    std::vector<std::byte> page1(PAGE_SIZE), page2(PAGE_SIZE);
    SerializeLeaf(node, page1);
    SerializeLeaf(new_leaf, page2);

    WritePage(page_num, page1);
    WritePage(new_page, page2);

    promoted = std::make_pair(new_leaf.keys_.front(), new_page);
}

template <typename Key, typename Value>
void BPlusTree<Key, Value>::SplitInternal(uint32_t page_num, InternalNode<Key>& node,
                                          std::optional<std::pair<Key, uint32_t>>& promoted) {
    size_t mid = node.keys_.size() / 2;
    Key mid_key = node.keys_[mid];

    InternalNode<Key> new_node;
    new_node.keys_.assign(node.keys_.begin() + mid + 1, node.keys_.end());
    new_node.children_.assign(node.children_.begin() + mid + 1, node.children_.end());

    node.keys_.resize(mid);
    node.children_.resize(mid + 1);

    uint32_t new_page = AllocatePage();
    std::vector<std::byte> page1(PAGE_SIZE), page2(PAGE_SIZE);

    SerializeInternal(node, page1);
    SerializeInternal(new_node, page2);

    WritePage(page_num, page1);
    WritePage(new_page, page2);

    promoted = std::make_pair(mid_key, new_page);
}

// ----- Page Helpers -----

template <typename Key, typename Value>
bool BPlusTree<Key, Value>::IsLeaf(std::span<const std::byte> page) {
    auto t = static_cast<NodeType>(page[0]);
    return t == NodeType::Leaf;
}

template <typename Key, typename Value>
uint32_t BPlusTree<Key, Value>::AllocatePage() {
    return next_page_++;
}

// ----- Serialization -----

template <typename Key, typename Value>
void BPlusTree<Key, Value>::SerializeLeaf(const LeafNode<Key, Value>& node, std::span<std::byte> page) {
    page[0] = static_cast<std::byte>(NodeType::Leaf);
    std::memcpy(&page[1], &node.next_leaf_page_, sizeof(uint32_t));

    size_t offset = 5;
    uint32_t count = node.keys_.size();
    std::memcpy(&page[offset], &count, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&page[offset], &node.keys_[i], sizeof(Key));
        offset += sizeof(Key);

        uint32_t vcount = node.values_[i].size();
        std::memcpy(&page[offset], &vcount, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        for (const Value& v : node.values_[i]) {
            std::memcpy(&page[offset], &v, sizeof(Value));
            offset += sizeof(Value);
        }
    }
}

template <typename Key, typename Value>
void BPlusTree<Key, Value>::DeserializeLeaf(std::span<const std::byte> page, LeafNode<Key, Value>& node) {
    std::memcpy(&node.next_leaf_page_, &page[1], sizeof(uint32_t));

    size_t offset = 5;
    uint32_t count;
    std::memcpy(&count, &page[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);

    node.keys_.resize(count);
    node.values_.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&node.keys_[i], &page[offset], sizeof(Key));
        offset += sizeof(Key);

        uint32_t vcount;
        std::memcpy(&vcount, &page[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        node.values_[i].resize(vcount);
        for (uint32_t j = 0; j < vcount; ++j) {
            std::memcpy(&node.values_[i][j], &page[offset], sizeof(Value));
            offset += sizeof(Value);
        }
    }
}

template <typename Key, typename Value>
void BPlusTree<Key, Value>::SerializeInternal(const InternalNode<Key>& node, std::span<std::byte> page) {
    page[0] = static_cast<std::byte>(NodeType::Internal);

    size_t offset = 1;
    uint32_t count = node.keys_.size();
    std::memcpy(&page[offset], &count, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&page[offset], &node.keys_[i], sizeof(Key));
        offset += sizeof(Key);
    }

    uint32_t child_count = node.children_.size();
    for (uint32_t i = 0; i < child_count; ++i) {
        std::memcpy(&page[offset], &node.children_[i], sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
}

template <typename Key, typename Value>
void BPlusTree<Key, Value>::DeserializeInternal(std::span<const std::byte> page, InternalNode<Key>& node) {
    size_t offset = 1;
    uint32_t count;
    std::memcpy(&count, &page[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);

    node.keys_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&node.keys_[i], &page[offset], sizeof(Key));
        offset += sizeof(Key);
    }

    node.children_.resize(count + 1);
    for (uint32_t i = 0; i <= count; ++i) {
        std::memcpy(&node.children_[i], &page[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
}

// ----- Disk I/O -----

template <typename Key, typename Value>
bool BPlusTree<Key, Value>::ReadPage(uint32_t page_num, std::span<std::byte> buf) {
    return os_.ReadPage(page_num, buf);
}

template <typename Key, typename Value>
bool BPlusTree<Key, Value>::WritePage(uint32_t page_num, std::span<const std::byte> buf) {
    return os_.WritePage(page_num, buf);
}

#endif // B_PLUS_TREE_H
