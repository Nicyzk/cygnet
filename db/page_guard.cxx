#include "page_guard.h"
#include "background_scheduler.h"
#include "buffer_manager.h"
#include "common.h"
#include "lru_k_replacer.h"
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>

/*
 * When using GuardedPageReader/Writer, we assume the following: 
 * 1. If page_id is invalid, it has been deleted since page_ids are monotonically increasing.
 * 2. If page_id is valid, the page must be in-memory. The buffer manager ensures this before getting a page guard.
 */

GuardedPageReader::GuardedPageReader(
    page_id_t page_id, std::shared_ptr<Frame> frame, std::shared_ptr<std::mutex> bpm_latch,
    std::shared_ptr<LRUKReplacer> lru_replacer, std::shared_ptr<Background_Scheduler> background_scheduler
): 
    page_id_(page_id), frame_(frame), bpm_latch_(bpm_latch),
    lru_replacer_(lru_replacer), background_scheduler_(background_scheduler) {
    rlock_ = std::shared_lock<std::shared_mutex>(frame->GetMutex());
    frame_->IncPinCount();
    is_pinned_ = true;
    lru_replacer_->SetNotEvictable(frame_->GetFrameId());
}

GuardedPageReader::~GuardedPageReader() {
    /* Reader has transfered ownership. */
    if (bpm_latch_ == nullptr)
        return;
    
    Drop();
}

GuardedPageReader::GuardedPageReader(GuardedPageReader&& that) noexcept:
    page_id_(that.page_id_),
    frame_(std::move(that.frame_)),
    bpm_latch_(std::move(that.bpm_latch_)),
    lru_replacer_(std::move(that.lru_replacer_)),
    background_scheduler_(std::move(that.background_scheduler_)) {
    /* Invalidate the old reader. */
    this->rlock_ = std::move(that.rlock_);
    this->is_pinned_ = that.is_pinned_;
    that.is_pinned_ = false;
    that.page_id_ = INVALID_PAGE_ID;
    that.frame_ = nullptr;
    that.bpm_latch_ = nullptr;
    that.lru_replacer_ = nullptr;
    that.background_scheduler_ = nullptr;
}

GuardedPageReader& GuardedPageReader::operator=(GuardedPageReader&& that) noexcept {
    /* Self-assignment detection. */
    if (&that == this)
        return *this;

    /* Release resources held by LHS object (this), since it is being replaced. */
    this->Drop();

    /* Transfer ownership */
    this->rlock_ = std::move(that.rlock_);
    this->is_pinned_ = that.is_pinned_;
    that.is_pinned_ = false;
    page_id_ = that.page_id_,
    frame_ = std::move(that.frame_),
    bpm_latch_ = std::move(that.bpm_latch_),
    lru_replacer_ = std::move(that.lru_replacer_),
    background_scheduler_ = std::move(that.background_scheduler_);

    /* Invalidate old reader. */
    that.page_id_ = INVALID_PAGE_ID;
    that.frame_ = nullptr;
    that.bpm_latch_ = nullptr;
    that.lru_replacer_ = nullptr;
    that.background_scheduler_ = nullptr;

    return *this;
}

const char* GuardedPageReader::GetData() const {
    return frame_->GetData();
}

void GuardedPageReader::Drop() {
    if (is_pinned_) {
        frame_->DecPinCount();
        is_pinned_ = false;
        if (frame_->GetPinCount() == 0)
            lru_replacer_->SetEvictable(frame_->GetFrameId());
        rlock_.unlock();
    }
}

GuardedPageWriter::GuardedPageWriter(
    page_id_t page_id, std::shared_ptr<Frame> frame, std::shared_ptr<std::mutex> bpm_latch,
    std::shared_ptr<LRUKReplacer> lru_replacer, std::shared_ptr<Background_Scheduler> background_scheduler
): 
    page_id_(page_id), frame_(frame), bpm_latch_(bpm_latch),
    lru_replacer_(lru_replacer), background_scheduler_(background_scheduler) {
    wlock_ = std::unique_lock<std::shared_mutex>(frame->GetMutex());
    frame->IncPinCount();
    is_pinned_ = true;
    lru_replacer_->SetNotEvictable(frame_->GetFrameId());
}

GuardedPageWriter::~GuardedPageWriter() {
    if (bpm_latch_ == nullptr)
        return;
    
    Drop();
}

GuardedPageWriter::GuardedPageWriter(GuardedPageWriter&& that):
    page_id_(that.page_id_),
    frame_(std::move(that.frame_)),
    bpm_latch_(std::move(that.bpm_latch_)),
    lru_replacer_(std::move(that.lru_replacer_)),
    background_scheduler_(std::move(that.background_scheduler_)) {
    /* Invalidate the old reader. */
    this->wlock_ = std::move(that.wlock_);
    this->is_pinned_ = that.is_pinned_;
    that.is_pinned_ = false;
    that.page_id_ = INVALID_PAGE_ID;
    that.frame_ = nullptr;
    that.bpm_latch_ = nullptr;
    that.lru_replacer_ = nullptr;
    that.background_scheduler_ = nullptr;
}

GuardedPageWriter& GuardedPageWriter::operator=(GuardedPageWriter&& that) {
    /* Self-assignment detection. */
    if (&that == this)
        return *this;

    /* Release resources held by LHS object (this), since it is being replaced. */
    this->Drop();

    /* Transfer ownership */
    this->wlock_ = std::move(that.wlock_);
    this->is_pinned_ = that.is_pinned_;
    that.is_pinned_ = false;
    page_id_ = that.page_id_,
    frame_ = std::move(that.frame_),
    bpm_latch_ = std::move(that.bpm_latch_),
    lru_replacer_ = std::move(that.lru_replacer_),
    background_scheduler_ = std::move(that.background_scheduler_);

    /* Invalidate old reader. */
    that.page_id_ = INVALID_PAGE_ID;
    that.frame_ = nullptr;
    that.bpm_latch_ = nullptr;
    that.lru_replacer_ = nullptr;
    that.background_scheduler_ = nullptr;

    return *this;
}

const char* GuardedPageWriter::GetData() {
    frame_->SetDirty(true);
    return frame_->GetDataMut();
}

char* GuardedPageWriter::GetDataMut() {
    frame_->SetDirty(true);
    return frame_->GetDataMut();
}

void GuardedPageWriter::FlushPage() {
    std::shared_ptr<Request> write_req = std::make_shared<Request>(false, page_id_, frame_->GetData());
    background_scheduler_->Schedule(write_req);
    try {
        write_req.get()->promise_.get_future().get();
    } catch (const std::exception &e) {
        std::cerr << "[FlushPage] " << e.what();
    }
}

void GuardedPageWriter::Drop() {
    if (is_pinned_) {
        frame_->DecPinCount();
        is_pinned_ = false;
        if (frame_->GetPinCount() == 0)
            lru_replacer_->SetEvictable(frame_->GetFrameId());
        wlock_.unlock();
    }
}

const page_id_t GuardedPageWriter::GetPageId() const {
    return page_id_;
}