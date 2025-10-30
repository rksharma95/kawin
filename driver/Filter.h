#pragma once
#include "pch.h"

#ifndef __FILTER_H__
#define __FILTER_H__

#pragma once

EXTERN_C_START
FLT_PREOP_CALLBACK_STATUS FLTAPI PreOperationCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

NTSTATUS FLTAPI InstanceFilterUnloadCallback(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS FLTAPI InstanceSetupCallback(
    _In_ PCFLT_RELATED_OBJECTS  FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS  Flags,
    _In_ DEVICE_TYPE  VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE  VolumeFilesystemType
);

NTSTATUS FLTAPI InstanceQueryTeardownCallback(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

NTSTATUS
ScannerPortConnect(
    _In_ PFLT_PORT ClientPort,
    _In_opt_ PVOID ServerPortCookie,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID* ConnectionCookie
);

VOID
ScannerPortDisconnect(
    _In_opt_ PVOID ConnectionCookie
);

NTSTATUS RegisterFilter(
    _In_ PDRIVER_OBJECT DriverObject
);

EXTERN_C_END

#ifdef ALLOC_PRAGMA
    #pragma alloc_text (PAGE, InstanceFilterUnloadCallback)
    #pragma alloc_text (PAGE, InstanceSetupCallback)
    #pragma alloc_text (PAGE, InstanceQueryTeardownCallback)
#endif

CONST FLT_OPERATION_REGISTRATION g_callbacks[] =
{
    {
        IRP_MJ_CREATE,
        0,
        PreOperationCreate,
        0
    },

    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION g_filterRegistration =
{
    sizeof(FLT_REGISTRATION),      //  Size
    FLT_REGISTRATION_VERSION,      //  Version
    0,                             //  Flags
    NULL,                          //  Context registration
    g_callbacks,                   //  Operation callbacks
    InstanceFilterUnloadCallback,  //  FilterUnload
    InstanceSetupCallback,         //  InstanceSetup
    InstanceQueryTeardownCallback, //  InstanceQueryTeardown
    NULL,                          //  InstanceTeardownStart
    NULL,                          //  InstanceTeardownComplete
    NULL,                          //  GenerateFileName
    NULL,                          //  GenerateDestinationFileName
    NULL                           //  NormalizeNameComponent
};

typedef struct _SCANNER_DATA {

    PFLT_FILTER Filter;
    PFLT_PORT ServerPort;
    PEPROCESS UserProcess;
    PFLT_PORT ClientPort;

} SCANNER_DATA, * PSCANNER_DATA;

extern SCANNER_DATA g_ScannerData;

#define MAX_FILTER_EVENT_SIZE (64 * 1024)

#pragma pack(push, 8)
typedef enum _FS_EVENT_TYPE
{
    EventType_HostLog = 1,
    EventType_MatchHostPolicy = 2,
} FS_EVENT_TYPE;

typedef enum _FS_EVENT_OPERATION {
    EventOperation_Invalid = 0,
    EventOperation_Process = 1,
    EventOperation_File = 2,
    EventOperation_Network = 3
} FS_EVENT_OPERATION;


typedef struct _FILE_EVENT {
    ULONG Operation;
    ULONG ProcessId;
    ULONG ProcessPathOffset;
    ULONG ProcessPathLength;
    ULONG FilePathOffset;
    ULONG FilePathLength;
} FILE_EVENT, * PFILE_EVENT;

typedef struct _PROCESS_EVENT {
    ULONG operation;
    ULONG process_id;
    ULONG parent_process_id;
    ULONG process_path_offset;
    ULONG process_path_length;
    ULONG command_line_offset;
    ULONG command_line_length;
    ULONG parent_process_path_offset;
    ULONG parent_process_path_length;
} PROCESS_EVENT, * PPROCESS_EVENT;

typedef struct _NETWORK_EVENT {
    ULONG operation;
    ULONG protocol;
    USHORT local_port;
    USHORT remote_port;
    UCHAR local_address[16];
    UCHAR remote_address[16];
    ULONG data_length;
    UCHAR address_family;
} NETWORK_EVENT, * PNETWORK_EVENT;

typedef struct _EVENT {
    ULONGLONG timestamp;
    FS_EVENT_TYPE type;
    FS_EVENT_OPERATION operation;
    BOOLEAN blocked;
    union {
        FILE_EVENT File;
        PROCESS_EVENT Process;
        NETWORK_EVENT Network;
    } data;
} EVENT, * PEVENT;

typedef struct _REPLY {
    BOOLEAN ack;
} EVENT_REPLY, * PEVENT_REPLY;

#pragma pack(pop)
#endif // !__FILTER_H__
