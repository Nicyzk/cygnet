#include "gtest/gtest.h"
#include "common.h"
#include "disk_manager.h"
#include "buffer_manager.h"
#include "page_guard.h"
#include <thread>

std::filesystem::path db_path(DB_PATH);

void CopyString(char *dest, const std::string &src) {
  EXPECT_LE(src.length() + 1, PAGE_SIZE);
  snprintf(dest, PAGE_SIZE, "%s", src.c_str());
}

TEST(BufferPoolManagerTest, DISABLED_VeryBasicTest) {
  // A very basic test.
  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(NUM_BUFFER_FRAMES, disk_manager.get(), K_DIST);

  const page_id_t pid = bpm->NewPage();
  const std::string str = "Hello, world!";

  // Check `WritePageGuard` basic functionality.
  {
    auto guard_opt = bpm->GetGuardedPageWriterNoCheck(pid);
    if (!guard_opt.has_value()) {
      std::cerr << "Failed to get GuardedPageWriter" << std::endl;
      return;
    }
    auto guard = std::move(guard_opt.value());
    CopyString(guard.GetDataMut(), str);
    EXPECT_STREQ(guard.GetData(), str.c_str());
  }

  // Check `ReadPageGuard` basic functionality.
  {
    auto guard_opt = bpm->GetGuardedPageReaderNoCheck(pid);
    if (!guard_opt.has_value()) {
      std::cerr << "Failed to get GuardedPageReader" << std::endl;
      return;
    }
    auto guard = std::move(guard_opt.value());
    EXPECT_STREQ(guard.GetData(), str.c_str());
  }

  // Check `ReadPageGuard` basic functionality (again).
  {
    auto guard_opt = bpm->GetGuardedPageReaderNoCheck(pid);
    if (!guard_opt.has_value()) {
      std::cerr << "Failed to get GuardedPageReader" << std::endl;
      return;
    }
    auto guard = std::move(guard_opt.value());
    EXPECT_STREQ(guard.GetData(), str.c_str());
  }

  ASSERT_TRUE(bpm->DeletePage(pid));
}


TEST(BufferPoolManagerTest, DISABLED_PagePinEasyTest) {
  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(2, disk_manager.get(), 5);

  const page_id_t pageid0 = bpm->NewPage();
  const page_id_t pageid1 = bpm->NewPage();

  const std::string str0 = "page0";
  const std::string str1 = "page1";
  const std::string str0updated = "page0updated";
  const std::string str1updated = "page1updated";

  {
    auto page0_write_opt = bpm->GetGuardedPageWriterNoCheck(pageid0);
    ASSERT_TRUE(page0_write_opt.has_value());
    auto page0_write = std::move(page0_write_opt.value());
    CopyString(page0_write.GetDataMut(), str0);

    auto page1_write_opt = bpm->GetGuardedPageWriterNoCheck(pageid1);
    ASSERT_TRUE(page1_write_opt.has_value());
    auto page1_write = std::move(page1_write_opt.value());
    CopyString(page1_write.GetDataMut(), str1);

    ASSERT_EQ(1, bpm->GetPinCount(pageid0));
    ASSERT_EQ(1, bpm->GetPinCount(pageid1));

    const auto temp_page_id1 = bpm->NewPage();
    const auto temp_page1_opt = bpm->GetGuardedPageReaderNoCheck(temp_page_id1);
    ASSERT_FALSE(temp_page1_opt.has_value());

    const auto temp_page_id2 = bpm->NewPage();
    const auto temp_page2_opt = bpm->GetGuardedPageWriterNoCheck(temp_page_id2);
    ASSERT_FALSE(temp_page2_opt.has_value());

    ASSERT_EQ(1, bpm->GetPinCount(pageid0));
    page0_write.Drop();
    ASSERT_EQ(0, bpm->GetPinCount(pageid0));

    ASSERT_EQ(1, bpm->GetPinCount(pageid1));
    page1_write.Drop();
    ASSERT_EQ(0, bpm->GetPinCount(pageid1));
  }

  {
    const auto temp_page_id1 = bpm->NewPage();
    const auto temp_page1_opt = bpm->GetGuardedPageReaderNoCheck(temp_page_id1);
    ASSERT_TRUE(temp_page1_opt.has_value());

    const auto temp_page_id2 = bpm->NewPage();
    const auto temp_page2_opt = bpm->GetGuardedPageWriterNoCheck(temp_page_id2);
    ASSERT_TRUE(temp_page2_opt.has_value());

    ASSERT_FALSE(bpm->GetPinCount(pageid0).has_value());
    ASSERT_FALSE(bpm->GetPinCount(pageid1).has_value());
  }

  {
    auto page0_write_opt = bpm->GetGuardedPageWriterNoCheck(pageid0);
    ASSERT_TRUE(page0_write_opt.has_value());
    auto page0_write = std::move(page0_write_opt.value());
    EXPECT_STREQ(page0_write.GetData(), str0.c_str());
    CopyString(page0_write.GetDataMut(), str0updated);

    auto page1_write_opt = bpm->GetGuardedPageWriterNoCheck(pageid1);
    ASSERT_TRUE(page1_write_opt.has_value());
    auto page1_write = std::move(page1_write_opt.value());
    EXPECT_STREQ(page1_write.GetData(), str1.c_str());
    CopyString(page1_write.GetDataMut(), str1updated);

    ASSERT_EQ(1, bpm->GetPinCount(pageid0));
    ASSERT_EQ(1, bpm->GetPinCount(pageid1));
  }

  ASSERT_EQ(0, bpm->GetPinCount(pageid0));
  ASSERT_EQ(0, bpm->GetPinCount(pageid1));

  {
    auto page0_read_opt = bpm->GetGuardedPageReaderNoCheck(pageid0);
    ASSERT_TRUE(page0_read_opt.has_value());
    const auto page0_read = std::move(page0_read_opt.value());
    EXPECT_STREQ(page0_read.GetData(), str0updated.c_str());

    auto page1_read_opt = bpm->GetGuardedPageReaderNoCheck(pageid1);
    ASSERT_TRUE(page1_read_opt.has_value());
    const auto page1_read = std::move(page1_read_opt.value());
    EXPECT_STREQ(page1_read.GetData(), str1updated.c_str());

    ASSERT_EQ(1, bpm->GetPinCount(pageid0));
    ASSERT_EQ(1, bpm->GetPinCount(pageid1));
  }

  ASSERT_EQ(0, bpm->GetPinCount(pageid0));
  ASSERT_EQ(0, bpm->GetPinCount(pageid1));

  remove(db_path);
}

