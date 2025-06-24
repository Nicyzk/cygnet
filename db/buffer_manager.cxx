#include "buffer_manager.h"
#include "lru_k_replacer.h"
#include "disk_manager.h"
#include "common.h"
#include <memory>
#include <optional>
#include <cassert>
#include <unordered_map>
#include <iostream>
#include "page_guard.h"

Frame::Frame(frame_id_t frame_id): frame_id_(frame_id), dirty_(false), pincount_(0) {
    data_ = new char[PAGE_SIZE];
    std::memset(data_, 0, PAGE_SIZE);
}

Frame::~Frame() {
    delete[] data_;
}

BufferManager::BufferManager(size_t num_buffer_frames, DiskManager* disk_manager, size_t k)
:background_scheduler_(std::make_shared<Background_Scheduler>(disk_manager)),
lru_k_replacer_(std::make_shared<LRUKReplacer>(num_buffer_frames, k)),
bpm_latch_(std::make_shared<std::mutex>()),
next_page_id_(0) {
    /* Allocate frames in buffer_. */
    buffer_.reserve(num_buffer_frames);
    for (int i=0; i<num_buffer_frames; i++) {
        buffer_.emplace_back(std::make_shared<Frame>(i));
        free_frames_.push_back(i);
    }
}

/* 
 * Returns the frame_id corresponding to page_id. Also updates in-memory frame<->page and page<->frame mappings.
 * If required, it assigns a free frame to the page, and potentially evicts a different page in the process.
 */
std::optional<frame_id_t> BufferManager::GetFreeFrame() {
    /* If free frame exists, return it. */
    std::optional<frame_id_t> frame_id_opt = std::nullopt;
    if (!free_frames_.empty()) {
        frame_id_opt = free_frames_.front();
        free_frames_.pop_front();
        return frame_id_opt;
    } 
    
    /* If no free frames, try to evict a page. */
    frame_id_opt = lru_k_replacer_->Evict();

    /* If still no evictable frames, return nullopt. */
    if (!frame_id_opt.has_value())
        return frame_id_opt;
    frame_id_t frame_id = frame_id_opt.value();

    assert(buffer_[frame_id]->GetPinCount() == 0);

    std::shared_ptr<Frame> frame = buffer_[frame_id];
    page_id_t prev_page_id = reverse_page_table_[frame_id];

    /* Flush previous page to disk if required. */
    if (frame->GetDirty()) {
        /* Schedules a flush. */
        std::shared_ptr<Request> write_req = std::make_shared<Request>(false, prev_page_id, frame->GetData());
        background_scheduler_->Schedule(write_req);
        try {
            write_req.get()->promise_.get_future().get();
        } catch (const std::exception &e) {
            std::cerr << "[GetFreeFrame] " << e.what();
        }
        frame->SetDirty(false);
    }

    /* Reset frame state. */
    char* data = frame->GetDataMut();
    std::memset(data, 0, PAGE_SIZE);
    page_table_.erase(prev_page_id);
    reverse_page_table_.erase(frame_id);

    return frame_id_opt;
}

page_id_t BufferManager::NewPage() {
    bpm_latch_->lock();
    std::optional<frame_id_t> frame_id_opt = GetFreeFrame();
    if (!frame_id_opt.has_value()) {
        bpm_latch_->unlock();
        return INVALID_PAGE_ID;
    }
    frame_id_t frame_id = frame_id_opt.value();
    
    /* Set page dirty. Page will be flushed if required. */
    buffer_[frame_id]->SetDirty(true);

    /* Map assigned frame to page. */
    page_table_[next_page_id_] = frame_id;
    reverse_page_table_[frame_id] = next_page_id_;

    next_page_id_++;
    bpm_latch_->unlock();
    return next_page_id_-1;
}

/* Disk manager simply invalidates page_id so that future (racy) read/writes throw error. */
bool BufferManager::DeletePage(page_id_t page_id) {
    bpm_latch_->lock();

    frame_id_t frame_id = page_table_[page_id];
    std::shared_ptr<Frame> frame = buffer_[frame_id];

    if (frame->GetPinCount() > 0) {
        bpm_latch_->unlock();
        return false;
    }

    /* No need to flush. We simply reset frame state. */
    frame->SetDirty(false);
    char* data = frame->GetDataMut();
    std::memset(data, 0, PAGE_SIZE);

    page_table_.erase(page_id);
    reverse_page_table_.erase(frame_id);
    free_frames_.push_back(frame_id);

    lru_k_replacer_->Remove(frame_id);

    background_scheduler_->DeletePage(page_id);

    bpm_latch_->unlock();
    return true;
}

