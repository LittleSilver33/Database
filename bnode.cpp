#include "bnode.h"
#include <cassert>
#include <cstring>
#include <algorithm>

constexpr int HEADER = 4;

BNode::BNode() {
    // Default constructor
}

BNode::BNode(std::vector<uint8_t> data) : data(std::move(data)) {
    // Constructor with data
}

// Helper functions for reading data and such
uint16_t BNode::btype() const {
    return *reinterpret_cast<const uint16_t*>(data.data());
}

uint16_t BNode::nkeys() const {
    return *reinterpret_cast<const uint16_t*>(data.data() + 2);
}

void BNode::setHeader(uint16_t btype, uint16_t nkeys) {
    *reinterpret_cast<uint16_t*>(data.data()) = btype;
    *reinterpret_cast<uint16_t*>(data.data() + 2) = nkeys;
}

uint64_t BNode::getPtr(uint16_t idx) const {
    assert(idx < nkeys());
    uint16_t pos = HEADER + 8 * idx;
    return *reinterpret_cast<const uint64_t*>(data.data() + pos);
}

void BNode::setPtr(uint16_t idx, uint64_t val) {
    assert(idx < nkeys());
    uint16_t pos = HEADER + 8 * idx;
    *reinterpret_cast<uint64_t*>(data.data() + pos) = val;
}



uint16_t BNode::offsetPos(uint16_t idx) const {
    assert(1 <= idx && idx <= nkeys());
    return HEADER + 8 * nkeys() + 2 * (idx - 1);
}

uint16_t BNode::getOffset(uint16_t idx) const {
    if (idx == 0) {
        return 0;
    }
    return *reinterpret_cast<const uint16_t*>(&data[offsetPos(idx)]);
}

void BNode::setOffset(uint16_t idx, uint16_t offset) {
    *reinterpret_cast<uint16_t*>(&data[offsetPos(idx)]) = offset;
}

uint16_t BNode::kvPos(uint16_t idx) const {
    assert(idx <= nkeys());
    return HEADER + 8 * nkeys() + 2 * nkeys() + getOffset(idx);
}

std::vector<uint8_t> BNode::getKey(uint16_t idx) const {
    assert(idx < nkeys());
    uint16_t pos = kvPos(idx);
    uint16_t klen = *reinterpret_cast<const uint16_t*>(&data[pos]);
    return std::vector<uint8_t>(data.begin() + pos + 4, data.begin() + pos + 4 + klen);
}

std::vector<uint8_t> BNode::getVal(uint16_t idx) const {
    assert(idx < nkeys());
    uint16_t pos = kvPos(idx);
    uint16_t klen = *reinterpret_cast<const uint16_t*>(&data[pos]);
    uint16_t vlen = *reinterpret_cast<const uint16_t*>(&data[pos + 2]);
    return std::vector<uint8_t>(data.begin() + pos + 4 + klen, data.begin() + pos + 4 + klen + vlen);
}

uint16_t BNode::nbytes() const {
    return kvPos(nkeys());
}

// returns the first kid node whose range intersects the key. (kid[i] <= key)
// TODO: bisect
uint16_t BNode::nodeLookupLE(const BNode& node, const std::vector<uint8_t>& key) {
    uint16_t nkeys = node.nkeys();
    uint16_t found = 0;

    // The first key is a copy from the parent node,
    // thus it's always less than or equal to the key.
    for (uint16_t i = 1; i < nkeys; i++) {
        int cmp = std::memcmp(node.getKey(i).data(), key.data(), std::min(node.getKey(i).size(), key.size()));
        if (cmp <= 0) {
            found = i;
        }
        if (cmp >= 0) {
            break;
        }
    }
    return found;
}

void BNode::nodeAppendRange(BNode& new_node, const BNode& old_node, uint16_t dst_new, uint16_t src_old, uint16_t n) {
    assert(src_old + n <= old_node.nkeys());
    assert(dst_new + n <= new_node.nkeys());

    if (n == 0) {
        return;
    }

    // Pointers
    for (uint16_t i = 0; i < n; i++) {
        new_node.setPtr(dst_new + i, old_node.getPtr(src_old + i));
    }

    // Offsets
    uint16_t dst_begin = new_node.getOffset(dst_new);
    uint16_t src_begin = old_node.getOffset(src_old);
    for (uint16_t i = 1; i <= n; i++) {
        uint16_t offset = dst_begin + old_node.getOffset(src_old + i) - src_begin;
        new_node.setOffset(dst_new + i, offset);
    }

    // KVs
    uint16_t begin = old_node.kvPos(src_old);
    uint16_t end = old_node.kvPos(src_old + n);
    std::copy(old_node.data.begin() + begin, old_node.data.begin() + end, new_node.data.begin() + new_node.kvPos(dst_new));
}

void BNode::nodeAppendKV(BNode& new_node, uint16_t idx, uint64_t ptr, const std::vector<uint8_t>& key, const std::vector<uint8_t>& val) {
    // Pointers
    new_node.setPtr(idx, ptr);

    // KVs
    uint16_t pos = new_node.kvPos(idx);
    *reinterpret_cast<uint16_t*>(&new_node.data[pos]) = static_cast<uint16_t>(key.size());
    *reinterpret_cast<uint16_t*>(&new_node.data[pos + 2]) = static_cast<uint16_t>(val.size());
    std::copy(key.begin(), key.end(), new_node.data.begin() + pos + 4);
    std::copy(val.begin(), val.end(), new_node.data.begin() + pos + 4 + key.size());

    // Offset of the next key
    new_node.setOffset(idx + 1, new_node.getOffset(idx) + 4 + static_cast<uint16_t>(key.size() + val.size()));
}

void BNode::leafInsert(BNode& old, uint16_t idx, const std::vector<uint8_t>& key, const std::vector<uint8_t>& val) {
    BNode new_node;
    new_node.setHeader(BNODE_LEAF, old.nkeys() + 1);

    // Append keys and values before and after the new key
    nodeAppendRange(new_node, old, 0, 0, idx);
    nodeAppendKV(new_node, idx, 0, key, val);
    nodeAppendRange(new_node, old, idx + 1, idx, old.nkeys() - idx);

    data = new_node.data; // Update the original node with the new data
}