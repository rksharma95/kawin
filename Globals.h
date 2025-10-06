#pragma once

#include "FastMutex.h"
#include "Rule.h"

#define RULE_TABLE_TAG 'rulP'

typedef struct _RULE_HASH_TABLE {
	LIST_ENTRY Buckets[NUM_BUCKETS];
} RULE_HASH_TABLE, * PRULE_HASH_TABLE;

struct Globals {
	NTSTATUS Init();
	VOID SetDefaultProcessPosture(_In_ RuleAction Posture);
	RuleAction GetDefaultProcessPosture();
	BOOLEAN IsProcessWhitelist();
	VOID DestroyRuleHashTable();
	BOOLEAN InsertRule(_In_ PUNICODE_STRING Path, _In_ RuleAction Action);
	PRULE_ENTRY LookupRule(_In_ PUNICODE_STRING Path);
	BOOLEAN RemoveRule(_In_ PUNICODE_STRING Path);
private:
	ULONG m_ProcessWhitelist;
	RuleAction m_DefaultProcessPosture;
	PRULE_HASH_TABLE m_Table;
	FastMutex m_Lock;
};
