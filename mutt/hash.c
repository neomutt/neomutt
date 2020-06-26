/**
 * @file
 * Hash Table data structure
 *
 * @authors
 * Copyright (C) 1996-2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2020 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page hash Hash Table data structure
 *
 * Hash Table data structure.
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include "hash.h"
#include "memory.h"
#include "string2.h"

#define SOME_PRIME 149711

/**
 * gen_string_hash - Generate a hash from a string - Implements hash_gen_hash_t
 */
static size_t gen_string_hash(union HashKey key, size_t num_elems)
{
  size_t hash = 0;
  const unsigned char *s = (const unsigned char *) key.strkey;

  while (*s != '\0')
    hash += ((hash << 7) + *s++);
  hash = (hash * SOME_PRIME) % num_elems;

  return hash;
}

/**
 * cmp_string_key - Compare two string keys - Implements hash_cmp_key_t
 */
static int cmp_string_key(union HashKey a, union HashKey b)
{
  return mutt_str_cmp(a.strkey, b.strkey);
}

/**
 * gen_case_string_hash - Generate a hash from a string (ignore the case) - Implements hash_gen_hash_t
 */
static size_t gen_case_string_hash(union HashKey key, size_t num_elems)
{
  size_t hash = 0;
  const unsigned char *s = (const unsigned char *) key.strkey;

  while (*s != '\0')
    hash += ((hash << 7) + tolower(*s++));
  hash = (hash * SOME_PRIME) % num_elems;

  return hash;
}

/**
 * cmp_case_string_key - Compare two string keys (ignore case) - Implements hash_cmp_key_t
 */
static int cmp_case_string_key(union HashKey a, union HashKey b)
{
  return mutt_istr_cmp(a.strkey, b.strkey);
}

/**
 * gen_int_hash - Generate a hash from an integer - Implements hash_gen_hash_t
 */
static size_t gen_int_hash(union HashKey key, size_t num_elems)
{
  return (key.intkey % num_elems);
}

/**
 * cmp_int_key - Compare two integer keys - Implements hash_cmp_key_t
 */
static int cmp_int_key(union HashKey a, union HashKey b)
{
  if (a.intkey == b.intkey)
    return 0;
  if (a.intkey < b.intkey)
    return -1;
  return 1;
}

/**
 * hash_new - Create a new Hash Table
 * @param num_elems Number of elements it should contain
 * @retval ptr New Hash Table
 *
 * The Hash Table can contain more elements than num_elems, but they will be
 * chained together.
 */
static struct HashTable *hash_new(size_t num_elems)
{
  struct HashTable *table = mutt_mem_calloc(1, sizeof(struct HashTable));
  if (num_elems == 0)
    num_elems = 2;
  table->num_elems = num_elems;
  table->table = mutt_mem_calloc(num_elems, sizeof(struct HashElem *));
  return table;
}

/**
 * union_hash_insert - Insert into a hash table using a union as a key
 * @param table Hash Table to update
 * @param key   Key to hash on
 * @param type  Data type
 * @param data  Data to associate with key
 * @retval ptr Newly inserted HashElem
 */
static struct HashElem *union_hash_insert(struct HashTable *table,
                                          union HashKey key, int type, void *data)
{
  if (!table)
    return NULL; // LCOV_EXCL_LINE

  struct HashElem *he = mutt_mem_calloc(1, sizeof(struct HashElem));
  size_t hash = table->gen_hash(key, table->num_elems);
  he->key = key;
  he->data = data;
  he->type = type;

  if (table->allow_dups)
  {
    he->next = table->table[hash];
    table->table[hash] = he;
  }
  else
  {
    struct HashElem *tmp = NULL, *last = NULL;

    for (tmp = table->table[hash], last = NULL; tmp; last = tmp, tmp = tmp->next)
    {
      const int rc = table->cmp_key(tmp->key, key);
      if (rc == 0)
      {
        FREE(&he);
        return NULL;
      }
      if (rc > 0)
        break;
    }
    if (last)
      last->next = he;
    else
      table->table[hash] = he;
    he->next = tmp;
  }
  return he;
}

/**
 * union_hash_find_elem - Find a HashElem in a Hash Table element using a key
 * @param table Hash Table to search
 * @param key   Key (either string or integer)
 * @retval ptr HashElem matching the key
 */
static struct HashElem *union_hash_find_elem(const struct HashTable *table, union HashKey key)
{
  if (!table)
    return NULL; // LCOV_EXCL_LINE

  size_t hash = table->gen_hash(key, table->num_elems);
  struct HashElem *he = table->table[hash];
  for (; he; he = he->next)
  {
    if (table->cmp_key(key, he->key) == 0)
      return he;
  }
  return NULL;
}

