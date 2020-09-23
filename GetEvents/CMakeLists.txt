cmake_minimum_required(VERSION 2.8)
project (GetEvents)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(X11 REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${X11_INCLUDE_DIR})
include_directories(${X11_XTest_INCLUDE_PATH})
include_directories(include)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS 
../proto/InputEvent.proto
../proto/TouchEvent.proto
../proto/TouchLocation.proto
../proto/TouchAction.proto
)

add_executable(GetEvents main.cpp 
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(GetEvents protobuf::libprotobuf)
target_link_libraries(GetEvents X11::X11)
target_link_libraries(GetEvents X11::Xtst)