TEST(BufferPoolManagerTest, DISABLED_PagePinMediumTest) {
  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(NUM_BUFFER_FRAMES, disk_manager.get(), K_DIST);

  // Scenario: The buffer pool is empty. We should be able to create a new page.
  const auto pid0 = bpm->NewPage();
  auto page0_opt = bpm->GetGuardedPageWriterNoCheck(pid0);
  ASSERT_TRUE(page0_opt.has_value());
  auto page0 = std::move(page0_opt.value());

  // Scenario: Once we have a page, we should be able to read and write content.
  const std::string hello = "Hello";
  CopyString(page0.GetDataMut(), hello);
  EXPECT_STREQ(page0.GetData(), hello.c_str());

  page0.Drop();

  // Create a vector of unique pointers to page guards, which prevents the guards from getting destructed.
  std::vector<GuardedPageWriter> pages;

  // Scenario: We should be able to create new pages until we fill up the buffer pool.
  for (size_t i = 0; i < NUM_BUFFER_FRAMES; i++) {
    const auto pid = bpm->NewPage();
    auto page_opt = bpm->GetGuardedPageWriterNoCheck(pid);
    ASSERT_TRUE(page_opt.has_value());
    pages.push_back(std::move(page_opt.value()));
  }

  // Scenario: All of the pin counts should be 1.
  for (const auto &page : pages) {
    const auto pid = page.GetPageId();
    EXPECT_EQ(1, bpm->GetPinCount(pid));
  }

  // Scenario: Once the buffer pool is full, we should not be able to create any new pages.
  for (size_t i = 0; i < NUM_BUFFER_FRAMES; i++) {
    const auto pid = bpm->NewPage();
    const auto fail = bpm->GetGuardedPageWriterNoCheck(pid);
    ASSERT_FALSE(fail.has_value());
  }

  // Scenario: Drop the first 5 pages to unpin them.
  for (size_t i = 0; i < NUM_BUFFER_FRAMES / 2; i++) {
    const auto pid = pages[0].GetPageId();
    EXPECT_EQ(1, bpm->GetPinCount(pid));
    pages.erase(pages.begin());
    EXPECT_EQ(0, bpm->GetPinCount(pid));
  }

  // Scenario: All of the pin counts of the pages we haven't dropped yet should still be 1.
  for (const auto &page : pages) {
    const auto pid = page.GetPageId();
    EXPECT_EQ(1, bpm->GetPinCount(pid));
  }

  // Scenario: After unpinning pages {1, 2, 3, 4, 5}, we should be able to create 4 new pages and bring them into
  // memory. Bringing those 4 pages into memory should evict the first 4 pages {1, 2, 3, 4} because of LRU.
  for (size_t i = 0; i < ((NUM_BUFFER_FRAMES / 2) - 1); i++) {
    const auto pid = bpm->NewPage();
    auto page_opt = bpm->GetGuardedPageWriterNoCheck(pid);
    ASSERT_TRUE(page_opt.has_value());
    pages.push_back(std::move(page_opt.value()));
  }

  // Scenario: There should be one frame available, and we should be able to fetch the data we wrote a while ago.
  {
    const auto original_page_opt = bpm->GetGuardedPageReaderNoCheck(pid0);
    ASSERT_TRUE(original_page_opt.has_value());
    EXPECT_STREQ(original_page_opt.value().GetData(), hello.c_str());
  }

  // Scenario: Once we unpin page 0 and then make a new page, all the buffer pages should now be pinned. Fetching page 0
  // again should fail.
  const auto last_pid = bpm->NewPage();
  const auto last_page_opt = bpm->GetGuardedPageReaderNoCheck(last_pid);
  ASSERT_TRUE(last_page_opt.has_value());

  const auto fail = bpm->GetGuardedPageReaderNoCheck(pid0);
  ASSERT_FALSE(fail.has_value());

  // Shutdown the disk manager (skip) and remove the temporary file we created.
  remove(db_path);
}


