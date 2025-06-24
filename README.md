mkdir build
cd build
cmake ..
cmake --build .


For ASAN/LSAN: cmake -Dasan=ON ..
For valgrind: cmake -Dvalgrind=ON ..