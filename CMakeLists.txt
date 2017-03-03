cmake_minimum_required(VERSION 3.6)
project(ProtobufStudy)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES proto_cc/query.pb.cc main.cpp)

include_directories(proto_cc)

include_directories(plib/protobuf-2.6.1/include)
include_directories(plib/zlib-1.2.8/include)

link_directories(plib/protobuf-2.6.1/lib)
link_directories(plib/zlib-1.2.8/lib)

add_executable(coder_test ${SOURCE_FILES})

target_link_libraries(coder_test protobuf)
target_link_libraries(coder_test z)
