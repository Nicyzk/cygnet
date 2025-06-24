#include <list>
#include <mutex>
#include <vector>
#include <optional>
#include "common.h"

#pragma once

/*
 * For every page in the cache, we track last k access times.
 * Evict the page with the largest difference in current time and kth previous access time.
 */
class LRUKNode {
    public:
        LRUKNode(frame_id_t frame_id, size_t k): fid_(frame_id), k_(k), is_evictable_(true) {}

        void RecordAccess();

        std::list<size_t>& GetTimes() { return times_; }

        bool GetEvictable() { return is_evictable_; }

        void SetEvictable() { is_evictable_ = true; }

        void SetNotEvictable() { is_evictable_ = false; }

        frame_id_t GetFrameId() { return fid_; }

        void Evict();

    private:
        std::list<size_t> times_;
        const frame_id_t fid_;
        size_t k_;
        bool is_evictable_;
};

class LRUKReplacer {
    public:
        LRUKReplacer(size_t num_frames, size_t k);
        
        ~LRUKReplacer() = default;

        std::optional<frame_id_t> Evict();

        void RecordAccess(frame_id_t frame_id);

        void SetEvictable(frame_id_t frame_id);

        void SetNotEvictable(frame_id_t frame_id);

        void Remove(frame_id_t frame_id);
    
    private:
        std::mutex lock_;
        size_t num_frames_;
        size_t k_;
        std::vector<LRUKNode> lru_nodes_;
};