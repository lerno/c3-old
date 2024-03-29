// Copyright (c) 2019 Christoffer Lerno. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "symtab.h"
#include <string.h>
#include <stdlib.h>
#include "../utils/errors.h"
#include <assert.h>
#include "../utils/lib.h"
#include "utils/malloc.h"
#include "tokens.h"

#define TABLE_MAX_LOAD 0.75
#define MAX_HASH_SIZE (1024 * 1024)


typedef struct _SymEntry
{
	const char *value;
	TokenType type;
	uint32_t key_len;
	uint32_t hash;
} SymEntry;

typedef struct _SymTab
{
	uint32_t count;
	uint32_t capacity;
	SymEntry *entries;
} SymTab;

typedef struct _Entry
{
	const char *key;
	uint32_t key_len;
	uint32_t hash;
	void *value;
} Entry;


static SymTab symtab;

void symtab_init(uint32_t capacity)
{
	assert (is_power_of_two(capacity) && "Must be a power of two");
	if (symtab.capacity != 0)
	{
		free(symtab.entries);
	}
	size_t size = capacity * sizeof(SymEntry);
	symtab.entries = MALLOC(size);
	memset(symtab.entries, 0, size);
	symtab.count = 0;
	symtab.capacity = capacity;

	// Add keywords.
	for (int i = 0; i < TOKEN_EOF; i++)
	{
		const char* name = token_type_to_string(i);
		// Skip non-keywords
		if (!is_lower(name[0]))
		{
			if (name[0] != '@' || !is_lower(name[1])) continue;
		}
		int len = strlen(name);
		TokenType type = (TokenType)i;
		const char* interned = symtab_add(name, strlen(name), fnv1a(name, len), &type);
		assert(type == i);
		assert(symtab_add(name, strlen(name), fnv1a(name, len), &type) == interned);

	}
}

static inline SymEntry *entry_find(const char *key, uint32_t key_len, uint32_t hash)
{
	uint32_t index = hash & (symtab.capacity - 1);
	while (1)
	{
		SymEntry *entry = &symtab.entries[index];
		if (entry->key_len == key_len && (entry->value == key || memcmp(key, entry->value, key_len) == 0)) return entry;
		if (entry->value == NULL)
		{
			return entry;
		}
		index = (index + 1) % (symtab.capacity - 1);
	}
}

const char *symtab_add(const char *symbol, uint32_t len, uint32_t fnv1hash, TokenType *type)
{
	if (symtab.count + 1 > symtab.capacity * TABLE_MAX_LOAD)
	{
		FATAL_ERROR("Symtab exceeded capacity, please increase --symtab.");
	}
	SymEntry *entry = entry_find(symbol, len, fnv1hash);
	if (entry->value)
	{
		*type = entry->type;
		return entry->value;
	}

	char *copy = MALLOC(len + 1);
	memcpy(copy, symbol, len);
	copy[len] = '\0';
	entry->value = copy;
	entry->key_len = len;
	entry->hash = fnv1hash;
	entry->type = *type;
	symtab.count++;
	return entry->value;
}

void stable_init(STable *table, uint32_t initial_size)
{
	assert(initial_size && "Size must be larger than 0");
	assert (is_power_of_two(initial_size) && "Must be a power of two");

	SEntry *entries = MALLOC(initial_size * sizeof(Entry));
	for (uint32_t i = 0; i < initial_size; i++)
	{
		entries[i].key = NULL;
		entries[i].value = NULL;
	}
	table->count = 0;
	table->capacity = initial_size;
	table->entries = entries;
}

void stable_clear(STable *table)
{
	memset(table->entries, 0, table->capacity * sizeof(Entry));
	table->count = 0;
}

#define TOMBSTONE ((void *)0x01)
static SEntry *sentry_find(SEntry *entries, uint32_t capacity, const char *key)
{
	uint32_t index = (uint32_t)((((uintptr_t)key) >> 2u) & (capacity - 1));
	SEntry *tombstone = NULL;
	while (1)
	{
		SEntry *entry = &entries[index];
		if (entry->key == key) return entry;
		if (entry->key == NULL)
		{
			if (entry->value != TOMBSTONE)
			{
				return tombstone ? tombstone : entry;
			}
			else
			{
				if (!tombstone) tombstone = entry;
			}
		}
		index = (index + 1) & (capacity - 1);
	}
}


void *stable_set(STable *table, const char *key, void *value)
{
	assert(value && "Cannot insert NULL");
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
	{
		ASSERT(table->capacity < MAX_HASH_SIZE, "Table size too large, exceeded %d", MAX_HASH_SIZE);

		uint32_t new_capacity = table->capacity ? (table->capacity << 1u) : 16u;
		SEntry *new_data = MALLOC(new_capacity * sizeof(SEntry));
		for (uint32_t i = 0; i < new_capacity; i++)
		{
			new_data[i].key = NULL;
			new_data[i].value = NULL;
		}
		table->count = 0;
		for (uint32_t i = 0; i < table->capacity; i++)
		{
			SEntry *entry = &table->entries[i];
			if (!entry->key) continue;
			table->count++;
			SEntry *dest = sentry_find(new_data, new_capacity, entry->key);
			*dest = *entry;
		}
		table->entries = new_data;
		table->capacity = new_capacity;
	}

	SEntry *entry = sentry_find(table->entries, table->capacity, key);
	void *old = entry->value && entry->value != TOMBSTONE ? entry->value : NULL;
	entry->key = key;
	entry->value = value;
	if (!old) table->count++;
	return old;
}


void *stable_get(STable *table, const char *key)
{
	if (!table->entries) return NULL;
	SEntry *entry = sentry_find(table->entries, table->capacity, key);
	return entry->key == NULL ? NULL : entry->value;
}

void *stable_delete(STable *table, const char *key)
{
	if (!table->count) return NULL;
	SEntry *entry = sentry_find(table->entries, table->capacity, key);
	if (!entry->key) return NULL;
	void *value = entry->value;
	entry->key = NULL;
	entry->value = TOMBSTONE;
	return value;
}
