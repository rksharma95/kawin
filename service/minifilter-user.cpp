// minifilter-user.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <string.h>
#include <crtdbg.h>
#include <assert.h>
#include <fltuser.h>
#include <dontuse.h>
#include <string>
#include <cwchar>
#include <iostream>

#define PORT_NAME L"\\ScannerPort"

#define MAX_FILTER_EVENT_SIZE (64 * 1024)
#define USER_MESSAGE_BUFFER_SIZE (MAX_FILTER_EVENT_SIZE + sizeof(FILTER_MESSAGE_HEADER))

// DeviceIOCTL
#define DEVICE_SYMLINK         L"\\??\\Karmor"
#define DEVICE_KARMOR 0x8022
#define MAX_PATH_LENGTH 260

typedef struct _USER_RULE_REQUEST {
    WCHAR Path[MAX_PATH_LENGTH]; // Null-terminated wide string
    SHORT Action;                // 0 = Audit, 1 = Block
} USER_RULE_REQUEST, * PUSER_RULE_REQUEST;

#define IOCTL_ADD_RULE CTL_CODE(DEVICE_KARMOR, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_REMOVE_RULE CTL_CODE(DEVICE_KARMOR, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

bool SendRuleIoctl(DWORD ioctlCode, const USER_RULE_REQUEST& request) {
    HANDLE hDevice = CreateFileW(
        DEVICE_SYMLINK,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open device: Error " << GetLastError() << std::endl;
        return false;
    }

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        hDevice,
        ioctlCode,
        (LPVOID)&request,
        sizeof(request),
        nullptr,
        0,
        &bytesReturned,
        nullptr
    );

    CloseHandle(hDevice);

    if (!success) {
        std::wcerr << L"DeviceIoControl failed: Error " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

// -DeviceIOCTL

typedef enum _FS_EVENT_TYPE
{
    EventType_Invalid = 0,
    EventType_Process = 1,
    EventType_File = 2,
} FS_EVENT_TYPE;

typedef struct _FILE_EVENT {
    ULONG ProcessId;
    ULONG ProcessPathOffset;
    ULONG ProcessPathLength;
    ULONG FilePathOffset;
    ULONG FilePathLength;
} FILE_EVENT, * PFILE_EVENT;

typedef struct _EVENT {
    FS_EVENT_TYPE type;
    union {
        FILE_EVENT File;
    } data;
}EVENT, * PEVENT;

typedef struct _REPLY {
    BOOLEAN ack;
} EVENT_REPLY, * PEVENT_REPLY;

typedef struct _EVENT_MESSAGE {
    FILTER_MESSAGE_HEADER MessageHeader;

    //EVENT Event;
    BYTE Event[MAX_FILTER_EVENT_SIZE];

    OVERLAPPED Ovlp;
}EVENT_MESSAGE, * PEVENT_MESSAGE;

typedef struct _EVENT_REPLY_MESSAGE {
    FILTER_REPLY_HEADER ReplyHeader;

    EVENT_REPLY Reply;
}EVENT_REPLY_MESSAGE, * PEVENT_REPLY_MESSAGE;

/*++

Module Name:

    minifilter-User.c

Abstract:

    This file contains the implementation for the main function of the
    user application piece of scanner.  This function is responsible for
    actually scanning file contents.

Environment:

    User mode

--*/

//
//  Default and Maximum number of threads.
//

#define SCANNER_DEFAULT_REQUEST_COUNT       5
#define SCANNER_DEFAULT_THREAD_COUNT        2
#define SCANNER_MAX_THREAD_COUNT            64

//
//  Global shutdown flag
//

volatile BOOL g_ShutdownRequested = FALSE;

//
//  Context passed to worker threads
//

typedef struct _SCANNER_THREAD_CONTEXT {

    HANDLE Port;
    HANDLE Completion;

} SCANNER_THREAD_CONTEXT, * PSCANNER_THREAD_CONTEXT;


VOID
Usage(
    VOID
)
/*++

Routine Description

    Prints usage

Arguments

    None

Return Value

    None

--*/
{
    std::cout << "Usage:\n"
        << "  minifilter-user rule add <path> <action>\n"
        << "  minifilter-user rule delete <path>\n"
        << "  minifilter-user log [requests per thread] [number of threads(1-64)]\n";
    printf("Connects to the scanner filter and scans buffers \n");
}


BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
/*++

Routine Description

    Console control handler for graceful shutdown

Arguments

    dwCtrlType - Type of control signal

Return Value

    TRUE if handled, FALSE otherwise

--*/
{
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        printf("\n[*] Shutdown signal received. Stopping event consumption...\n");
        g_ShutdownRequested = TRUE;
        return TRUE;
    default:
        return FALSE;
    }
}


