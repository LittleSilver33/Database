#include <vector>;

template <typename Key, typename Value>
class LeafNode {
    public:
        uint32_t next_leaf_node_num_;

        // Stores the data. Each Key[i] maps the corresponding Value[i].
        std::vector<Key> keys_;
        std::vector<std::vector<Value>> values_;
};

template <typename Key>
class InternalNode {
    public:
        // The page numbers of all child_nodes. Must be greater than 0.
        std::vector<uint32_t> child_node_nums_;
        // The keys to choose where to traverse
        std::vector<Key> keys_;

        bool is_root_;
}