std::optional<GuardedPageReader> BufferManager::GetGuardedPageReader(page_id_t page_id) {
    bpm_latch_->lock();

    /* If page not in memory and not on disk i.e. INVALID_PAGE_ID, return nullopt. */
    if ((page_table_.find(page_id) == page_table_.end()) && (!background_scheduler_->CheckPageExists(page_id))) {
        bpm_latch_->unlock();
        return std::nullopt;
    }

    /* If page already in memory, a frame is already assigned. */
    frame_id_t frame_id;
    if ((page_table_.find(page_id) != page_table_.end())) {
        frame_id = page_table_[page_id];
        auto guard = GuardedPageReader{
            page_id, buffer_[frame_id], bpm_latch_, 
            lru_k_replacer_, background_scheduler_ 
        };
        bpm_latch_->unlock();
        return std::optional<GuardedPageReader>(std::move(guard));
    }

    /* If page is on disk, but not in memory. */
    std::optional<frame_id_t> frame_id_opt = GetFreeFrame();
    if (!frame_id_opt.has_value()) {
        bpm_latch_->unlock();
        return std::nullopt;
    }
    frame_id = frame_id_opt.value();

    /* Map assigned frame to page. */
    page_table_[page_id] = frame_id;
    reverse_page_table_[frame_id] = page_id;

    /* Read the page into frame. */
    std::shared_ptr<Request> read_req = std::make_shared<Request>(true, page_id, buffer_[frame_id]->GetDataMut());
    background_scheduler_->Schedule(read_req);
    try {
        read_req.get()->promise_.get_future().get();
    } catch (const std::exception &e) {
        std::cerr << "[GetGuardedPageReader] " << e.what();
    }

    /* Note: guard constructor increments pincount, which implies page_id must be valid henceforth. */
    GuardedPageReader guard{
        page_id, buffer_[frame_id], bpm_latch_,
        lru_k_replacer_, background_scheduler_ 
    };

    bpm_latch_->unlock();
    return std::optional<GuardedPageReader>(std::move(guard));
}

std::optional<GuardedPageWriter> BufferManager::GetGuardedPageWriter(page_id_t page_id) {
    bpm_latch_->lock();

    /* If page not in memory and not on disk, return nullopt. */
    if ((page_table_.find(page_id) == page_table_.end()) && (!background_scheduler_->CheckPageExists(page_id))) {
        bpm_latch_->unlock();
        return std::nullopt;
    }

    /* If page already in memory, a frame is already assigned. */
    frame_id_t frame_id;
    if ((page_table_.find(page_id) != page_table_.end())) {
        frame_id = page_table_[page_id];
        bpm_latch_->unlock();
        auto guard = GuardedPageWriter{
            page_id, buffer_[frame_id], bpm_latch_, 
            lru_k_replacer_, background_scheduler_ 
        };
        return std::optional<GuardedPageWriter>(std::move(guard));
    }

    /* If page is on disk, but not in memory. */
    std::optional<frame_id_t> frame_id_opt = GetFreeFrame();
    if (!frame_id_opt.has_value()) {
        bpm_latch_->unlock();
        return std::nullopt;
    }
    frame_id = frame_id_opt.value();

    /* Map assigned frame to page. */
    page_table_[page_id] = frame_id;
    reverse_page_table_[frame_id] = page_id;

    /* Read the page into frame. */
    std::shared_ptr<Request> read_req = std::make_shared<Request>(true, page_id, buffer_[frame_id]->GetDataMut());
    background_scheduler_->Schedule(read_req);
    try {
        read_req.get()->promise_.get_future().get();
    }  catch (const std::exception &e) {
        std::cerr << "[GetGuardedPageWriter] " << e.what();
    }

    bpm_latch_->unlock();
    /* Note: guard constructor increments pincount, which implies page_id must be valid henceforth. */
    auto guard = GuardedPageWriter{
        page_id, buffer_[frame_id], bpm_latch_,
        lru_k_replacer_, background_scheduler_ 
    };
    return std::optional<GuardedPageWriter>(std::move(guard));
}

std::optional<size_t> BufferManager::GetPinCount(page_id_t page_id) {
    bpm_latch_->lock();
    std::optional<size_t> pin_count = std::nullopt;
    if (page_table_.find(page_id) != page_table_.end()) {
        pin_count = buffer_[page_table_[page_id]]->GetPinCount();
    }
    bpm_latch_->unlock();
    return pin_count;
}