DWORD
ScannerWorker(
    _In_ PSCANNER_THREAD_CONTEXT Context
)
/*++

Routine Description

    This is a worker thread that continuously processes events from the filter

Arguments

    Context  - This thread context has a pointer to the port handle we use to send/receive messages,
                and a completion port handle that was already associated with the comm. port by the caller

Return Value

    HRESULT indicating the status of thread exit.

--*/
{
    PEVENT event;
    EVENT_REPLY_MESSAGE replyMessage;
    PEVENT_MESSAGE message;
    LPOVERLAPPED pOvlp;
    BOOL result;
    DWORD outSize;
    HRESULT hr;
    ULONG_PTR key;

#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant

    while (!g_ShutdownRequested) {

#pragma warning(pop)

        result = GetQueuedCompletionStatus(Context->Completion, &outSize, &key, &pOvlp, INFINITE);

        message = CONTAINING_RECORD(pOvlp, EVENT_MESSAGE, Ovlp);

        if (!result) {
            hr = HRESULT_FROM_WIN32(GetLastError());

            // Check if this is due to shutdown
            if (g_ShutdownRequested || hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {
                printf("Scanner: Port disconnected or shutdown requested.\n");
                break;
            }

            printf("Scanner: GetQueuedCompletionStatus failed. Error = 0x%X\n", hr);
            break;
        }

        //printf("Received message, size %Id\n", pOvlp->InternalHigh);

        event = (PEVENT)message->Event;

        if (event->type == EventType_File) {
            printf("=======================================\n");
            printf("Operation: File\n");
            printf("ProcessID: %lu\n", event->data.File.ProcessId);
            //printf("Requestor process path offset: %d\n", event->data.File.ProcessPathOffset);
            //printf("Requestor process path length: %d\n", event->data.File.ProcessPathLength);
            if (event->data.File.ProcessPathLength > 0) {
                WCHAR* processPath = (WCHAR*)(message->Event + event->data.File.ProcessPathOffset);
                int pathChar = event->data.File.ProcessPathLength / sizeof(WCHAR);

                std::wstring p_pathStr(processPath, pathChar);
                std::wcout << L"Process: " << p_pathStr << std::endl;
            }
            //printf("Requestor file path offset: %d\n", event->data.File.FilePathOffset);
            //printf("Requestor file path length: %d\n", event->data.File.FilePathLength);
            if (event->data.File.FilePathLength > 0) {
                WCHAR* filePath = (WCHAR*)(message->Event + event->data.File.FilePathOffset);
                int pathChar = event->data.File.FilePathLength / sizeof(WCHAR);

                std::wstring f_pathStr(filePath, pathChar);
                std::wcout << L"File: " << f_pathStr << std::endl;
            }

        }
        else {
            printf("Unknown event type %d\n", event->type);
        }

        replyMessage.ReplyHeader.Status = 0;
        replyMessage.ReplyHeader.MessageId = message->MessageHeader.MessageId;

        replyMessage.Reply.ack = true;

        //printf("Replying message, Ack: %d\n", replyMessage.Reply.ack);

        //printf("Replying message size: %llu\n", sizeof(EVENT_REPLY_MESSAGE) - sizeof(FILTER_REPLY_HEADER));

        hr = FilterReplyMessage(Context->Port,
            (PFILTER_REPLY_HEADER)&replyMessage,
            sizeof(replyMessage));

        if (SUCCEEDED(hr)) {

            printf("Replied message\n");

        }
        else {

            //printf("Scanner: Error replying message. Error = 0x%X\n", hr);

            // If reply failed due to invalid handle, break the loop
            if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {
                break;
            }
        }

        // Reissue the request to get the next message (indefinite loop)
        memset(&message->Ovlp, 0, sizeof(OVERLAPPED));

        hr = FilterGetMessage(Context->Port,
            &message->MessageHeader,
            USER_MESSAGE_BUFFER_SIZE,
            &message->Ovlp);

        if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {

            // If not pending, check if it's a fatal error
            if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {
                printf("Scanner: Port is disconnected, probably due to scanner filter unloading.\n");
            }
            else {
                printf("Scanner: FilterGetMessage failed. Error = 0x%X\n", hr);
            }
            break;
        }
    }

    if (!SUCCEEDED(hr)) {

        if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {

            //
            //  Scanner port disconncted.
            //

            printf("Scanner: Port is disconnected, probably due to scanner filter unloading.\n");

        }
        else {

            printf("Scanner: Unknown error occured. Error = 0x%X\n", hr);
        }
    }

    printf("Scanner: Worker thread exiting.\n");
    return hr;
}


int _cdecl
wmain(
    _In_ int argc,
    _In_reads_(argc) wchar_t* argv[]
)
{
    DWORD requestCount = SCANNER_DEFAULT_REQUEST_COUNT;
    DWORD threadCount = SCANNER_DEFAULT_THREAD_COUNT;
    HANDLE threads[SCANNER_MAX_THREAD_COUNT] = { NULL };
    SCANNER_THREAD_CONTEXT context;
    HANDLE port, completion;
    PEVENT_MESSAGE messages;
    DWORD threadId;
    HRESULT hr;

    //
    //  Check how many threads and per thread requests are desired.
    //

    if (argc < 2) {
        Usage();
        return 1;
    }

    std::wstring command = argv[1];

    if (command == L"rule") {
        if (argc < 4) {
            Usage();
            return 1;
        }

        std::wstring subcommand = argv[2];

        if (subcommand == L"add") {
            if (argc != 5) {
                std::cerr << "[ERROR] 'rule add' requires <path> and <action>\n";
                Usage();
                return 1;
            }

            USER_RULE_REQUEST req = {};
            wcsncpy_s(req.Path, argv[3], MAX_PATH_LENGTH - 1);

            std::wstring action = argv[4];
            if (action == L"audit") {
                req.Action = 0;
            }
            else if (action == L"block") {
                req.Action = 1;
            }
            else {
                std::wcerr << L"[ERROR] Invalid action. Use 'audit' or 'block'\n";
                return 1;
            }

            std::wcout << L"Sending 'add rule' to driver...\n";
            if (SendRuleIoctl(IOCTL_ADD_RULE, req)) {
                std::wcout << L"Rule added successfully.\n";
            }
            else {
                std::wcerr << L"Failed to add rule.\n";
                return 1;
            }

        }
        else if (subcommand == L"delete") {
            if (argc != 4) {
                std::cerr << "[ERROR] 'rule delete' requires <path>\n";
                Usage();
                return 1;
            }

            USER_RULE_REQUEST req = {};
            wcsncpy_s(req.Path, argv[3], MAX_PATH_LENGTH - 1);
            req.Action = -1; // Driver ignores this for delete

            std::wcout << L"Sending 'delete rule' to driver...\n";
            if (SendRuleIoctl(IOCTL_REMOVE_RULE, req)) {
                std::wcout << L"Rule deleted successfully.\n";
            }
            else {
                std::wcerr << L"Failed to delete rule.\n";
                return 1;
            }

        }
        else {
            std::wcerr << "[ERROR] Unknown subcommand for rule: " << subcommand << "\n";
            Usage();
            return 1;
        }
        return 0;

    }
    else if (command == L"log") {
        if (argc != 4) {
            std::wcerr << "[ERROR] 'log' requires <requestCount> and <threadCount>\n";
            Usage();
            return 1;
        }

        int requestCount = std::stoi(argv[2]);
        int threadCount = std::stoi(argv[3]);

        if (requestCount <= 0) {
            std::cerr << "[ERROR] requestCount must be a positive integer.\n";
            return 1;
        }

        if (threadCount <= 0 || threadCount > 64) {
            std::cerr << "[ERROR] threadCount must be between 1 and 64.\n";
            return 1;
        }

        std::cout << "[log] requestCount = " << requestCount
            << ", threadCount = " << threadCount << "\n";

        // Your logging logic here

    }
    else {
        std::wcerr << "[ERROR] Unknown command: " << command << "\n";
        Usage();
        return 1;
    }

    /*if (argc > 1) {

        requestCount = atoi(argv[1]);

        if (requestCount <= 0) {

            Usage();
            return 1;
        }

        if (argc > 2) {

            threadCount = atoi(argv[2]);
        }

        if (threadCount <= 0 || threadCount > 64) {

            Usage();
            return 1;
        }
    }*/

    //
    //  Set up console control handler for graceful shutdown
    //

    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        printf("WARNING: Unable to set console control handler: %d\n", GetLastError());
    }

    //
    //  Open a commuication channel to the filter
    //

    printf("Scanner: Connecting to the filter ...\n");

    hr = FilterConnectCommunicationPort(PORT_NAME,
        0,
        NULL,
        0,
        NULL,
        &port);

    if (IS_ERROR(hr)) {

        printf("ERROR: Connecting to filter port: 0x%08x\n", hr);
        return 2;
    }

    //
    //  Create a completion port to associate with this handle.
    //

    completion = CreateIoCompletionPort(port,
        NULL,
        0,
        threadCount);

    if (completion == NULL) {

        printf("ERROR: Creating completion port: %d\n", GetLastError());
        CloseHandle(port);
        return 3;
    }

    printf("Scanner: Port = 0x%p Completion = 0x%p\n", port, completion);
    printf("Scanner: Thread Count = %d, Request Count per Thread = %d\n", threadCount, requestCount);
    printf("Scanner: Total message buffers allocated = %d\n", threadCount * requestCount);
    printf("Scanner: Press Ctrl+C to stop...\n\n");

    context.Port = port;
    context.Completion = completion;

    //
    //  Allocate messages based on thread count and request count.
    //  This determines the number of concurrent outstanding requests.
    //

    messages = (PEVENT_MESSAGE)calloc(((size_t)threadCount) * requestCount, sizeof(EVENT_MESSAGE));

    if (messages == NULL) {

        hr = ERROR_NOT_ENOUGH_MEMORY;
        goto main_cleanup;
    }

    //
    //  Create specified number of threads.
    //

    for (DWORD i = 0; i < threadCount; i++) {

        threads[i] = CreateThread(NULL,
            0,
            (LPTHREAD_START_ROUTINE)ScannerWorker,
            &context,
            0,
            &threadId);

        if (threads[i] == NULL) {

            //
            //  Couldn't create thread.
            //

            hr = GetLastError();
            printf("ERROR: Couldn't create thread: %d\n", hr);
            goto main_cleanup;
        }

        //
        //  Issue initial requests for each thread based on requestCount.
        //  These buffers will be continuously reused for indefinite event consumption.
        //

        for (DWORD j = 0; j < requestCount; j++) {

            PEVENT_MESSAGE msg = &(messages[i * requestCount + j]);

            memset(&msg->Ovlp, 0, sizeof(OVERLAPPED));

            //
            //  Request messages from the filter driver.
            //

            hr = FilterGetMessage(port,
                &msg->MessageHeader,
                FIELD_OFFSET(EVENT_MESSAGE, Ovlp),
                &msg->Ovlp);

            if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
                printf("ERROR: Initial FilterGetMessage failed: 0x%08x\n", hr);
                goto main_cleanup;
            }
        }
    }

    printf("Scanner: All threads started and consuming events indefinitely...\n\n");

    hr = S_OK;

main_cleanup:

    //
    //  Wait for all threads to complete
    //

    printf("\nScanner: Waiting for threads to exit...\n");

    for (DWORD i = 0; i < threadCount && threads[i] != NULL; i++) {
        WaitForSingleObjectEx(threads[i], INFINITE, FALSE);
        CloseHandle(threads[i]);
    }

    printf("Scanner: All threads exited. Result = 0x%08x\n", hr);

    CloseHandle(port);
    CloseHandle(completion);

    free(messages);

    return hr;
}