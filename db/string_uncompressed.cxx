#include "string_uncompressed.h"
#include "buffer_manager.h"
/*
 * Storage layout relative to page start pointer
 * 0x0-0x4: [dictionary_size]
 * 0x4-0x8: [dictionary_end] -> Offset from page START to end of dictionary. In this case, end of page.
 * 0x8-... : [offsets] -> Offsets from page END to start of corresponding item. Idx into result_data is implicitly Row Idx.
 * [...] free space
 * [dictionary_end-2]: "db"
 */

void UncompressedStringStorage::InitSegment(
    std::shared_ptr<BufferManager> buffer_manager, page_id_t page_id, idx_t segment_size
) {
    auto page_writer = buffer_manager->GetGuardedPageWriter(page_id);
    char* ptr = page_writer.GetDataMut();
    uint32_t* dictionary_size = reinterpret_cast<uint32_t*>(ptr);
    uint32_t* dictionary_end = reinterpret_cast<uint32_t*>(ptr + sizeof(uint32_t));
    *dictionary_size = 0;
    *dictionary_end = segment_size;
}

std::unique_ptr<GuardedPageWriter> UncompressedStringStorage::InitAppend(ColumnSegment &segment) {
    auto page_writer = segment.buffer_manager_->GetGuardedPageWriter(segment.page_id_);
    return std::make_unique<GuardedPageWriter>(std::move(page_writer));
}

/* Current assumption: no overflow blocks allowed. */
idx_t UncompressedStringStorage::Append(ColumnAppendState &append_state, ColumnSegment &segment, std::vector<std::string> &data) {
    char* ptr = append_state.write_guard->GetDataMut();
    idx_t count = data.size();
    int32_t* offsets = reinterpret_cast<int32_t*>(ptr + DICTIONARY_HEADER_SIZE);
    uint32_t* dictionary_size = reinterpret_cast<uint32_t*>(ptr);
    uint32_t* dictionary_end = reinterpret_cast<uint32_t*>(ptr + sizeof(uint32_t));
    
    idx_t remaining_space = RemainingSpace(append_state, segment);
    auto segment_count = segment.count.load();
    for (int i=0; i<count; i++) {
        if (remaining_space <= sizeof(uint32_t)) {
            segment.count += i;
            return i;
        }

        remaining_space -= sizeof(uint32_t);
        char* end = ptr + *dictionary_end;

        // TODO: allow overflow blocks
        idx_t string_length = data[i].size();
        if (remaining_space < string_length) {
            segment.count += i;
            return i;
        }

        *dictionary_size += string_length;
        remaining_space -= string_length;

        char* dict_pos = end - *dictionary_size;
        std::memcpy(dict_pos, data[i].c_str(), string_length);

        offsets[segment_count + i] = static_cast<int32_t>(*dictionary_size);
    }

    segment.count += count;
    return count;
}

void UncompressedStringStorage::FinalizeAppend() {
    // Duckdb removes the unused space between header and dictionary. Not necessary for now.
}

idx_t UncompressedStringStorage::RemainingSpace(ColumnAppendState &append_state, ColumnSegment &segment) {
    uint32_t dictionary_size = *reinterpret_cast<uint32_t *>(append_state.write_guard->GetDataMut());
    idx_t used_space = dictionary_size + segment.count * sizeof(int32_t) + DICTIONARY_HEADER_SIZE;
    idx_t remaining_space = segment.segment_size_ - used_space;
    return remaining_space;
}

std::unique_ptr<GuardedPageReader> UncompressedStringStorage::InitScan(ColumnSegment &segment) {
    auto page_reader = segment.buffer_manager_->GetGuardedPageReader(segment.page_id_);
    return std::make_unique<GuardedPageReader>(std::move(page_reader));
}

idx_t UncompressedStringStorage::Scan(ColumnScanState &scan_state, ColumnSegment &segment,
    std::vector<std::string> &result, idx_t count) {
    assert(result.size() >= count);
    const char* ptr  = scan_state.read_guard->GetData();
    const int32_t* offsets = reinterpret_cast<const int32_t*>(ptr + DICTIONARY_HEADER_SIZE);
    const uint32_t* dictionary_size = reinterpret_cast<const uint32_t*>(ptr);
    const uint32_t* dictionary_end = reinterpret_cast<const uint32_t*>(ptr + sizeof(uint32_t));

    int32_t previous_offset = 0;
    idx_t scan_count = std::min(count, segment.count.load());
    for (idx_t i=0; i<scan_count; i++) {
        int32_t current_offset = offsets[i];
        idx_t string_length = current_offset - previous_offset;
        result[i] = std::string(ptr + *dictionary_end - current_offset, string_length);
        previous_offset = current_offset;
    }
    return scan_count;
}