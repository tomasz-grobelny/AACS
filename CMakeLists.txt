cmake_minimum_required(VERSION 2.8)
project (AAServer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.67 REQUIRED COMPONENTS filesystem)
pkg_check_modules (LIBUSBGX REQUIRED IMPORTED_TARGET libusbgx)

include_directories(/opt/linux/include/uapi/linux/usb/)
include_directories(include)

add_executable(AAServer main.cpp 
    src/utils.cpp 
    src/descriptors.cpp
    src/Library.cpp
    src/Function.cpp
    src/MassStorageFunction.cpp
    src/FfsFunction.cpp
    src/Configuration.cpp
    src/Gadget.cpp
    src/Udc.cpp
    src/ModeSwitcher.cpp
)

target_link_libraries(AAServer Boost::filesystem)
target_link_libraries(AAServer PkgConfig::LIBUSBGX)
include_directories(${LIBUSBGX_INCLUDE_DIRS})
