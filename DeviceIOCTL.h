#pragma once

//#include <ntddk.h>
#define DEVICE_KARMOR 0x8022
#define MAX_PATH_LENGTH 260

typedef struct _USER_RULE_REQUEST {
    WCHAR Path[MAX_PATH_LENGTH]; // Null-terminated wide string
    SHORT Action;                // 0 = Audit, 1 = Block
} USER_RULE_REQUEST, * PUSER_RULE_REQUEST;

#define IOCTL_ADD_RULE CTL_CODE(DEVICE_KARMOR, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_REMOVE_RULE CTL_CODE(DEVICE_KARMOR, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

