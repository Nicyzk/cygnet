#include <vector>
#include <memory>
#include "storage/table/column_data.h"

class RowGroupCollection;

class RowGroup {
    public:

    private:
        RowGroupCollection& collection;
        std::vector<std::shared_ptr<ColumnData>> columns;
};

struct PersistentRowGroupData {
    PersistentRowGroupData() = default;

	// disable copy constructors
	PersistentRowGroupData(const PersistentRowGroupData &other) = delete;
	PersistentRowGroupData &operator=(const PersistentRowGroupData &) = delete;

	//! enable move constructors
	PersistentRowGroupData(PersistentRowGroupData &&other) noexcept = default;
	PersistentRowGroupData &operator=(PersistentRowGroupData &&) = default;

	~PersistentRowGroupData() = default;

    std::vector<PersistentColumnData> column_data;
    idx_t start;
    idx_t count;

    // save to file
    void Serialize() const;

    // load from file
    static PersistentRowGroupData Deserialize();

    bool HasUpdates() const;
};