/**
 * union_hash_find - Find the HashElem data in a Hash Table element using a key
 * @param table Hash Table to search
 * @param key   Key (either string or integer)
 * @retval ptr Data attached to the HashElem matching the key
 */
static void *union_hash_find(const struct HashTable *table, union HashKey key)
{
  if (!table)
    return NULL; // LCOV_EXCL_LINE
  struct HashElem *he = union_hash_find_elem(table, key);
  if (he)
    return he->data;
  return NULL;
}

/**
 * union_hash_delete - Remove an element from a Hash Table
 * @param table   Hash Table to use
 * @param key     Key (either string or integer)
 * @param data    Private data to match (or NULL for any match)
 */
static void union_hash_delete(struct HashTable *table, union HashKey key, const void *data)
{
  if (!table)
    return; // LCOV_EXCL_LINE

  size_t hash = table->gen_hash(key, table->num_elems);
  struct HashElem *he = table->table[hash];
  struct HashElem **last = &table->table[hash];

  while (he)
  {
    if (((data == he->data) || !data) && (table->cmp_key(he->key, key) == 0))
    {
      *last = he->next;
      if (table->hdata_free)
        table->hdata_free(he->type, he->data, table->hdata);
      if (table->strdup_keys)
        FREE(&he->key.strkey);
      FREE(&he);

      he = *last;
    }
    else
    {
      last = &he->next;
      he = he->next;
    }
  }
}

/**
 * mutt_hash_new - Create a new Hash Table (with string keys)
 * @param num_elems Number of elements it should contain
 * @param flags Flags, see #HashFlags
 * @retval ptr New Hash Table
 */
struct HashTable *mutt_hash_new(size_t num_elems, HashFlags flags)
{
  struct HashTable *table = hash_new(num_elems);
  if (flags & MUTT_HASH_STRCASECMP)
  {
    table->gen_hash = gen_case_string_hash;
    table->cmp_key = cmp_case_string_key;
  }
  else
  {
    table->gen_hash = gen_string_hash;
    table->cmp_key = cmp_string_key;
  }
  if (flags & MUTT_HASH_STRDUP_KEYS)
    table->strdup_keys = true;
  if (flags & MUTT_HASH_ALLOW_DUPS)
    table->allow_dups = true;
  return table;
}

/**
 * mutt_hash_int_new - Create a new Hash Table (with integer keys)
 * @param num_elems Number of elements it should contain
 * @param flags Flags, see #HashFlags
 * @retval ptr New Hash Table
 */
struct HashTable *mutt_hash_int_new(size_t num_elems, HashFlags flags)
{
  struct HashTable *table = hash_new(num_elems);
  table->gen_hash = gen_int_hash;
  table->cmp_key = cmp_int_key;
  if (flags & MUTT_HASH_ALLOW_DUPS)
    table->allow_dups = true;
  return table;
}

/**
 * mutt_hash_set_destructor - Set the destructor for a Hash Table
 * @param table   Hash Table to use
 * @param fn      Callback function to free Hash Table's resources
 * @param fn_data Data to pass to the callback function
 */
void mutt_hash_set_destructor(struct HashTable *table, hash_hdata_free_t fn, intptr_t fn_data)
{
  if (!table)
    return;
  table->hdata_free = fn;
  table->hdata = fn_data;
}

/**
 * mutt_hash_typed_insert - Insert a string with type info into a Hash Table
 * @param table  Hash Table to use
 * @param strkey String key
 * @param type   Type to associate with the key
 * @param data   Private data associated with the key
 * @retval ptr Newly inserted HashElem
 */
struct HashElem *mutt_hash_typed_insert(struct HashTable *table,
                                        const char *strkey, int type, void *data)
{
  if (!table || !strkey)
    return NULL;

  union HashKey key;
  key.strkey = table->strdup_keys ? mutt_str_dup(strkey) : strkey;
  return union_hash_insert(table, key, type, data);
}

/**
 * mutt_hash_insert - Add a new element to the Hash Table (with string keys)
 * @param table  Hash Table (with string keys)
 * @param strkey String key
 * @param data   Private data associated with the key
 * @retval ptr Newly inserted HashElem
 */
struct HashElem *mutt_hash_insert(struct HashTable *table, const char *strkey, void *data)
{
  return mutt_hash_typed_insert(table, strkey, -1, data);
}

/**
 * mutt_hash_int_insert - Add a new element to the Hash Table (with integer keys)
 * @param table  Hash Table (with integer keys)
 * @param intkey Integer key
 * @param data   Private data associated with the key
 * @retval ptr Newly inserted HashElem
 */
