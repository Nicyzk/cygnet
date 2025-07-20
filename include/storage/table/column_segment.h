#include "append_state.h"
#include "buffer_manager.h"
#include "common.h"
#include "segment_base.h"

#pragma once

enum class ColumnSegmentType: uint8_t { TRANSIENT, PERSISTENT };

class ColumnSegment: public SegmentBase {
    public:
        std::shared_ptr<BufferManager> buffer_manager_;
        page_id_t page_id_;
        size_t offset_;
        ColumnSegmentType segment_type_;
        size_t segment_size_;

    public:
        ColumnSegment(
            std::shared_ptr<BufferManager> buffer_manager, row_id_t start, 
            size_t count, page_id_t page_id, size_t offset, ColumnSegmentType segment_type, size_t segment_size
        );

        static std::unique_ptr<ColumnSegment> CreateTransientSegment(
            std::shared_ptr<BufferManager> buffer_manager, row_id_t start, idx_t count
        );

        void ConvertToPersistent();

        void InitAppend(ColumnAppendState &append_state);
        size_t Append(ColumnAppendState &append_state, std::vector<std::string> &data);
        void FinalizeAppend(ColumnAppendState &append_state);

        void InitScan(ColumnScanState &scan_state);
        size_t Scan(ColumnScanState &scan_state, std::vector<std::string> &result, size_t count);
        void FinalizeScan();
};