#include <cstdint>

#pragma once

#define DB_PATH "/home/nic/Desktop/cygnet/dbfile"
#define PAGE_SIZE 4096
#define DEFAULT_DB_PAGES 1
#define NUM_BACKGROUND_THREADS 1
#define NUM_BUFFER_FRAMES 10
#define INVALID_PAGE_ID -1
#define K_DIST 10

using frame_id_t = int32_t;
using page_id_t = int32_t;
using row_id_t = int32_t;
using idx_t = std::size_t;