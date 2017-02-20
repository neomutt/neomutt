/*
 * Copyright (C) 1996-2009 Michael R. Elkins <me@mutt.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "mutt.h"

#define SOMEPRIME 149711

static unsigned int gen_string_hash (union hash_key key, unsigned int n)
{
  unsigned int h = 0;
  unsigned char *s = (unsigned char *)key.strkey;

  while (*s)
    h += (h << 7) + *s++;
  h = (h * SOMEPRIME) % n;

  return h;
}

static int cmp_string_key (union hash_key a, union hash_key b)
{
  return mutt_strcmp (a.strkey, b.strkey);
}

static unsigned int gen_case_string_hash (union hash_key key, unsigned int n)
{
  unsigned int h = 0;
  unsigned char *s = (unsigned char *)key.strkey;

  while (*s)
    h += (h << 7) + tolower (*s++);
  h = (h * SOMEPRIME) % n;

  return h;
}

static int cmp_case_string_key (union hash_key a, union hash_key b)
{
  return mutt_strcasecmp (a.strkey, b.strkey);
}

static unsigned int gen_int_hash (union hash_key key, unsigned int n)
{
  return key.intkey % n;
}

static int cmp_int_key (union hash_key a, union hash_key b)
{
  if (a.intkey == b.intkey)
    return 0;
  if (a.intkey < b.intkey)
    return -1;
  return 1;
}

static HASH *new_hash (int nelem)
{
  HASH *table = safe_calloc (1, sizeof (HASH));
  if (nelem == 0)
    nelem = 2;
  table->nelem = nelem;
  table->curnelem = 0;
  table->table = safe_calloc (nelem, sizeof (struct hash_elem *));
  return table;
}

HASH *hash_create (int nelem, int flags)
{
  HASH *table = new_hash (nelem);
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
    table->strdup_keys = 1;
  if (flags & MUTT_HASH_ALLOW_DUPS)
    table->allow_dups = 1;
  return table;
}

HASH *int_hash_create (int nelem, int flags)
{
  HASH *table = new_hash (nelem);
  table->gen_hash = gen_int_hash;
  table->cmp_key = cmp_int_key;
  if (flags & MUTT_HASH_ALLOW_DUPS)
    table->allow_dups = 1;
  return table;
}

HASH *hash_resize (HASH *ptr, int nelem, int lower)
{
  HASH *table;
  struct hash_elem *elem, *tmp;
  int i;

  table = hash_create (nelem, lower);

  for (i = 0; i < ptr->nelem; i++)
  {
    for (elem = ptr->table[i]; elem; )
    {
      tmp = elem;
      elem = elem->next;
      hash_insert (table, tmp->key.strkey, tmp->data);
      FREE (&tmp);
    }
  }
  FREE (&ptr->table);
  FREE (&ptr);
  return table;
}

/* table        hash table to update
 * key          key to hash on
 * data         data to associate with `key'
 * allow_dup    if nonzero, duplicate keys are allowed in the table 
 */
static int union_hash_insert (HASH * table, union hash_key key, void *data)
{
  struct hash_elem *ptr;
  unsigned int h;

  ptr = safe_malloc (sizeof (struct hash_elem));
  h = table->gen_hash (key, table->nelem);
  ptr->key = key;
  ptr->data = data;

  if (table->allow_dups)
  {
    ptr->next = table->table[h];
    table->table[h] = ptr;
    table->curnelem++;
  }
  else
  {
    struct hash_elem *tmp, *last;
    int r;

    for (tmp = table->table[h], last = NULL; tmp; last = tmp, tmp = tmp->next)
    {
      r = table->cmp_key (tmp->key, key);
      if (r == 0)
      {
	FREE (&ptr);
	return (-1);
      }
      if (r > 0)
	break;
    }
    if (last)
      last->next = ptr;
    else
      table->table[h] = ptr;
    ptr->next = tmp;
    table->curnelem++;
  }
  return h;
}

int hash_insert (HASH * table, const char *strkey, void *data)
{
  union hash_key key;
  key.strkey = table->strdup_keys ? safe_strdup (strkey) : strkey;
  return union_hash_insert (table, key, data);
}