TEST(BufferPoolManagerTest, DISABLED_PageAccessTest) {
  const size_t rounds = 50;

  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(1, disk_manager.get(), K_DIST);

  const auto pid = bpm->NewPage();
  char buf[PAGE_SIZE];

  auto thread = std::thread([&]() {
    // The writer can keep writing to the same page.
    for (size_t i = 0; i < rounds; i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      auto guard_opt = bpm->GetGuardedPageWriterNoCheck(pid);
      ASSERT_TRUE(guard_opt.has_value());
      CopyString(guard_opt.value().GetDataMut(), std::to_string(i));
    }
  });

  for (size_t i = 0; i < rounds; i++) {
    // Wait for a bit before taking the latch, allowing the writer to write some stuff.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // While we are reading, nobody should be able to modify the data.
    auto guard_opt = bpm->GetGuardedPageReaderNoCheck(pid);
    ASSERT_TRUE(guard_opt.has_value());
    auto guard = std::move(guard_opt.value());
    // Save the data we observe.
    memcpy(buf, guard.GetData(), PAGE_SIZE);

    // Sleep for a bit. If latching is working properly, nothing should be writing to the page.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Check that the data is unmodified.
    EXPECT_STREQ(guard.GetData(), buf);
  }

  thread.join();
}

TEST(BufferPoolManagerTest, DISABLED_ContentionTest) {
  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(NUM_BUFFER_FRAMES, disk_manager.get(), K_DIST);

  const size_t rounds = 100000;

  const auto pid = bpm->NewPage();

  auto thread1 = std::thread([&]() {
    for (size_t i = 0; i < rounds; i++) {
      auto guard_opt = bpm->GetGuardedPageWriterNoCheck(pid);
      ASSERT_TRUE(guard_opt.has_value());
      auto guard = std::move(guard_opt.value());
      CopyString(guard.GetDataMut(), std::to_string(i));
    }
  });

  auto thread2 = std::thread([&]() {
    for (size_t i = 0; i < rounds; i++) {
      auto guard_opt = bpm->GetGuardedPageWriterNoCheck(pid);
      ASSERT_TRUE(guard_opt.has_value());
      auto guard = std::move(guard_opt.value());
      CopyString(guard.GetDataMut(), std::to_string(i));
    }
  });

  auto thread3 = std::thread([&]() {
    for (size_t i = 0; i < rounds; i++) {
      auto guard_opt = bpm->GetGuardedPageWriterNoCheck(pid);
      ASSERT_TRUE(guard_opt.has_value());
      auto guard = std::move(guard_opt.value());
      CopyString(guard.GetDataMut(), std::to_string(i));
    }
  });

  auto thread4 = std::thread([&]() {
    for (size_t i = 0; i < rounds; i++) {
      auto guard_opt = bpm->GetGuardedPageWriterNoCheck(pid);
      ASSERT_TRUE(guard_opt.has_value());
      auto guard = std::move(guard_opt.value());
      CopyString(guard.GetDataMut(), std::to_string(i));
    }
  });

  thread3.join();
  thread2.join();
  thread4.join();
  thread1.join();
}

