#ifndef BNODE_H
#define BNODE_H
#include <vector>
#include <cstdint>


class BNode {
public:
    BNode();
    explicit BNode(std::vector<uint8_t> data);

    const std::vector<uint8_t>& GetData() const { return data; }

    static const uint8_t BNODE_NODE = 1;
    static const uint8_t BNODE_LEAF = 2;

    uint16_t btype() const;
    uint16_t nkeys() const;
    void setHeader(uint16_t btype, uint16_t nkeys);

    uint64_t getPtr(uint16_t idx) const;
    void setPtr(uint16_t idx, uint64_t val);

    uint16_t offsetPos(uint16_t idx) const;
    uint16_t getOffset(uint16_t idx) const;
    void setOffset(uint16_t idx, uint16_t offset);

    uint16_t kvPos(uint16_t idx) const;
    std::vector<uint8_t> getKey(uint16_t idx) const;
    std::vector<uint8_t> getVal(uint16_t idx) const;

    uint16_t nbytes() const;

    uint16_t nodeLookupLE(const BNode& node, const std::vector<uint8_t>& key);

    void nodeAppendRange(BNode& new_node, const BNode& old_node, uint16_t dst_new, uint16_t src_old, uint16_t n);
    void nodeAppendKV(BNode& new_node, uint16_t idx, uint64_t ptr, const std::vector<uint8_t>& key, const std::vector<uint8_t>& val);
    void leafInsert(BNode& old, uint16_t idx, const std::vector<uint8_t>& key, const std::vector<uint8_t>& val);

private:
    std::vector<uint8_t> data;
};

#endif