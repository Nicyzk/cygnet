add_library(db background_scheduler.cxx buffer_manager.cxx disk_manager.cxx lru_k_replacer.cxx page_guard.cxx
column_segment.cxx string_uncompressed.cxx)

target_include_directories(db PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)