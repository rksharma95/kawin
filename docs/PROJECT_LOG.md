# kawin

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Core Components](#2-core-components)
3. [User Mode Service](#3-user-mode-service)
    - 3.1 [configuration management]()
    - 3.2 [policy management]()
    - 3.3 [ICOTL communicator]()
    - 3.4 [IOCP filter port communicator]()
    - 3.5 [gRPC]()
4. [Kernel Driver]()
    - 4.1 [common functionalities](#41-common-functionalities)
    - 4.2 [IOCTL dispatch routines](#42-ioctl-dispatch-routines)
    - 4.3 [minifilter for file monitoring](#43-minifilter-for-file-monitoring)
    - 4.4 [system callbacks for process, registry (optional) monitoring](#44-system-callbacks-for-process-registry-optional-monitoring)
    - 4.5 [policy engine](#45-policy-engine)
5. [Windows Installer](#5-windows-installer)
6. [Signing and packaging](#6-signing-and-packaging)
7. [CI/CD Pipeline](#7-cicd-pipeline)
8. [Testing](#8-testing)

## 1. Architecture Overview

![High-level-architecture](./assets/kawin-arch.svg)

### 2. Core Components
| Component | Language | Description |
|------------|-----------|-------------|
| **User Mode Service** | C++ (Win32) | Windows Service; mediates between kernel and backend |
| **Kernel Driver** | C (Kernel Mode) | Minifilter driver; captures system events |
| **Communication Layer** | C/C++ | IOCTL interface between driver and service |
| **Backend API** | gRPC | Receives telemetry, sends policy updates |
| **Installer** | WiX | Winodows installer (MSI), installs driver and service with proper signing |

## 3. [User Mode Service](../kasvc)

**Purpose**: 
- load and initialize configurations
- initialize driver communication channels
    - IOCTL for configuration and policy management
    - IOCP using filter port for event streaming
- policy management
- manage service lifecycle (start, stop, reconcile)
- implement backend API using gRPC to publish event and for configuration and policy management.

Read more [here](../kasvc/README.md) for detailed architecture and current progress.

### 3.1 configuration management

- [x] accept configuration using a (json) file
- [ ] configuration validation
- [x] load configuration and watch for changes
- [ ] reconcile service on configuration update

### 3.2 Polict management

- [ ] accept policy using file (using CLI)

    main user mode service will be running as windows service in background, considering that it would be better to implement another cli that accepts the kubearmor security policies (KSP, KHP, CSP) in yaml and apply the policy by connecting with the service as a client on gRPC endpoint.
- [ ] support policy management using gRPC. (IOCTL comm. in next section)

- [ ] support CRUD operations on policy

### 3.3 IOCTL Communicator

- [ ] connect with kernel driver using IOCTL to support configuration and policy updation
- [ ] it will be a subsystem of [policy gRPC service](#352-driveragent-service) to communcate with kernel driver for policy management.

```c++
#define IOCTL_ADD_RULE CTL_CODE(DEVICE_KARMOR, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_REMOVE_RULE CTL_CODE(DEVICE_KARMOR, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)
```
### 3.4 ICOP Filter Port Communicator

[I/O Completion port](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports) looks more promising method to handle the stream of events that kernel driver emits than implementing the dedicated workers. 

- [x] connect with kernel driver by creating IO Completion port using filter port. 


### 3.5 gRPC

#### 3.5.1 feeder service

- [x] publish (WatchLogs, WatchAlerts) the events received from kernel
- [x] queue the received events and publish the events to the active subscribers

#### 3.5.2 driver/agent service

- [ ] publish agent state implement [KubeArmor StateAgent service](https://github.com/kubearmor/KubeArmor/blob/main/protobuf/state.proto)
- [ ] support CRUD operation on policy
- [ ] support configuration updates

## 4. Kernel Driver

**Purpose**: 
- register file system callback (minifilter), to monitor file events
- hook process handle events (system callbacks) to monitor process events
- manage policy engine (policy CRUD operations and matching)
- manage kernel driver lifecycle (load, unload, reconcile)
- forward event data to user mode service
    - it is critical to avoid delays in processing critical paths (os core services), avoid sync mode if possible. 

current status: https://github.com/rksharma95/kawin/tree/main/driver

### 4.1 common functionalities

- [ ] configuration management
- [ ] self protection (disable driver unload)
- [ ] emit ETW events if enabled

current status: 

### 4.2 [IOCTL dispatch routines](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/writing-dispatch-routines)

- [ ] define rule structure
    ```c++
    typedef struct _RULE_ENTRY {
    LIST_ENTRY ListEntry;
    UNICODE_STRING Path;
    RuleAction Action;
    } RULE_ENTRY, * PRULE_ENTRY;
    ```
- [ ] write dispatch routines to support configuration, and policy management using IOCTL
    ```c++
    #define IOCTL_ADD_RULE CTL_CODE(DEVICE_KARMOR, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
    #define IOCTL_REMOVE_RULE CTL_CODE(DEVICE_KARMOR, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)
    ```
current status: https://github.com/rksharma95/kawin/blob/39218bdbd563a764174ac7bcf2a0a166f232728e/driver/Rule.cpp

### 4.3 minifilter for file monitoring

- [ ] register a minifilter
- [ ] capture target events
- [ ] match events against applied policies
- [ ] send event to user service using filterport
        
    - we're handling the event burst in user end service to no need to handle events asyncronously.  

current status: https://github.com/rksharma95/kawin/blob/39218bdbd563a764174ac7bcf2a0a166f232728e/driver/Filter.cpp

### 4.4 [system callbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/callback-objects) for process, registry (optional) monitoring

- [ ] register callbacks to access process handle
- [ ] match event against applied (process) policies
- [ ] send event to userend service using filter port

current status: https://github.com/rksharma95/kawin/blob/39218bdbd563a764174ac7bcf2a0a166f232728e/driver/Karmor.cpp#L304-L359

### 4.5 policy engine

- [ ] define policy rule strcuture
- [ ] implement underlying structure to store the rules
- [ ] support CRUD operation on policy
- [ ] policy matcher

current status: https://github.com/rksharma95/kawin/blob/39218bdbd563a764174ac7bcf2a0a166f232728e/driver/Globals.cpp

## 5. Windows Installer

- [ ] create installer using [WiX](https://github.com/wixtoolset/wix)

## 6. Signing and packaging

- [ ] ev signing the driver
- [ ] create and publish the package using windows partner portal

## 7. CI/CD Pipeline

- implement CI/CD pipeline.

## 8. Testing

- unit and integration tests
- test kernel driver using driver verifier, scale test kernel driver