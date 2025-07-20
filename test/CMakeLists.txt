cmake_minimum_required(VERSION 3.14)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

option(VALGRIND "Build with Valgrind" OFF)
find_program(CMAKE_MEMORYCHECK_COMMAND valgrind)
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )

function(add_memcheck_test name binary)
  set(memcheck_command "${CMAKE_MEMORYCHECK_COMMAND} ${CMAKE_MEMORYCHECK_COMMAND_OPTIONS}")
  separate_arguments(memcheck_command)
  add_test(memcheck_${name} ${memcheck_command} ./${binary} ${ARGN})
endfunction(add_memcheck_test)

list(APPEND MYTESTS disk_manager_test buffer_manager_test column_segment_test)
foreach(mytest ${MYTESTS})
  add_executable(${mytest} ${mytest}.cxx)
  target_include_directories(${mytest} PUBLIC
          "${PROJECT_SOURCE_DIR}/include"
  )
  target_link_libraries(${mytest} db GTest::gtest_main)

  if (NOT VALGRIND)
    gtest_discover_tests(${mytest})
  else()
    add_memcheck_test(${mytest} ${mytest})
  endif()
endforeach()
