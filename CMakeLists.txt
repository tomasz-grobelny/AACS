cmake_minimum_required(VERSION 2.8)
project (AAServer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.67 REQUIRED COMPONENTS filesystem)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
pkg_check_modules (LIBUSBGX REQUIRED IMPORTED_TARGET libusbgx)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(/opt/linux/include/uapi/linux/usb/)
include_directories(include)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS 
    proto/MediaStreamType.proto
    proto/MediaChannel.proto
    proto/Channel.proto
    proto/ChannelOpenRequest.proto
    proto/ServiceDiscoveryRequest.proto
    proto/ServiceDiscoveryResponse.proto
)

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
    src/Message.cpp
    src/ModeSwitcher.cpp
    src/AaCommunicator.cpp
    src/SocketClient.cpp
    src/SocketCommunicator.cpp
    src/ManualResetEvent.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(AAServer Threads::Threads)
target_link_libraries(AAServer Boost::filesystem)
target_link_libraries(AAServer PkgConfig::LIBUSBGX)
target_link_libraries(AAServer OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(AAServer protobuf::libprotobuf)
include_directories(${LIBUSBGX_INCLUDE_DIRS})

add_custom_command(OUTPUT self_sign.key COMMAND openssl genrsa -out self_sign.key 2048 > /dev/null 2>&1)
add_custom_target(sskey DEPENDS self_sign.key)

add_custom_command(OUTPUT self_sign.crt COMMAND openssl req -x509 -new -nodes -key self_sign.key -subj "/C=PL/ST=MA/O=TAG/CN=Tomasz Grobelny" -sha256 -days 1024 -out self_sign.crt DEPENDS sskey)
add_custom_target(sscrt DEPENDS self_sign.crt)

add_custom_command(OUTPUT dhparam.pem COMMAND openssl dhparam -out dhparam.pem 2048 > /dev/null 2>&1)
add_custom_target(dhparam DEPENDS dhparam.pem)

add_dependencies(AAServer sskey sscrt dhparam)
