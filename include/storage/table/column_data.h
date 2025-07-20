#include "common.h"
#include "buffer_manager.h"
#include "append_state.h"
#include "column_segment_tree.h"
#include <memory>
#include "storage/data_pointer.h"

#pragma once

struct PersistentColumnData;

// Manages a column within a row group, which comprises >= 1 column segments
class ColumnData {
    public:
        ColumnData(std::shared_ptr<BufferManager> buffer_manager, idx_t column_index, row_id_t start_row);

        idx_t start_row;
        idx_t column_index;
        std::shared_ptr<BufferManager> buffer_manager;
        idx_t count;

    protected:
        void InitializeAppend(ColumnAppendState &append_state);
        void AppendData(ColumnAppendState &append_state, std::vector<std::string> &data);

        void AppendTransientSegment(idx_t start_row);

        void BeginScanVectorInternal(ColumnScanState &scan_state);
        idx_t ScanVector(ColumnScanState &scan_state, std::vector<std::string> result);

        // Use the data pointers in the PersistentColumnData to initialize/load the current ColumnData
        void InitializeColumn(PersistentColumnData &column_data);

    private:
        ColumnSegmentTree data;
};


struct PersistentColumnData {
    PersistentColumnData(std::vector<DataPointer> pointers);

    PersistentColumnData(const PersistentColumnData& other) = delete;
    PersistentColumnData& operator=(const PersistentColumnData& other) = delete;

    // remember to override this! And why can you override?
    PersistentColumnData(PersistentColumnData&& that) = default;
    PersistentColumnData& operator=(PersistentColumnData&& that) = default; 

    ~PersistentColumnData();

    std::vector<DataPointer> pointers;
    bool has_updates = false;

    // Saves the pointers to a file
    void Serialize() const;

    // Loads the column data from a file using the data pointers
    void Deserialize();
};
