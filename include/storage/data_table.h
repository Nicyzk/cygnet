#include <memory>
#include "table/persistent_table_data.h"

class DataTable {
    public:
        DataTable(std::unique_ptr<PersistentTableData> data = nullptr);

        
};