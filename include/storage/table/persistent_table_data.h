#include "common.h"

class PersistentTableData {
    public:
        idx_t total_rows;
        idx_t row_group_count;
        MetaBlockPointer block_pointer;
};