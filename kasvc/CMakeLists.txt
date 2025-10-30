cmake_minimum_required(VERSION 3.15)
project(KubeArmorUserService VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(WIN32)
    add_definitions(
        -D_WIN32_WINNT=0x0A00
        -DWIN32_LEAN_AND_MEAN
        -DNOMINMAX
    )
endif()

# Set paths
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};C:/Users/VC/cmake")

# Find packages
find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

message(STATUS "Found Protobuf ${Protobuf_VERSION}")
message(STATUS "Found gRPC ${gRPC_VERSION}")

# Get protoc and plugin
get_target_property(PROTOBUF_PROTOC protobuf::protoc IMPORTED_LOCATION_RELEASE)
if(NOT PROTOBUF_PROTOC)
    get_target_property(PROTOBUF_PROTOC protobuf::protoc IMPORTED_LOCATION_DEBUG)
endif()
if(NOT PROTOBUF_PROTOC)
    get_target_property(PROTOBUF_PROTOC protobuf::protoc IMPORTED_LOCATION)
endif()

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
if(NOT GRPC_CPP_PLUGIN)
    get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_DEBUG)
endif()
if(NOT GRPC_CPP_PLUGIN)
    get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# ============================================
# Proto Library 1: Feeder
# ============================================
set(FEEDER_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protos)
set(FEEDER_PROTO_FILES ${FEEDER_PROTO_DIR}/kubearmor.proto)

add_library(feeder_proto ${FEEDER_PROTO_FILES})
target_link_libraries(feeder_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
target_include_directories(feeder_proto PUBLIC ${GENERATED_DIR})

protobuf_generate(
    TARGET feeder_proto
    LANGUAGE cpp
    PROTOC_OUT_DIR ${GENERATED_DIR}
    IMPORT_DIRS ${FEEDER_PROTO_DIR}
)

protobuf_generate(
    TARGET feeder_proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
    PROTOC_OUT_DIR ${GENERATED_DIR}
    IMPORT_DIRS ${FEEDER_PROTO_DIR}
)

# ============================================
# Main Executable
# ============================================
set(SOURCES
    src/main.cpp
    
    # Data
    src/data/event_processor.cpp
    src/data/event_types.cpp
    
    # Application
    src/app/monitoring_service.cpp
    
    # communication
    src/comm/message_parser.cpp
    src/comm/iocp_filter_port_communicator.cpp
    src/comm/json_config_store.cpp
    
    # gRPC
    src/rpc/feeder_event_publisher.cpp
    src/rpc/feeder_service.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GENERATED_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        feeder_proto              # Feeder proto from submodule
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
        Threads::Threads
        fltlib.lib
        ws2_32.lib
)

# MSVC settings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4 /permissive- /Zc:__cplusplus /EHsc /MP /utf-8
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS
    )
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
    set_property(TARGET feeder_proto PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

# Install
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Feeder Proto: ${FEEDER_PROTO_DIR}")
message(STATUS "")