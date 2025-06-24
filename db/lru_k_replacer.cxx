#include <chrono>
#include "lru_k_replacer.h"
#include "cassert"
#include "common.h"

/* Maybe can optimize to have locking on LRUKNodes too? */

void LRUKNode::RecordAccess() {
    std::chrono::time_point now = std::chrono::steady_clock::now();
    if (times_.size() >= k_) {
        times_.pop_front();
    }
    times_.push_back(now.time_since_epoch().count());
}

void LRUKNode::Evict() {
    times_.clear();
}

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k): num_frames_(num_frames), k_(k) {
    lru_nodes_.reserve(num_frames);
    for (int i=0; i<num_frames; i++)
        lru_nodes_.emplace_back(i, k);
}

/* TODO: Slightly incorrect because we evict any node that has < k accesses immediately. To fix. */
std::optional<frame_id_t> LRUKReplacer::Evict(){
    lock_.lock();

    /* LRU K algorithm */
    std::optional<frame_id_t> evictable_frame_id = std::nullopt;
    size_t current_time_since_epoch = std::chrono::steady_clock::now().time_since_epoch().count();
    size_t max_backward_k_distance = 0;

    for (int i=0; i<num_frames_; i++) {
        LRUKNode& node = lru_nodes_[i];
        if (!node.GetEvictable())
            continue;

        std::list<size_t>& times = node.GetTimes();
        size_t node_num_accesses = times.size();
        size_t node_backward_k_distance = current_time_since_epoch - times.front();

        if (node_num_accesses < k_) {
            evictable_frame_id = node.GetFrameId();
            break;
        }

        if (node_backward_k_distance >= max_backward_k_distance) {
            max_backward_k_distance = node_backward_k_distance;
            evictable_frame_id = node.GetFrameId();
        }
    }

    /* evict frame */
    if (evictable_frame_id.has_value()) {
        LRUKNode& node = lru_nodes_[evictable_frame_id.value()];
        assert(node.GetEvictable());
        node.Evict();
    }

    lock_.unlock();
    return evictable_frame_id;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
    lock_.lock();
    LRUKNode& node = lru_nodes_[frame_id];
    node.RecordAccess();
    lock_.unlock();
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id) {
    lock_.lock();
    LRUKNode& node = lru_nodes_[frame_id];
    node.SetEvictable();
    lock_.unlock();
}

void LRUKReplacer::SetNotEvictable(frame_id_t frame_id) {
    lock_.lock();
    LRUKNode& node = lru_nodes_[frame_id];
    node.SetNotEvictable();
    lock_.unlock();
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
    lock_.lock();
    LRUKNode& node = lru_nodes_[frame_id];
    assert(node.GetEvictable());
    node.Evict();
    lock_.unlock();
}
