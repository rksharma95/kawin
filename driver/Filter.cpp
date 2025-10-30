#include "Filter.h"
#include "FilenameInformationGuard.h"

constexpr auto EVENT_TAG = 'evnt';

static const HANDLE g_systemProcessId = reinterpret_cast<HANDLE>(4);

FLT_PREOP_CALLBACK_STATUS FLTAPI PreOperationCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(CompletionContext);

    FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PEVENT event = NULL;
    ULONG eventLength = sizeof(EVENT);
    PEVENT_REPLY eventReply = NULL;
    ULONG replyLength;
    LARGE_INTEGER timeOut = { 0 };
    timeOut.QuadPart = -10 * 1000 * 1000; // 1 sec
    PEPROCESS process = NULL;
    PUNICODE_STRING imagePath = NULL;

    //
    // Pre-create callback to get file info during creation or opening
    //

    if (!Data || !FltObjects)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    //
    // Skip if this PreCreate call was performed from the System process.
    //
    if (PsGetCurrentProcessId() == g_systemProcessId)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    //
    // Check if the FileObject being processed represents: Named pipe, Mailslot, or Volume. Skip this call if it returns true.
    // 
    if (FltObjects->FileObject->Flags & (FO_NAMED_PIPE | FO_MAILSLOT | FO_VOLUME_OPEN))
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    __try {

        replyLength = sizeof(EVENT_REPLY);

        if (g_ScannerData.ClientPort != NULL && g_ScannerData.Filter != NULL) {

            process = FltGetRequestorProcess(Data);

            if (process == NULL) {
                DbgPrint("!!! unable to get requestor process");
            }
            else {
                const NTSTATUS status = SeLocateProcessImageName(process, &imagePath);
                if (status == STATUS_SUCCESS) {
                    if (imagePath != NULL) {
                        DbgPrint("requestor process image path: %wZ of length %d\n", imagePath, imagePath->Length);
                        eventLength += imagePath->Length;
                    }
                }
                else {
                    DbgPrint("SeLocateProcessImageName failed 0x%x\n", status);
                }
            }

            if (FltObjects->FileObject->FileName.Length > 0) {
                eventLength += FltObjects->FileObject->FileName.Length;
            }

            DbgPrint("event size %llu with process path %lu\n", sizeof(EVENT), eventLength);

            if (eventLength > MAX_FILTER_EVENT_SIZE) {
                returnStatus = FLT_PREOP_COMPLETE;
                __leave;
            }

            event = (PEVENT)ExAllocatePoolZero(NonPagedPool,
                eventLength,
                'nacS');

            eventReply = (PEVENT_REPLY)ExAllocatePoolZero(NonPagedPool,
                sizeof(EVENT_REPLY),
                'nacS');

            if (event == NULL || eventReply == NULL) {
                Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                Data->IoStatus.Information = 0;
                returnStatus = FLT_PREOP_COMPLETE;
                __leave;
            }
            ULONG currentOffset = sizeof(EVENT);
            PUCHAR basePtr = (PUCHAR)event;

            event->type = EventType_HostLog;
            event->operation = EventOperation_File;
            KeQuerySystemTime((LARGE_INTEGER*)&event->timestamp);
            event->blocked = false;
            event->data.File.ProcessId = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
            event->data.File.Operation = 0;

            if (imagePath != NULL) {
                RtlCopyMemory(basePtr + currentOffset, imagePath->Buffer, imagePath->Length);
                event->data.File.ProcessPathOffset = currentOffset;
                event->data.File.ProcessPathLength = imagePath->Length;
                currentOffset += imagePath->Length;
            }
            else {
                event->data.File.ProcessPathOffset = 0;
                event->data.File.ProcessPathLength = 0;
                DbgPrint("!!! imagePath is NULL");
            }

            if (FltObjects->FileObject->FileName.Length > 0) {
                RtlCopyMemory(basePtr + currentOffset, FltObjects->FileObject->FileName.Buffer, FltObjects->FileObject->FileName.Length);
                event->data.File.FilePathLength = FltObjects->FileObject->FileName.Length;
                event->data.File.FilePathOffset = currentOffset;
                currentOffset += FltObjects->FileObject->FileName.Length;
            }
            else {
                event->data.File.FilePathLength = 0;
                event->data.File.FilePathOffset = 0;
            }

            NTSTATUS status = FltSendMessage(g_ScannerData.Filter,
                &g_ScannerData.ClientPort,
                event,
                eventLength,
                (PVOID)eventReply,
                &replyLength,
                &timeOut);

            if (status == STATUS_SUCCESS) {
                if (eventReply->ack) {
                    DbgPrint("!!! successfully sent event to user-mode and ack");
                }
                else {
                    DbgPrint("!!! successfully sent event to user-mode but not ack");
                }
            }
            else {
                DbgPrint("!!! couldn't send event to user-mode, status 0x%X\n", status);

            }

        }


    }
    __finally {

        if (event != NULL) {
            ExFreePoolWithTag(event, 'nacS');
        }

        if (eventReply != NULL) {
            ExFreePoolWithTag(eventReply, 'nacS');
        }

        if (imagePath != NULL) {
            ExFreePool(imagePath);
        }
    }

    return returnStatus;
}

