#include "../common.h"

// Represents a column segment stored on disk. Allows for saving/loading.
struct DataPointer {
    explicit DataPointer();

    DataPointer(const DataPointer& other) = delete;
    DataPointer& operator=(const DataPointer& other) = delete;

    DataPointer(DataPointer&& that) = default;
    DataPointer& operator=(DataPointer&& that) = default;

    idx_t row_start;
    idx_t count;

    // Saves/serializes the data pointer to disk
    void Serialize() const;

    // Loads/deserializes the data pointer from disk
    void Deserialize();

    // CompressionType compression-type;
    // BaseStatistics stats;
};