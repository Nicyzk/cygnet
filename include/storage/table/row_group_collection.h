#include <memory>
#include "storage/table/row_group_segment_tree.h"

class RowGroupCollection {
    public:
        // RowGroupCollection(DataTable info);

    private:
        std::shared_ptr<RowGroupSegmentTree> row_groups;
};

struct PersistentCollectionData {
    PersistentCollectionData() = default;
    // disable copy constructors
	PersistentCollectionData(const PersistentCollectionData &other) = delete;
	PersistentCollectionData &operator=(const PersistentCollectionData &) = delete;
	//! enable move constructors
	PersistentCollectionData(PersistentCollectionData &&other) noexcept = default;
	PersistentCollectionData &operator=(PersistentCollectionData &&) = default;
	~PersistentCollectionData() = default;

    std::vector<PersistentRowGroupData> row_group_data;

    void Serialize() const;
    static PersistentCollectionData Deserialize();
    bool HasUpdates() const;
};