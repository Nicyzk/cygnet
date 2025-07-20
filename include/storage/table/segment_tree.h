#include "common.h"
#include <memory>
#include <vector>
#include <mutex>
#include <algorithm>
#include <iostream>

#pragma once

struct SegmentLock {
    public:
        SegmentLock(std::mutex &lock): lock(lock) {}

        SegmentLock(const SegmentLock& segment_lock) = delete;
        SegmentLock operator=(const SegmentLock& segment_lock) = delete;

        SegmentLock(SegmentLock&& that): lock(std::move(that.lock)) {
        }

        SegmentLock& operator=(SegmentLock&& that) {
            if (&that == this)
                return *this;

            lock = std::move(that.lock);
            return *this;
        }

    private:
        std::unique_lock<std::mutex> lock;
};

template <class T>
struct SegmentNode {
    idx_t row_start;
    std::unique_ptr<T> node;
};

// We do not support lazy loading for now. But how do we load the segments from disk???
template <class T>
class SegmentTree { 
    public:
        SegmentTree() {
        }

        T* GetRootSegment() {
            auto l = SegmentLock(node_lock);
            return nodes.empty() ? nullptr : nodes[0].node.get();
        };
        idx_t GetSegmentCount() {
            auto l = SegmentLock(node_lock);
            return nodes.size();
        };

        T* GetNextSegment(T* segment) {
            idx_t index = segment->index;
            if (index >= nodes.size())
        }
        
        T* GetLastSegment() {
            auto l = SegmentLock(node_lock);
            return nodes.empty() ? nullptr : nodes.back().node.get();
        }

        // Assume nodes are stored in increasing row_start order. Do a binary search.
        T* GetSegment(idx_t row_number) {
            idx_t segment_index;
            if (!TryGetSegmentIndex(row_number, segment_index)) {
                std::cerr << "[GetSegment] Failed for row number: " << row_number << std::endl;
                throw "Failed";
            }
            return nodes[segment_index].get();
        }

        bool TryGetSegmentIndex(idx_t row_number, idx_t &result) {
            if (nodes.empty())
                return false;

            auto it = std::lower_bound(nodes.begin(), nodes.end(), row_number,
                [](const SegmentNode<T> &a, idx_t row_number) {
                    return a.row_start < row_number;
                }
            );

            if (it == nodes.end())
                return false;

            if (row_number > (it->node->start + it->node->count))
                return false;

            result = it - nodes.begin();
            return true;
        }

        void AppendSegment();
        bool HasSegment(T* segment);
    private:
        std::vector<SegmentNode<T>> nodes;
        std::mutex node_lock;
};