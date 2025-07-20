#include "page_guard.h"

#pragma once

class ColumnSegment;
class ColumnSegmentTree;

struct ColumnAppendState {
    std::unique_ptr<GuardedPageWriter> write_guard;
    ColumnSegment* current = nullptr;
};

struct ColumnScanState {
    std::unique_ptr<GuardedPageReader> read_guard;

};