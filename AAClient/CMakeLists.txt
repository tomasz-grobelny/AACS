cmake_minimum_required(VERSION 2.8)
project (AAClient)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules (libusb REQUIRED IMPORTED_TARGET libusb-1.0)

include_directories(${libusb_INCLUDE_DIR} include)
add_executable(AAClient main.cpp 
    src/Library.cpp
    src/Device.cpp
    src/AaCommunicator.cpp
    src/utils.cpp
    src/Message.cpp
    )
target_link_libraries(AAClient Threads::Threads)
target_link_libraries(AAClient PkgConfig::libusb)
target_link_libraries(AAClient OpenSSL::SSL OpenSSL::Crypto)
