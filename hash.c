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

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include "hash.h"
#include "lib.h"

#define SOMEPRIME 149711

static unsigned int gen_string_hash(union HashKey key, unsigned int n)
{
  unsigned int h = 0;
  unsigned char *s = (unsigned char *) key.strkey;

  while (*s)
    h += (h << 7) + *s++;
  h = (h * SOMEPRIME) % n;

  return h;
}

static int cmp_string_key(union HashKey a, union HashKey b)
{
  return mutt_strcmp(a.strkey, b.strkey);
}

static unsigned int gen_case_string_hash(union HashKey key, unsigned int n)
{
  unsigned int h = 0;
  unsigned char *s = (unsigned char *) key.strkey;

  while (*s)
    h += (h << 7) + tolower(*s++);
  h = (h * SOMEPRIME) % n;

  return h;
}

static int cmp_case_string_key(union HashKey a, union HashKey b)
{
  return mutt_strcasecmp(a.strkey, b.strkey);
}

static unsigned int gen_int_hash(union HashKey key, unsigned int n)
{
  return key.intkey % n;
}

static int cmp_int_key(union HashKey a, union HashKey b)
{
  if (a.intkey == b.intkey)
    return 0;
  if (a.intkey < b.intkey)
    return -1;
  return 1;
}

static struct Hash *new_hash(int nelem)
{
  struct Hash *table = safe_calloc(1, sizeof(struct Hash));
  if (nelem == 0)
    nelem = 2;
  table->nelem = nelem;
  table->table = safe_calloc(nelem, sizeof(struct HashElem *));
  return table;
}

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
    int r;

    for (tmp = table->table[h], last = NULL; tmp; last = tmp, tmp = tmp->next)
    {
      r = table->cmp_key(tmp->key, key);
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

int hash_insert(struct Hash *table, const char *strkey, void *data)
{
  union HashKey key;
  key.strkey = table->strdup_keys ? safe_strdup(strkey) : strkey;
  return union_hash_insert(table, key, data);
}

int int_hash_insert(struct Hash *table, unsigned int intkey, void *data)
{
  union HashKey key;
  key.intkey = intkey;
  return union_hash_insert(table, key, data);
}

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

static void *union_hash_find(const struct Hash *table, union HashKey key)
{
  struct HashElem *ptr = union_hash_find_elem(table, key);
  if (ptr)
    return ptr->data;
  else
    return NULL;
}

void *hash_find(const struct Hash *table, const char *strkey)
{
  union HashKey key;
  key.strkey = strkey;
  return union_hash_find(table, key);
}

struct HashElem *hash_find_elem(const struct Hash *table, const char *strkey)
{
  union HashKey key;
  key.strkey = strkey;
  return union_hash_find_elem(table, key);
}

void *int_hash_find(const struct Hash *table, unsigned int intkey)
{
  union HashKey key;
  key.intkey = intkey;
  return union_hash_find(table, key);
}

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

static void union_hash_delete(struct Hash *table, union HashKey key,
                              const void *data, void (*destroy)(void *))
{
  int hash;
  struct HashElem *ptr, **last;

  if (!table)
    return;

  hash = table->gen_hash(key, table->nelem);
  ptr = table->table[hash];
  last = &table->table[hash];

  while (ptr)
  {
    if ((data == ptr->data || !data) && table->cmp_key(ptr->key, key) == 0)
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

void hash_delete(struct Hash *table, const char *strkey, const void *data,
                 void (*destroy)(void *))
{
  union HashKey key;
  key.strkey = strkey;
  union_hash_delete(table, key, data, destroy);
}

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