NTSTATUS
ScannerPortConnect(
    _In_ PFLT_PORT ClientPort,
    _In_opt_ PVOID ServerPortCookie,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID* ConnectionCookie
)
/*++

Routine Description

    This is called when user-mode connects to the server port - to establish a
    connection

Arguments

    ClientPort - This is the client connection port that will be used to
        send messages from the filter

    ServerPortCookie - The context associated with this port when the
        minifilter created this port.

    ConnectionContext - Context from entity connecting to this port (most likely
        your user mode service)

    SizeofContext - Size of ConnectionContext in bytes

    ConnectionCookie - Context to be passed to the port disconnect routine.

Return Value

    STATUS_SUCCESS - to accept the connection

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionCookie = NULL);

    FLT_ASSERT(g_ScannerData.ClientPort == NULL);
    FLT_ASSERT(g_ScannerData.UserProcess == NULL);

    //
    //  Set the user process and port. In a production filter it may
    //  be necessary to synchronize access to such fields with port
    //  lifetime. For instance, while filter manager will synchronize
    //  FltCloseClientPort with FltSendMessage's reading of the port
    //  handle, synchronizing access to the UserProcess would be up to
    //  the filter.
    //

    g_ScannerData.UserProcess = PsGetCurrentProcess();
    g_ScannerData.ClientPort = ClientPort;

    DbgPrint("!!! scanner.sys --- connected, port=0x%p\n", ClientPort);

    return STATUS_SUCCESS;
}

VOID
ScannerPortDisconnect(
    _In_opt_ PVOID ConnectionCookie
)
/*++

Routine Description

    This is called when the connection is torn-down. We use it to close our
    handle to the connection

Arguments

    ConnectionCookie - Context from the port connect routine

Return value

    None

--*/
{
    UNREFERENCED_PARAMETER(ConnectionCookie);

    PAGED_CODE();

    DbgPrint("!!! scanner.sys --- disconnected, port=0x%p\n", g_ScannerData.ClientPort);

    //
    //  Close our handle to the connection: note, since we limited max connections to 1,
    //  another connect will not be allowed until we return from the disconnect routine.
    //

    FltCloseClientPort(g_ScannerData.Filter, &g_ScannerData.ClientPort);

    //
    //  Reset the user-process field.
    //

    g_ScannerData.UserProcess = NULL;
}

NTSTATUS FLTAPI InstanceFilterUnloadCallback(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(Flags);

    //
    // This is called before a filter is unloaded.
    // If NULL is specified for this routine, then the filter can never be unloaded.
    //
    if (g_ScannerData.ClientPort) {
        FltCloseClientPort(g_ScannerData.Filter, &g_ScannerData.ClientPort);
    }

    if (g_ScannerData.ServerPort) {
        FltCloseCommunicationPort(g_ScannerData.ServerPort);
    }

    if (g_ScannerData.Filter)
    {
        FltUnregisterFilter(g_ScannerData.Filter);
    }

    return STATUS_SUCCESS;
}

NTSTATUS FLTAPI InstanceSetupCallback(
    _In_ PCFLT_RELATED_OBJECTS  FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS  Flags,
    _In_ DEVICE_TYPE  VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE  VolumeFilesystemType)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    //
    // This is called to see if a filter would like to attach an instance to the given volume.
    //

    return STATUS_SUCCESS;
}

NTSTATUS FLTAPI InstanceQueryTeardownCallback(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    //
    // This is called to see if the filter wants to detach from the given volume.
    //

    return STATUS_SUCCESS;
}

NTSTATUS RegisterFilter(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    PSECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uniString;
    NTSTATUS status;

    RtlInitUnicodeString(&uniString, L"\\ScannerPort");

    //
    // register minifilter driver
    //
    status = FltRegisterFilter(DriverObject, &g_filterRegistration, &g_ScannerData.Filter);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    KdPrint(("KdPrint:fsminifilter driver loaded"));


    status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
    if (NT_SUCCESS(status)) {

        InitializeObjectAttributes(&oa,
            &uniString,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,
            sd);

        status = FltCreateCommunicationPort(
            g_ScannerData.Filter,
            &g_ScannerData.ServerPort,
            &oa,
            NULL,
            ScannerPortConnect,
            ScannerPortDisconnect,
            NULL,
            1);

        FltFreeSecurityDescriptor(sd);

        if (NT_SUCCESS(status)) {
            //
            // start minifilter driver
            //
            status = FltStartFiltering(g_ScannerData.Filter);
            if (NT_SUCCESS(status))
            {
                return STATUS_SUCCESS;
            }

            FltCloseCommunicationPort(g_ScannerData.ServerPort);
        }
    }
    FltUnregisterFilter(g_ScannerData.Filter);

    return status;

}