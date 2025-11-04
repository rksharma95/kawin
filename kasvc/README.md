```text
KubeArmorUserService/
|-- CMakeLists.txt
|-- config.json
|-- README.md
|
|---include
|   |---app
|   |   |---monitoring_service.h
|   |   |
|   |   |---interfaces
|   |       |---i_configuration_store.h
|   |       |---i_event_publisher.h
|   |       |---i_event_receiver.h
|   |
|   |---comm
|   |   |---iocp_filter_port_communicator.h
|   |   |---json_config_store.h
|   |   |---kernel_message.h
|   |   |---message_parser.h
|   |
|   |---common
|   |   |---constants.h
|   |   |---logger.h
|   |   |---result.h
|   |   |---thread_safe_queue.h
|   |   |---types.h
|   |
|   |---data
|   |   |---event_processor.h
|   |   |---event_types.h
|   |
|   |---nlohmann
|   |   |---json.hpp
|   |
|   |---rpc
|       |---feeder_event_publisher.h
|       |---feeder_service.h
|
|---protos
|   |---kubearmor.proto
|
|---src
    |---main.cpp
    |
    |---app
    |   |---monitoring_service.cpp
    |
    |---comm
    |   |---iocp_filter_port_communicator.cpp
    |   |---json_config_store.cpp
    |   |---message_parser.cpp
    |
    |---data
    |   |---event_processor.cpp
    |   |---event_types.cpp
    |
    |---rpc
        |---feeder_event_publisher.cpp
        |---feeder_service.cpp
```

## Architecture Overview

![High-level-architecture](../docs/assets/KubeArmorUserService.svg)

## Build

### Prerequisites

- cmake (min: VERSION 3.15)
- gRPC

### Build and Execute
- create a build directory
    ```
    mkdir build
    cd build
    ```
- generate build system files 
    ```
    cmake .. -G "Visual Studio 17 2022" -A x64 # change according to your build environment
    ```
- build executable
    ```
    cmake --build . --config Release
    ```

- successful build will create `.exe` file in `./Release` folder.

- run the service
    ```
    KubeArmorUserService.exe <path-to-config.json> <- optional if config.json is in same directory 
    ```