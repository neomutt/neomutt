/**
 * @file
 * Hash table data structure
 *
 * @authors
 * Copyright (C) 1996-2009 Michael R. Elkins <me@mutt.org>
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
 * @page hash Hash table data structure
 *
 * Hash table data structure.
 *
 * | Function               | Description
 * | :--------------------- | :---------------------------------------------------------
 * | hash_create()          | Create a new Hash table (with string keys)
 * | hash_delete()          | Remove an element from a Hash table
 * | hash_destroy()         | Destroy a hash table
 * | hash_find()            | Find the HashElem data in a Hash table element using a key
 * | hash_find_bucket()     | Find the HashElem in a Hash table element using a key
 * | hash_find_elem()       | Find the HashElem in a Hash table element using a key
 * | hash_insert()          | Add a new element to the Hash table (with string keys)
 * | hash_walk()            | Iterate through all the HashElem's in a Hash table
 * | int_hash_create()      | Create a new Hash table (with integer keys)
 * | int_hash_delete()      | Remove an element from a Hash table
 * | int_hash_find()        | Find the HashElem data in a Hash table element using a key
 * | int_hash_insert()      | Add a new element to the Hash table (with integer keys)
 */

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include "hash.h"
#include "memory.h"
#include "string2.h"

#define SOMEPRIME 149711

/**
 * gen_string_hash - Generate a hash from a string
 * @param key String key
 * @param n   Number of elements in the Hash table
 * @retval num Cryptographic hash of the string
 */
static unsigned int gen_string_hash(union HashKey key, unsigned int n)
{
  unsigned int h = 0;
  unsigned char *s = (unsigned char *) key.strkey;

  while (*s)
    h += ((h << 7) + *s++);
  h = (h * SOMEPRIME) % n;

  return h;
}

/**
 * cmp_string_key - Compare two string keys
 * @param a First key to compare
 * @param b Second key to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int cmp_string_key(union HashKey a, union HashKey b)
{
  return mutt_strcmp(a.strkey, b.strkey);
}

/**
 * gen_case_string_hash - Generate a hash from a string (ignore the case)
 * @param key String key
 * @param n   Number of elements in the Hash table
 * @retval num Cryptographic hash of the string
 */
static unsigned int gen_case_string_hash(union HashKey key, unsigned int n)
{
  unsigned int h = 0;
  unsigned char *s = (unsigned char *) key.strkey;

  while (*s)
    h += ((h << 7) + tolower(*s++));
  h = (h * SOMEPRIME) % n;

  return h;
}

/**
 * cmp_case_string_key - Compare two string keys (ignore case)
 * @param a First key to compare
 * @param b Second key to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int cmp_case_string_key(union HashKey a, union HashKey b)
{
  return mutt_strcasecmp(a.strkey, b.strkey);
}

/**
 * gen_int_hash - Generate a hash from an integer
 * @param key Integer key
 * @param n   Number of elements in the Hash table
 * @retval num Cryptographic hash of the integer
 */
static unsigned int gen_int_hash(union HashKey key, unsigned int n)
{
  return key.intkey % n;
}

/**
 * cmp_int_key - Compare two integer keys
 * @param a First key to compare
 * @param b Second key to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
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
 * new_hash - Create a new Hash table
 * @param nelem Number of elements it should contain
 * @retval ptr New Hash table
 *
 * The Hash table can contain more elements than nelem, but they will be
 * chained together.
 */
static struct Hash *new_hash(int nelem)
{
  struct Hash *table = safe_calloc(1, sizeof(struct Hash));
  if (nelem == 0)
    nelem = 2;
  table->nelem = nelem;
  table->table = safe_calloc(nelem, sizeof(struct HashElem *));
  return table;
}

/**
 * union_hash_insert - Insert into a hash table using a union as a key
 * @param table     Hash table to update
 * @param key       Key to hash on
 * @param data      Data to associate with `key'
 * @retval -1 on error
 * @retval >=0 on success, index into the hash table
 */
