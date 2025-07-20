#include "common.h"
#include "atomic"

#pragma once

// template<class T> // not required yet
class SegmentBase {
    public:
        SegmentBase(row_id_t start, idx_t count) : start(start), count(count) {}
        row_id_t start;
        std::atomic<idx_t> count;

        // index within segment tree
        idx_t index;
};