TEST(BufferPoolManagerTest, DISABLED_DeadlockTest) {
  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(NUM_BUFFER_FRAMES, disk_manager.get(), K_DIST);

  const auto pid0 = bpm->NewPage();
  const auto pid1 = bpm->NewPage();

  auto guard0_opt = bpm->GetGuardedPageWriterNoCheck(pid0);
  ASSERT_TRUE(guard0_opt.has_value());
  auto guard0 = std::move(guard0_opt.value());

  // A crude way of synchronizing threads, but works for this small case.
  std::atomic<bool> start = false;

  auto child = std::thread([&]() {
    // Acknowledge that we can begin the test.
    start.store(true);

    // Attempt to write to page 0.
    auto guard0_opt = bpm->GetGuardedPageWriterNoCheck(pid0);
    ASSERT_TRUE(guard0_opt.has_value());
    auto guard0 = std::move(guard0_opt.value());
  });

  // Wait for the other thread to begin before we start the test.
  while (!start.load()) {
  }

  // Make the other thread wait for a bit.
  // This mimics the main thread doing some work while holding the write latch on page 0.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // If your latching mechanism is incorrect, the next line of code will deadlock.
  // Think about what might happen if you hold a certain "all-encompassing" latch for too long...

  // While holding page 0, take the latch on page 1.
  auto guard1_opt = bpm->GetGuardedPageWriterNoCheck(pid1);
  ASSERT_TRUE(guard1_opt.has_value());
  auto guard1 = std::move(guard1_opt.value());

  // Let the child thread have the page 0 since we're done with it.
  guard0.Drop();

  child.join();
}

TEST(BufferPoolManagerTest, EvictableTest) {
  // Test if the evictable status of a frame is always correct.
  const size_t rounds = 1000;
  const size_t num_readers = 8;

  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  // Only allocate 1 frame of memory to the buffer pool manager.
  auto bpm = std::make_shared<BufferManager>(1, disk_manager.get(), K_DIST);

  for (size_t i = 0; i < rounds; i++) {
    std::mutex mutex;
    std::condition_variable cv;

    // This signal tells the readers that they can start reading after the main thread has already taken the read latch.
    bool signal = false;

    // This page will be loaded into the only available frame.
    const auto winner_pid = bpm->NewPage();
    // We will attempt to load this page into the occupied frame, and it should fail every time.
    const auto loser_pid = bpm->NewPage();

    std::vector<std::thread> readers;
    for (size_t j = 0; j < num_readers; j++) {
      readers.emplace_back([&]() {
        std::unique_lock<std::mutex> lock(mutex);

        // Wait until the main thread has taken a read latch on the page.
        while (!signal) {
          cv.wait(lock);
        }

        // Read the page in shared mode.
        auto read_guard_opt = bpm->GetGuardedPageReaderNoCheck(winner_pid);
        ASSERT_TRUE(read_guard_opt.has_value());
        auto read_guard = std::move(read_guard_opt.value());

        // Since the only frame is pinned, no thread should be able to bring in a new page.
        ASSERT_FALSE(bpm->GetGuardedPageReaderNoCheck(loser_pid).has_value());
      });
    }

    std::unique_lock<std::mutex> lock(mutex);

    if (i % 2 == 0) {
      // Take the read latch on the page and pin it.
      auto read_guard_opt = bpm->GetGuardedPageReaderNoCheck(winner_pid);
      ASSERT_TRUE(read_guard_opt.has_value());
      auto read_guard = std::move(read_guard_opt.value());

      // Wake up all of the readers.
      signal = true;
      cv.notify_all();
      lock.unlock();

      // Allow other threads to read.
      read_guard.Drop();
    } else {
      // Take the read latch on the page and pin it.
      auto write_guard_opt = bpm->GetGuardedPageWriterNoCheck(winner_pid);
      ASSERT_TRUE(write_guard_opt.has_value());
      auto write_guard = std::move(write_guard_opt.value());

      // Wake up all of the readers.
      signal = true;
      cv.notify_all();
      lock.unlock();

      // Allow other threads to read.
      write_guard.Drop();
    }

    for (size_t i = 0; i < num_readers; i++) {
      readers[i].join();
    }
  }
}