static int union_hash_insert(struct Hash *table, union HashKey key, void *data)
{
  struct HashElem *ptr = NULL;
  unsigned int h;

  ptr = safe_malloc(sizeof(struct HashElem));
  h = table->gen_hash(key, table->nelem);
  ptr->key = key;
  ptr->data = data;

  if (table->allow_dups)
  {
    ptr->next = table->table[h];
    table->table[h] = ptr;
  }
  else
  {
    struct HashElem *tmp = NULL, *last = NULL;

    for (tmp = table->table[h], last = NULL; tmp; last = tmp, tmp = tmp->next)
    {
      int r = table->cmp_key(tmp->key, key);
      if (r == 0)
      {
        FREE(&ptr);
        return -1;
      }
      if (r > 0)
        break;
    }
    if (last)
      last->next = ptr;
    else
      table->table[h] = ptr;
    ptr->next = tmp;
  }
  return h;
}

/**
 * union_hash_find_elem - Find a HashElem in a Hash table element using a key
 * @param table Hash table to search
 * @param key   Key (either string or integer)
 * @retval ptr HashElem matching the key
 */
static struct HashElem *union_hash_find_elem(const struct Hash *table, union HashKey key)
{
  int hash;
  struct HashElem *ptr = NULL;

  if (!table)
    return NULL;

  hash = table->gen_hash(key, table->nelem);
  ptr = table->table[hash];
  for (; ptr; ptr = ptr->next)
  {
    if (table->cmp_key(key, ptr->key) == 0)
      return ptr;
  }
  return NULL;
}

/**
 * union_hash_find - Find the HashElem data in a Hash table element using a key
 * @param table Hash table to search
 * @param key   Key (either string or integer)
 * @retval ptr Data attached to the HashElem matching the key
 */
static void *union_hash_find(const struct Hash *table, union HashKey key)
{
  struct HashElem *ptr = union_hash_find_elem(table, key);
  if (ptr)
    return ptr->data;
  else
    return NULL;
}

/**
 * union_hash_delete - Remove an element from a Hash table
 * @param table   Hash table to use
 * @param key     Key (either string or integer)
 * @param data    Private data to match (or NULL for any match)
 * @param destroy Callback function to free the HashElem's data
 */
static void union_hash_delete(struct Hash *table, union HashKey key,
                              const void *data, void (*destroy)(void *))
{
  int hash;
  struct HashElem *ptr = NULL, **last = NULL;

  if (!table)
    return;

  hash = table->gen_hash(key, table->nelem);
  ptr = table->table[hash];
  last = &table->table[hash];

  while (ptr)
  {
    if (((data == ptr->data) || !data) && table->cmp_key(ptr->key, key) == 0)
    {
      *last = ptr->next;
      if (destroy)
        destroy(ptr->data);
      if (table->strdup_keys)
        FREE(&ptr->key.strkey);
      FREE(&ptr);

      ptr = *last;
    }
    else
    {
      last = &ptr->next;
      ptr = ptr->next;
    }
  }
}

/**
 * hash_create - Create a new Hash table (with string keys)
 * @param nelem Number of elements it should contain
 * @param flags Flags, e.g. #MUTT_HASH_STRCASECMP
 * @retval ptr New Hash table
 */
