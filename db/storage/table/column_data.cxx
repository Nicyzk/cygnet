#include "storage/table/column_data.h"
#include "column_segment.h"
#include <exception>

ColumnData::ColumnData(std::shared_ptr<BufferManager> buffer_manager, idx_t column_index, row_id_t start_row)
: buffer_manager(buffer_manager), column_index(column_index), start_row(start_row) {

}

void ColumnData::InitializeAppend(ColumnAppendState &append_state) {
    if (data.IsEmpty()) {
        AppendTransientSegment(start_row);
    }

    if (segment->segment_type_ == ColumnSegmentType::PERSISTENT) {
        idx_t num_rows = segment->start + segment->count;
        AppendTransientSegment(num_rows);
    }

    state.current = data.GetLastSegment();
    state.current->InitializeAppend(state);
}

void ColumnData::AppendData(ColumnAppendState &state, std::vector<std::string> &data) {
    idx_t append_count = data.size();
    this->count += append_count;

    while (true) {
        idx_t copied_elements = state.current->Append(state, data);
        if (copied_elements == append_count) {
            break;
        }

        // Could not fit data into the current column segment, so we create a new one.
        AppendTransientSegment(state.current->start + state.current->count);
        state.current = data.GetLastSegment();
        state.current->InitializeAppend(state);

        // should be added but why
    }
}


void PersistentColumnData::Serialize() const {
    if (has_updates) {
        throw "Column data with updates cannot be serialized";
    }

    // write data pointers of this persistent column data to file
    file.write(pointers);
}

void PersistentColumnData::Deserialize() {

}

void PersistentRowGroupData::Serialize() const {
    // internally I belive Serializer.WriteProperty will call check that column_data is a list, and call Serialize method on ecah object
    file.write("columns", column_data);
    file.write("start", start);
    file.write("count", count);
}

PersistentRowGroupData PersistentRowGroupData::Deserialize() {
    PersistentRowGroupData data;
    return data;
}