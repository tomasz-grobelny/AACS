cmake_minimum_required(VERSION 2.8)
project (AAClient)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(fmt REQUIRED)
pkg_check_modules (libusb REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules (gst REQUIRED IMPORTED_TARGET gstreamer-base-1.0)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${libusb_INCLUDE_DIR} include)
include_directories(${gst_INCLUDE_DIRS})

include(${CMAKE_CURRENT_SOURCE_DIR}/../proto/CMakeLists.txt)

add_executable(AAClient main.cpp 
    src/Library.cpp
    src/Device.cpp
    src/AaCommunicator.cpp
    ../src/utils.cpp
    src/Message.cpp
    src/ChannelHandler.cpp
    src/DefaultChannelHandler.cpp
    src/VideoChannelHandler.cpp
    src/InputChannelHandler.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    )
target_link_libraries(AAClient Threads::Threads)
target_link_libraries(AAClient PkgConfig::libusb)
target_link_libraries(AAClient PkgConfig::gst)
target_link_libraries(AAClient OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(AAClient protobuf::libprotobuf)
target_link_libraries(AAClient fmt::fmt)

add_custom_command(OUTPUT dhparam.pem COMMAND openssl dhparam -out dhparam.pem 2048 > /dev/null 2>&1)
add_custom_target(dhparam_aaclient DEPENDS dhparam.pem)
add_dependencies(AAClient dhparam_aaclient)
add_custom_command(TARGET AAClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/ssl/headunit.crt
            ${CMAKE_CURRENT_BINARY_DIR}/headunit.crt)
add_custom_command(TARGET AAClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/ssl/headunit.key
            ${CMAKE_CURRENT_BINARY_DIR}/headunit.key)
