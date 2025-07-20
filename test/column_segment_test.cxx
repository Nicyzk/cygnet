// Target MVP:
// // Pseudocode
// auto segment = ColumnSegment::Create(CompressionType::RLE);
// segment.Append({1, 1, 1, 2, 2});
// segment.FlushToDisk(buffer_manager);

// auto segment2 = ColumnSegment::LoadFromDisk(buffer_manager, page_id);
// segment2.Scan(); // -> {1, 1, 1, 2, 2}

#include "gtest/gtest.h"
#include <filesystem>
#include "append_state.h"
#include "column_segment.h"
#include "common.h"

std::filesystem::path db_path(DB_PATH);

TEST(ColumnSegmentTest, VeryBasicTest) {
  std::filesystem::remove(db_path); // always start with new db

  // A very basic test.
  auto disk_manager = std::make_shared<DiskManager>(db_path, PAGE_SIZE);
  auto bpm = std::make_shared<BufferManager>(NUM_BUFFER_FRAMES, disk_manager.get(), K_DIST);

  auto column_segment = ColumnSegment::CreateTransientSegment(bpm, 0, disk_manager->GetPageSize());
  auto append_state = ColumnAppendState();
  
  std::vector<std::string> data{
    "a", "b", "c", "d", "e",
    "f", "g", "h", "i", "j"
  };
  column_segment->InitAppend(append_state);
  column_segment->Append(append_state, data);
  column_segment->FinalizeAppend(append_state);

  std::vector<std::string> result;
  result.resize(data.size());

  auto scan_state = ColumnScanState();
  column_segment->InitScan(scan_state);
  column_segment->Scan(scan_state, result, 10);
  scan_state.read_guard.reset();

  for (int i=0; i<data.size(); i++) {
    ASSERT_EQ(result[i], data[i]);
  }
}