struct Hash *hash_create(int nelem, int flags)
{
  struct Hash *table = new_hash(nelem);
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
 * int_hash_create - Create a new Hash table (with integer keys)
 * @param nelem Number of elements it should contain
 * @param flags Flags, e.g. #MUTT_HASH_ALLOW_DUPS
 * @retval ptr New Hash table
 */
struct Hash *int_hash_create(int nelem, int flags)
{
  struct Hash *table = new_hash(nelem);
  table->gen_hash = gen_int_hash;
  table->cmp_key = cmp_int_key;
  if (flags & MUTT_HASH_ALLOW_DUPS)
    table->allow_dups = true;
  return table;
}

/**
 * hash_insert - Add a new element to the Hash table (with string keys)
 * @param table  Hash table (with string keys)
 * @param strkey String key
 * @param data   Private data associated with the key
 * @retval -1 on error
 * @retval >=0 on success, index into the hash table
 */
int hash_insert(struct Hash *table, const char *strkey, void *data)
{
  union HashKey key;
  key.strkey = table->strdup_keys ? safe_strdup(strkey) : strkey;
  return union_hash_insert(table, key, data);
}

/**
 * int_hash_insert - Add a new element to the Hash table (with integer keys)
 * @param table  Hash table (with integer keys)
 * @param intkey Integer key
 * @param data   Private data associated with the key
 * @retval -1 on error
 * @retval >=0 on success, index into the hash table
 */
int int_hash_insert(struct Hash *table, unsigned int intkey, void *data)
{
  union HashKey key;
  key.intkey = intkey;
  return union_hash_insert(table, key, data);
}

/**
 * hash_find - Find the HashElem data in a Hash table element using a key
 * @param table  Hash table to search
 * @param strkey String key to search for
 * @retval ptr Data attached to the HashElem matching the key
 */
void *hash_find(const struct Hash *table, const char *strkey)
{
  union HashKey key;
  key.strkey = strkey;
  return union_hash_find(table, key);
}

/**
 * hash_find_elem - Find the HashElem in a Hash table element using a key
 * @param table  Hash table to search
 * @param strkey String key to search for
 * @retval ptr HashElem matching the key
 */
struct HashElem *hash_find_elem(const struct Hash *table, const char *strkey)
{
  union HashKey key;
  key.strkey = strkey;
  return union_hash_find_elem(table, key);
}

/**
 * int_hash_find - Find the HashElem data in a Hash table element using a key
 * @param table  Hash table to search
 * @param intkey Integer key
 * @retval ptr Data attached to the HashElem matching the key
 */
void *int_hash_find(const struct Hash *table, unsigned int intkey)
{
  union HashKey key;
  key.intkey = intkey;
  return union_hash_find(table, key);
}

/**
 * hash_find_bucket - Find the HashElem in a Hash table element using a key
 * @param table Hash table to search
 * @param strkey String key to search for
 * @retval ptr HashElem matching the key
 *
 * Unlike hash_find_elem(), this will return the first matching entry.
 */
struct HashElem *hash_find_bucket(const struct Hash *table, const char *strkey)
{
  union HashKey key;
  int hash;

  if (!table)
    return NULL;

  key.strkey = strkey;
  hash = table->gen_hash(key, table->nelem);
  return table->table[hash];
}

/**
 * hash_delete - Remove an element from a Hash table
 * @param table   Hash table to use
 * @param strkey  String key to match
 * @param data    Private data to match (or NULL for any match)
 * @param destroy Callback function to free the HashElem's data
 */
void hash_delete(struct Hash *table, const char *strkey, const void *data,
                 void (*destroy)(void *))
{
  union HashKey key;
  key.strkey = strkey;
  union_hash_delete(table, key, data, destroy);
}

/**
 * int_hash_delete - Remove an element from a Hash table
 * @param table   Hash table to use
 * @param intkey  Integer key to match
 * @param data    Private data to match (or NULL for any match)
 * @param destroy Callback function to free the HashElem's data
 */
void int_hash_delete(struct Hash *table, unsigned int intkey, const void *data,
                     void (*destroy)(void *))
{
  union HashKey key;
  key.intkey = intkey;
  union_hash_delete(table, key, data, destroy);
}

/**
 * hash_destroy - Destroy a hash table
 * @param ptr     Pointer to the hash table to be freed
 * @param destroy Function to call to free the ->data member (optional)
 */
void hash_destroy(struct Hash **ptr, void (*destroy)(void *))
{
  struct Hash *pptr = NULL;
  struct HashElem *elem = NULL, *tmp = NULL;

  if (!ptr || !*ptr)
    return;

  pptr = *ptr;
  for (int i = 0; i < pptr->nelem; i++)
  {
    for (elem = pptr->table[i]; elem;)
    {
      tmp = elem;
      elem = elem->next;
      if (destroy)
        destroy(tmp->data);
      if (pptr->strdup_keys)
        FREE(&tmp->key.strkey);
      FREE(&tmp);
    }
  }
  FREE(&pptr->table);
  FREE(ptr);
}

/**
 * hash_walk - Iterate through all the HashElem's in a Hash table
 * @param table Hash table to search
 * @param state Cursor to keep track
 * @retval ptr  Next HashElem in the Hash table
 * @retval NULL When the last HashElem has been seen
 */
struct HashElem *hash_walk(const struct Hash *table, struct HashWalkState *state)
{
  if (state->last && state->last->next)
  {
    state->last = state->last->next;
    return state->last;
  }

  if (state->last)
    state->index++;

  while (state->index < table->nelem)
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
