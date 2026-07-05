# PlayVault

PlayVault is a desktop app written in C++ and GTKmm for managing a local library of PlayStation games. The program reads game identification codes (e.g., CUSAXXXXX) and asynchronously retrieves the actual titles via web scraping, without blocking the graphical user interface.

## Dipendences
- C++17 o superiore
- GTKmm
- libcurl

## How to compile
mkdir build
cd build
cmake ..
make
./PlayVault