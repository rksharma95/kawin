#include "Rule.h"

PRULE_ENTRY AllocateRuleEntry(PUNICODE_STRING Path, RuleAction Action) {
    PRULE_ENTRY entry = (PRULE_ENTRY)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(RULE_ENTRY), RULE_ENTRY_TAG);
    if (!entry) 
    {
        KdPrint(("failed to allocate rule entry..."));
        return NULL;
    }
    RtlZeroMemory(entry, sizeof(RULE_ENTRY));

    USHORT size = Path->Length;
    entry->Path.Buffer = (PWCH)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, RULE_PATH_TAG);
    if (!entry->Path.Buffer) {
        KdPrint(("failed to allocate rule path buffer..."));
        ExFreePoolWithTag(entry, RULE_ENTRY_TAG);
        return NULL;
    }
    entry->Path.Length = 0;
    entry->Path.MaximumLength = Path->MaximumLength;
    entry->Action = Action;
    RtlCopyUnicodeString(&entry->Path, Path);

    return entry;
}

VOID FreeRuleEntry(PRULE_ENTRY Entry) {
    if (Entry) {
        if (Entry->Path.Buffer)
            ExFreePoolWithTag(Entry->Path.Buffer, RULE_PATH_TAG);
        ExFreePoolWithTag(Entry, RULE_ENTRY_TAG);
    }
}

ULONG HashPath(PUNICODE_STRING Path) {
    ULONG hash = 0;
    RtlHashUnicodeString(Path, TRUE, HASH_STRING_ALGORITHM_X65599, &hash);
    return hash;
}

BOOLEAN MatchPath(PRULE_ENTRY Entry, PUNICODE_STRING Path) {
    if (&Entry->Path) {
        KdPrint(("checking if path %wZ", &Entry->Path));
    }
    if (Path) {
        KdPrint(("matches %wZ", Path));
    }
    return RtlEqualUnicodeString(&Entry->Path, Path, TRUE);
}