struct HashElem *mutt_hash_int_insert(struct HashTable *table, unsigned int intkey, void *data)
{
  if (!table)
    return NULL;
  union HashKey key;
  key.intkey = intkey;
  return union_hash_insert(table, key, -1, data);
}

/**
 * mutt_hash_find - Find the HashElem data in a Hash Table element using a key
 * @param table  Hash Table to search
 * @param strkey String key to search for
 * @retval ptr Data attached to the HashElem matching the key
 */
void *mutt_hash_find(const struct HashTable *table, const char *strkey)
{
  if (!table || !strkey)
    return NULL;
  union HashKey key;
  key.strkey = strkey;
  return union_hash_find(table, key);
}

/**
 * mutt_hash_find_elem - Find the HashElem in a Hash Table element using a key
 * @param table  Hash Table to search
 * @param strkey String key to search for
 * @retval ptr HashElem matching the key
 */
struct HashElem *mutt_hash_find_elem(const struct HashTable *table, const char *strkey)
{
  if (!table || !strkey)
    return NULL;
  union HashKey key;
  key.strkey = strkey;
  return union_hash_find_elem(table, key);
}

/**
 * mutt_hash_int_find - Find the HashElem data in a Hash Table element using a key
 * @param table  Hash Table to search
 * @param intkey Integer key
 * @retval ptr Data attached to the HashElem matching the key
 */
void *mutt_hash_int_find(const struct HashTable *table, unsigned int intkey)
{
  if (!table)
    return NULL;
  union HashKey key;
  key.intkey = intkey;
  return union_hash_find(table, key);
}

/**
 * mutt_hash_find_bucket - Find the HashElem in a Hash Table element using a key
 * @param table Hash Table to search
 * @param strkey String key to search for
 * @retval ptr HashElem matching the key
 *
 * Unlike mutt_hash_find_elem(), this will return the first matching entry.
 */
struct HashElem *mutt_hash_find_bucket(const struct HashTable *table, const char *strkey)
{
  if (!table || !strkey)
    return NULL;

  union HashKey key;

  key.strkey = strkey;
  size_t hash = table->gen_hash(key, table->num_elems);
  return table->table[hash];
}

/**
 * mutt_hash_delete - Remove an element from a Hash Table
 * @param table   Hash Table to use
 * @param strkey  String key to match
 * @param data    Private data to match (or NULL for any match)
 */
void mutt_hash_delete(struct HashTable *table, const char *strkey, const void *data)
{
  if (!table || !strkey)
    return;
  union HashKey key;
  key.strkey = strkey;
  union_hash_delete(table, key, data);
}

/**
 * mutt_hash_int_delete - Remove an element from a Hash Table
 * @param table   Hash Table to use
 * @param intkey  Integer key to match
 * @param data    Private data to match (or NULL for any match)
 */
void mutt_hash_int_delete(struct HashTable *table, unsigned int intkey, const void *data)
{
  if (!table)
    return;
  union HashKey key;
  key.intkey = intkey;
  union_hash_delete(table, key, data);
}

/**
 * mutt_hash_free - Free a hash table
 * @param[out] ptr Hash Table to be freed
 */
void mutt_hash_free(struct HashTable **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct HashTable *table = *ptr;
  struct HashElem *elem = NULL, *tmp = NULL;

  for (size_t i = 0; i < table->num_elems; i++)
  {
    for (elem = table->table[i]; elem;)
    {
      tmp = elem;
      elem = elem->next;
      if (table->hdata_free)
        table->hdata_free(tmp->type, tmp->data, table->hdata);
      if (table->strdup_keys)
        FREE(&tmp->key.strkey);
      FREE(&tmp);
    }
  }
  FREE(&table->table);
  FREE(ptr);
}

/**
 * mutt_hash_walk - Iterate through all the HashElem's in a Hash Table
 * @param table Hash Table to search
 * @param state Cursor to keep track
 * @retval ptr  Next HashElem in the Hash Table
 * @retval NULL When the last HashElem has been seen
 */
struct HashElem *mutt_hash_walk(const struct HashTable *table, struct HashWalkState *state)
{
  if (!table || !state)
    return NULL;

  if (state->last && state->last->next)
  {
    state->last = state->last->next;
    return state->last;
  }

  if (state->last)
    state->index++;

  while (state->index < table->num_elems)
  {
    if (table->table[state->index])
    {
      state->last = table->table[state->index];
      return state->last;
    }
    state->index++;
  }

  state->index = 0;
  state->last = NULL;
  return NULL;
}