int int_hash_insert (HASH * table, unsigned int intkey, void *data)
{
  union hash_key key;
  key.intkey = intkey;
  return union_hash_insert (table, key, data);
}

static struct hash_elem *union_hash_find_elem (const HASH *table, union hash_key key)
{
  int hash;
  struct hash_elem *ptr;

  if (!table)
    return NULL;

  hash = table->gen_hash (key, table->nelem);
  ptr = table->table[hash];
  for (; ptr; ptr = ptr->next)
  {
    if (table->cmp_key (key, ptr->key) == 0)
      return (ptr);
  }
  return NULL;
}

void hash_set_data (HASH *table, const char *key, void *data)
{
  if (!table)
    return;

  union hash_key k;
  k.strkey = key;

  unsigned int hash = table->gen_hash (k, table->nelem);

  struct hash_elem *ptr = table->table[hash];
  if (!ptr)
    return;

  ptr->data = data;
}

static void *union_hash_find (const HASH *table, union hash_key key)
{
  struct hash_elem *ptr = union_hash_find_elem (table, key);
  if (ptr)
    return ptr->data;
  else
    return NULL;
}

void *hash_find (const HASH *table, const char *strkey)
{
  union hash_key key;
  key.strkey = strkey;
  return union_hash_find (table, key);
}

struct hash_elem *hash_find_elem (const HASH *table, const char *strkey)
{
  union hash_key key;
  key.strkey = strkey;
  return union_hash_find_elem (table, key);
}

void *int_hash_find (const HASH *table, unsigned int intkey)
{
  union hash_key key;
  key.intkey = intkey;
  return union_hash_find (table, key);
}

struct hash_elem *hash_find_bucket (const HASH *table, const char *strkey)
{
  union hash_key key;
  int hash;

  if (!table)
    return NULL;

  key.strkey = strkey;
  hash = table->gen_hash (key, table->nelem);
  return table->table[hash];
}

static void union_hash_delete (HASH *table, union hash_key key, const void *data,
		       void (*destroy) (void *))
{
  int hash;
  struct hash_elem *ptr, **last;

  if (!table)
    return;

  hash = table->gen_hash (key, table->nelem);
  ptr = table->table[hash];
  last = &table->table[hash];

  while (ptr)
  {
    if ((data == ptr->data || !data)
	&& table->cmp_key (ptr->key, key) == 0)
    {
      *last = ptr->next;
      if (destroy)
	destroy (ptr->data);
      if (table->strdup_keys)
        FREE (&ptr->key.strkey);
      FREE (&ptr);
      table->curnelem--;

      ptr = *last;
    }
    else
    {
      last = &ptr->next;
      ptr = ptr->next;
    }
  }
}

void hash_delete (HASH *table, const char *strkey, const void *data,
                  void (*destroy) (void *))
{
  union hash_key key;
  key.strkey = strkey;
  union_hash_delete (table, key, data, destroy);
}

void int_hash_delete (HASH *table, unsigned int intkey, const void *data,
                  void (*destroy) (void *))
{
  union hash_key key;
  key.intkey = intkey;
  union_hash_delete (table, key, data, destroy);
}

/* ptr		pointer to the hash table to be freed
 * destroy()	function to call to free the ->data member (optional) 
 */
void hash_destroy (HASH **ptr, void (*destroy) (void *))
{
  int i;
  HASH *pptr;
  struct hash_elem *elem, *tmp;

  if (!ptr || !*ptr)
    return;

  pptr = *ptr;
  for (i = 0 ; i < pptr->nelem; i++)
  {
    for (elem = pptr->table[i]; elem; )
    {
      tmp = elem;
      elem = elem->next;
      if (destroy)
	destroy (tmp->data);
      if (pptr->strdup_keys)
        FREE (&tmp->key.strkey);
      FREE (&tmp);
    }
  }
  FREE (&pptr->table);
  FREE (ptr);		/* __FREE_CHECKED__ */
}

struct hash_elem *hash_walk(const HASH *table, struct hash_walk_state *state)
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

