#include "common.h"
#include "disk_manager.h"
#include <gtest/gtest.h>
#include <memory>

std::filesystem::path db_path(DB_PATH);

class DiskManagerTest: public testing::Test {
    protected:
        std::unique_ptr<DiskManager> dm_;

        void CreateDB() {
            std::filesystem::remove(db_path);
            dm_ = std::make_unique<DiskManager>(db_path, PAGE_SIZE);
        }

        void OpenDB() {
            dm_ = std::make_unique<DiskManager>(db_path, PAGE_SIZE);
        }
};

/* tests that db file is created and is preallocated the default number of pages */
TEST_F(DiskManagerTest, DISABLED_CreateDBTest) {
    CreateDB();
    EXPECT_EQ(std::filesystem::file_size(db_path), DEFAULT_DB_PAGES * PAGE_SIZE);

    /* Allocate pages until the db file doubles */
    const std::unordered_map<page_id_t, size_t>& pages = dm_->GetPages();
    for (int i=0; i< (DEFAULT_DB_PAGES + 1); i++) {
        dm_->AllocatePage(i);
        ASSERT_EQ(pages.size(), i+1);
        if (i < DEFAULT_DB_PAGES)
            ASSERT_EQ(std::filesystem::file_size(db_path), DEFAULT_DB_PAGES * PAGE_SIZE);
        else
            ASSERT_EQ(std::filesystem::file_size(db_path), 2 * DEFAULT_DB_PAGES * PAGE_SIZE);;
    }
    remove(db_path);
}

TEST_F(DiskManagerTest, DISABLED_ReadWriteTest) {
    CreateDB();

    char data[PAGE_SIZE] = "Hello";
    char buf[PAGE_SIZE] = {0};
    dm_->AllocatePage(0);

    dm_->WritePage(0, data);
    dm_->ReadPage(0, buf);
    ASSERT_EQ(memcmp(data, buf, sizeof(data)), 0);
    remove(db_path);
}


/* TODO: tests on free slots allocation */

/* TODO: tests on existing file */