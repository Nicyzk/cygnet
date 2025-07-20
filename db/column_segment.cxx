#include "column_segment.h"
#include "append_state.h"
#include "buffer_manager.h"
#include "string_uncompressed.h"
#include "common.h"
#include <memory.h>
#include <iostream>

ColumnSegment::ColumnSegment(
    std::shared_ptr<BufferManager> buffer_manager, row_id_t start, idx_t count,
    page_id_t page_id, idx_t offset, ColumnSegmentType segment_type, idx_t segment_size 
)
: SegmentBase(start, count), buffer_manager_(buffer_manager), page_id_(page_id), offset_(offset),
segment_type_(segment_type), segment_size_(segment_size) {
    // ColumnSegment must be backed by an allocated page.
    if (page_id == INVALID_PAGE_ID) {
        // handle
        std::cerr << "[ColumnSegment] invalid page id!" << std::endl;
    }
    UncompressedStringStorage::InitSegment(buffer_manager, page_id, segment_size);
    // add compression function
}

std::unique_ptr<ColumnSegment> ColumnSegment::CreateTransientSegment(std::shared_ptr<BufferManager> buffer_manager, row_id_t start, idx_t segment_size) {
    page_id_t page_id = buffer_manager->NewPage();
    return std::make_unique<ColumnSegment>(buffer_manager, start, 0, page_id, 0, ColumnSegmentType::TRANSIENT, segment_size);
}

void ColumnSegment::InitAppend(ColumnAppendState &append_state) {
    append_state.write_guard = UncompressedStringStorage::InitAppend(*this);
}

idx_t ColumnSegment::Append(ColumnAppendState &append_state, std::vector<std::string> &data) {
    return UncompressedStringStorage::Append(append_state, *this, data);
}

void ColumnSegment::FinalizeAppend(ColumnAppendState &append_state) {
    // destroy the append state
    UncompressedStringStorage::FinalizeAppend();
    append_state.write_guard.reset();
}

void ColumnSegment::InitScan(ColumnScanState &scan_state) {
    scan_state.read_guard = UncompressedStringStorage::InitScan(*this);
}

idx_t ColumnSegment::Scan(ColumnScanState &scan_state, std::vector<std::string> &result, idx_t count) {
    return UncompressedStringStorage::Scan(scan_state, *this, result, count);
}