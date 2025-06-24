#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "disk_manager.h"
#include "common.h"

DiskManager::DiskManager(const std::filesystem::path &db_path): db_path_(db_path) {
    db_io_.open(db_path, std::ios::binary | std::ios::out | std::ios::in);

    if (!db_io_.is_open()) {
        db_io_.clear();
        db_io_.open(db_path, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
        if (!db_io_.is_open()) {
            std::cerr << "[DiskManager] failed to create/open file!" << std::endl;
            return;
        }
        std::filesystem::resize_file(db_path_, DEFAULT_DB_PAGES * PAGE_SIZE);
        db_capacity_ = DEFAULT_DB_PAGES;
        // TODO: handle resize error
    }
}

// DiskManager::~DiskManager()

/* Allocates a new page. */
void DiskManager::AllocatePage(page_id_t next_page_id) {
    db_io_latch_.lock();

    size_t page_pos;
    size_t num_pages = pages_.size();

    /* If a free slot exists, use it. */
    if (!free_slots_.empty()) {
        page_pos = free_slots_.back();
        free_slots_.pop_back();
        pages_.insert({next_page_id, page_pos * PAGE_SIZE});
        db_io_latch_.unlock();
        return;
    }

    /* Increase (double) capacity if required. */
    if (num_pages + 1 > db_capacity_) {
        db_capacity_ *= 2;
        std::filesystem::resize_file(db_path_, db_capacity_ * PAGE_SIZE);
    }

    page_pos = num_pages;
    pages_.insert({next_page_id, page_pos * PAGE_SIZE});
    db_io_latch_.unlock();
}

void DiskManager::ReadPage(page_id_t page_id, char* data) {
    /* If page has not been allocated, throw error. */
    if (pages_.find(page_id) == pages_.end()) {
        std::cerr << "[ReadPage] reading from unallocated page!" << std::endl;
        return;
    }

    /* Seek to required page. */
    db_io_latch_.lock();
    db_io_.seekg(pages_[page_id], std::ios::beg);
    db_io_.read(data, PAGE_SIZE);
    db_io_latch_.unlock();

    if (db_io_.bad()) {
        std::cerr << "[ReadPage] error while reading page!" << std::endl;
        return;
    }

    /* Should never happen: encounter EOF in middle of page. */
    if (db_io_.gcount() < PAGE_SIZE) {
        std::cerr << "[ReadPage] read less than a full page!" << std::endl;
        return;
    }
}

void DiskManager::WritePage(page_id_t page_id, const char* data) {
    /* If page has not been allocated, page was allocated in-memory and now flushed. Allocate a page first. */
    if (pages_.find(page_id) == pages_.end()) {
        AllocatePage(page_id);
    }

    /* Overwrite data. */
    db_io_latch_.lock();
    db_io_.seekp(pages_[page_id], std::ios::beg);
    db_io_.write(data, PAGE_SIZE);
    db_io_latch_.unlock();

    if (db_io_.bad()) {
        std::cerr << "[WritePage] failed to write to page!" << std::endl;
        return;
    }

    db_io_.flush();
}

void DiskManager::DeletePage(page_id_t page_id) {
    /* It is possible that page has only been allocated in-memory, and does not exist on disk. */
    if (pages_.find(page_id) == pages_.end())
        return;

    /* Free page, no need to zero data. */
    db_io_latch_.lock();
    pages_.erase(page_id);
    free_slots_.emplace_back(page_id);
    db_io_latch_.unlock();
}

bool DiskManager::CheckPageExists(page_id_t page_id) {
    return (pages_.find(page_id) != pages_.end());
}