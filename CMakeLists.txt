cmake_minimum_required(VERSION 3.28.3)

project(Cygnet
        VERSION 0.0.0
        DESCRIPTION "A Database System"
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

option(ASAN "Build with AddressSanitizer" OFF)

if (ASAN) 
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -g")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -g")
endif()

enable_testing()

add_subdirectory(db)
add_subdirectory(test)
