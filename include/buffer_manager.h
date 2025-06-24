#include <list>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <cstring>
#include "common.h"
#include "disk_manager.h"
#include "lru_k_replacer.h"
#include "background_scheduler.h"

#pragma once

/* forward declarations */
class GuardedPageReader;
class GuardedPageWriter;

class Frame {
    private:
        const frame_id_t frame_id_;
        bool dirty_;
        std::atomic<size_t> pincount_; // Need to make this atomic!! Equivalent to Frame being evictable.
        char* data_;
        std::shared_mutex rwlock_;

    public: 
        Frame(frame_id_t frame_id);
        ~Frame();

        frame_id_t GetFrameId() { return frame_id_; }

        bool GetDirty() { return dirty_; }
        void SetDirty(bool dirty) { dirty_ = dirty; }

        void IncPinCount() { pincount_.fetch_add(1); }
        void DecPinCount() { pincount_.fetch_sub(1); }
        size_t GetPinCount() { return pincount_.load(); }

        const char* GetData() { return data_; }; /* For reading from frame, and writing frame to disk. */
        char* GetDataMut() { return data_; }; /* For modifying frame, and reading a page from disk into memory/frame. */
        
        std::shared_mutex& GetMutex() { return rwlock_; }
};

class BufferManager {
    private:
        std::vector<std::shared_ptr<Frame>> buffer_;

        std::unordered_map<page_id_t, frame_id_t> page_table_;
        std::unordered_map<frame_id_t, page_id_t> reverse_page_table_;

        std::list<frame_id_t> free_frames_;

        std::shared_ptr<LRUKReplacer> lru_k_replacer_;
        std::shared_ptr<Background_Scheduler> background_scheduler_;

        std::shared_ptr<std::mutex> bpm_latch_;

        /*
         * Important note: Page_id is monotonically increasing and is not recyclable. 
         * This ensures we know which page_id has been deleted.
         * 
         * For e.g., if page_id can be recycled, we could have the following situation:
         * 1. Thread 1 deletes page 1
         * 2. Thread 2 tries to access page 1 but is preempted.
         * 3. Thread 3 allocates a new page, and reuses page_id 1.
         * 4. Thread 2 continues execution and writes to page 1, without realizing the page had been deleted.
         */
        page_id_t next_page_id_; 
    
        /* Evicts frame if required. Schedules a flush of old page to disk. */
        std::optional<frame_id_t> GetFreeFrame();

    public:
        BufferManager(size_t num_buffer_frames, DiskManager* disk_manager, size_t k);

        /* Allocates new page on disk only. */
        page_id_t NewPage();

        /* If pincount_ > 0, return false. Else uses disk manager to delete page, and evict frame. */
        bool DeletePage(page_id_t page_id);

        /* Performs 1. reading from in-memory page. */
        std::optional<GuardedPageReader> GetGuardedPageReader(page_id_t page_id);

        /* Performs 1. writing to in-memory page, and 2. flushing to disk. */
        std::optional<GuardedPageWriter> GetGuardedPageWriter(page_id_t page_id);

        std::optional<size_t> GetPinCount(page_id_t page_id);
};