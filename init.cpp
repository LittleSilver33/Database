#include <cassert>
#include "config.h"

void init() {
    int node1max = HEADER + 8 + 2 + 4 + BTREE_MAX_KEY_SIZE + BTREE_MAX_VAL_SIZE;
    assert(node1max <= BTREE_PAGE_SIZE);
}
