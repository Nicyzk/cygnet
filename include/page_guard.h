#include "background_scheduler.h"
#include "common.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "buffer_manager.h"
#include "lru_k_replacer.h"

#pragma once

/* 
 * GuardedPageReaders(Writers) operate on the assumption that the page is already mapped to a frame
 * (i.e. in memory) by the buffer manager.
 * ReadPage() & WritePage() therefore operate on pages in memory.
 * Only FlushPage() uses the disk manager to write to disk.
 */
class GuardedPageReader {
    private:
        /* Reader lock on frame. */ 
        std::shared_lock<std::shared_mutex> rlock_;

        /* If guard is holding a pin to the frame. */
        bool is_pinned_ = false;
        
        page_id_t page_id_;
        std::shared_ptr<std::mutex> bpm_latch_;
        std::shared_ptr<Frame> frame_;
        std::shared_ptr<Background_Scheduler> background_scheduler_;
        std::shared_ptr<LRUKReplacer> lru_replacer_;

    public:
        GuardedPageReader(
            page_id_t page_id,
            std::shared_ptr<Frame> frame,
            std::shared_ptr<std::mutex> bpm_latch,
            std::shared_ptr<LRUKReplacer> lru_replacer,
            std::shared_ptr<Background_Scheduler> background_scheduler
        );

        ~GuardedPageReader();

        GuardedPageReader(const GuardedPageReader& guarded_page_reader) = delete;
        GuardedPageReader operator=(const GuardedPageReader& guarded_page_reader)  = delete;

        GuardedPageReader(GuardedPageReader&& that) noexcept;
        GuardedPageReader& operator=(GuardedPageReader&& that) noexcept;

        const char* GetData() const;
        void Drop();
};


class GuardedPageWriter {
    private:
        /* Writer lock on frame. */
        std::unique_lock<std::shared_mutex> wlock_;
        
        /* If guard is holding a pin to the frame. */
        bool is_pinned_ = false;

        page_id_t page_id_;
        std::shared_ptr<std::mutex> bpm_latch_;
        std::shared_ptr<Frame> frame_;
        std::shared_ptr<Background_Scheduler> background_scheduler_;
        std::shared_ptr<LRUKReplacer> lru_replacer_;
    
    public:
        GuardedPageWriter(
            page_id_t page_id,
            std::shared_ptr<Frame> frame,
            std::shared_ptr<std::mutex> bpm_latch,
            std::shared_ptr<LRUKReplacer> lru_replacer,
            std::shared_ptr<Background_Scheduler> background_scheduler
        );

        ~GuardedPageWriter();

        GuardedPageWriter(const GuardedPageWriter& guarded_page_writer) = delete;
        GuardedPageWriter operator=(const GuardedPageWriter& guarded_page_writer) = delete;

        GuardedPageWriter(GuardedPageWriter&& that);
        GuardedPageWriter& operator=(GuardedPageWriter&& that);

        const char* GetData();
        char* GetDataMut();
        void FlushPage();
        void Drop();
        const page_id_t GetPageId() const;
};