#include "append_state.h"
#include "buffer_manager.h"
#include "column_segment.h"
#include "common.h"
#include "page_guard.h"
#include <cassert>
#include <cstdint>

#pragma once

/*
 * Storage layout relative to page start pointer
 * 0x0-0x4: [dictionary_size]
 * 0x4-0x8: [dictionary_end] -> Points to end of dictionary. In this case, end of page.
 * 0x8-... : [offsets] -> Offsets from page start to corresponding value. Idx into result_data is implicitly Row Idx.
 * [...] free space
 * [dictionary_end-2]: "db"
 */
struct UncompressedStringStorage {
    public:
        static constexpr uint16_t DICTIONARY_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint32_t);
        
    public:
        static void InitSegment(std::shared_ptr<BufferManager> buffer_manager, page_id_t page_id, idx_t segment_size); 

        static std::unique_ptr<GuardedPageWriter> InitAppend(ColumnSegment &segment);

        /* Current assumption: no overflow blocks allowed. */
        static idx_t Append(ColumnAppendState &append_state, ColumnSegment &segment, std::vector<std::string> &data);

        static void FinalizeAppend();

        static idx_t RemainingSpace(ColumnAppendState &append_state, ColumnSegment &segment);

        static std::unique_ptr<GuardedPageReader> InitScan(ColumnSegment &segment);

        static idx_t Scan(ColumnScanState &scan_state, ColumnSegment &segment,
            std::vector<std::string> &result, idx_t count);
};