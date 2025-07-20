#include "common.h"
#include "filesystem"
#include "fstream"
#include <vector>
#include <unordered_map>
#include <mutex>

#pragma once

class DiskManager {
    private:
        std::filesystem::path db_path_;
        std::fstream db_io_;
        std::unordered_map<page_id_t, size_t> pages_; /* maps page_ids to offsets */
        std::vector<size_t> free_slots_;
        size_t db_capacity_;
        std::mutex db_io_latch_;
        const idx_t page_size_;

    public:
        DiskManager(const std::filesystem::path &db_path, idx_t page_size);

        ~DiskManager() = default;

        void AllocatePage(page_id_t next_page_id);
        
        void WritePage(page_id_t, const char* data);
        
        void ReadPage(page_id_t page_id, char* data);

        inline std::unordered_map<page_id_t, size_t>& GetPages() {
            return pages_;
        }

        void DeletePage(page_id_t);

        bool CheckPageExists(page_id_t);

        idx_t GetPageSize() { return page_size_; }
};