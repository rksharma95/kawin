#include "Globals.h"

NTSTATUS Globals::Init() {
    m_Table = (PRULE_HASH_TABLE)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(RULE_HASH_TABLE), RULE_TABLE_TAG);
    if (!m_Table)
        return STATUS_INSUFFICIENT_RESOURCES;
    if (m_Table) {
        for (int i = 0; i < NUM_BUCKETS; ++i) {
            InitializeListHead(&m_Table->Buckets[i]);
        }
    }
    m_Lock.Init();
    m_DefaultProcessPosture = RuleAction::Audit;
    m_ProcessWhitelist = 0;
    return STATUS_SUCCESS;
}

VOID Globals::DestroyRuleHashTable() {
    if (!m_Table) 
        return;
    KdPrint(("destroying rule table..."));
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        PLIST_ENTRY head = &m_Table->Buckets[i];
        while (!IsListEmpty(head)) {
            PLIST_ENTRY entry = RemoveHeadList(head);
            if (entry)
            {
                PRULE_ENTRY rule = CONTAINING_RECORD(entry, RULE_ENTRY, ListEntry);
                if (rule)
                {
                    KdPrint(("removed a rule with path: %wZ...", &rule->Path));
                    FreeRuleEntry(rule);
                }
            }
        }
    }
    ExFreePoolWithTag(m_Table, RULE_TABLE_TAG);
    m_Table = NULL;
    KdPrint(("destryed all rules..."));
}

VOID Globals::SetDefaultProcessPosture(_In_ RuleAction Posture) {
    m_DefaultProcessPosture = Posture;
}

RuleAction Globals::GetDefaultProcessPosture() {
    return m_DefaultProcessPosture;
}

BOOLEAN Globals::IsProcessWhitelist() {
    return m_ProcessWhitelist > 0;
}

BOOLEAN Globals::InsertRule(_In_ PUNICODE_STRING Path, _In_ RuleAction Action) {
    Locker<FastMutex> locker(m_Lock);
    PRULE_ENTRY entry = AllocateRuleEntry(Path, Action);
    if (!entry) 
    {
        KdPrint(("cannot allocate rule entry..."));
        return FALSE;
    }
    ULONG hash = HashPath(Path);
    ULONG index = hash % NUM_BUCKETS;
    if (Action == RuleAction::Allow) {
        m_ProcessWhitelist++;
    }
    KdPrint(("inserting rule with path: %wZ and action: %s at index: %lu", Path, Action == RuleAction::Block ? "Block" : "Audit", index));
    InsertTailList(&m_Table->Buckets[index], &entry->ListEntry);
    KdPrint(("inserted rule successfully..."));
    return TRUE;
}

PRULE_ENTRY Globals::LookupRule(_In_ PUNICODE_STRING Path) {
    KdPrint(("looking up rule..."));
    ULONG hash = HashPath(Path);
    ULONG index = hash % NUM_BUCKETS;

    PLIST_ENTRY head = &m_Table->Buckets[index];
    KdPrint(("looking up path %wZ at index %lu", Path, index));
    for (PLIST_ENTRY e = head->Flink; e != head; e = e->Flink) {
        PRULE_ENTRY rule = CONTAINING_RECORD(e, RULE_ENTRY, ListEntry);
        if (MatchPath(rule, Path))
        {
            KdPrint(("rule found..."));
            return rule;
        }
    }
    KdPrint(("rule not found..."));
    return NULL;
}

BOOLEAN Globals::RemoveRule(_In_ PUNICODE_STRING Path) {
    Locker<FastMutex> lock(m_Lock);
    KdPrint(("removing rule..."));
    PRULE_ENTRY rule = LookupRule(Path);
    if (!rule) 
        return FALSE;
    RemoveEntryList(&rule->ListEntry);
    FreeRuleEntry(rule);
    if (*(&rule->Action) == RuleAction::Allow && m_ProcessWhitelist) {
        m_ProcessWhitelist--;
    }
    KdPrint(("removed rule successfully..."));
    return TRUE;
}
