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

static unsigned int hash_string (const unsigned char *s, unsigned int n)
{
  unsigned int h = 0;

  while (*s)
    h += (h << 7) + *s++;
  h = (h * SOMEPRIME) % n;

  return h;
}

static unsigned int hash_case_string (const unsigned char *s, unsigned int n)
{
  unsigned int h = 0;

  while (*s)
    h += (h << 7) + tolower (*s++);
  h = (h * SOMEPRIME) % n;

  return h;
}

HASH *hash_create (int nelem, int lower)
{
  HASH *table = safe_malloc (sizeof (HASH));
  if (nelem == 0)
    nelem = 2;
  table->nelem = nelem;
  table->table = safe_calloc (nelem, sizeof (struct hash_elem *));
  if (lower)
  {
    table->hash_string = hash_case_string;
    table->cmp_string = mutt_strcasecmp;
  }
  else
  {
    table->hash_string = hash_string;
    table->cmp_string = mutt_strcmp;
  }
  return table;
}

/* table        hash table to update
 * key          key to hash on
 * data         data to associate with `key'
 * allow_dup    if nonzero, duplicate keys are allowed in the table 
 */
int hash_insert (HASH * table, const char *key, void *data, int allow_dup)
{
  struct hash_elem *ptr;
  unsigned int h;

  ptr = (struct hash_elem *) safe_malloc (sizeof (struct hash_elem));
  h = table->hash_string ((unsigned char *) key, table->nelem);
  ptr->key = key;
  ptr->data = data;

  if (allow_dup)
  {
    ptr->next = table->table[h];
    table->table[h] = ptr;
  }
  else
  {
    struct hash_elem *tmp, *last;
    int r;

    for (tmp = table->table[h], last = NULL; tmp; last = tmp, tmp = tmp->next)
    {
      r = table->cmp_string (tmp->key, key);
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
  }
  return h;
}

void *hash_find_hash (const HASH * table, int hash, const char *key)
{
  struct hash_elem *ptr = table->table[hash];
  for (; ptr; ptr = ptr->next)
  {
    if (table->cmp_string (key, ptr->key) == 0)
      return (ptr->data);
  }
  return NULL;
}

void hash_delete_hash (HASH * table, int hash, const char *key, const void *data,
		       void (*destroy) (void *))
{
  struct hash_elem *ptr = table->table[hash];
  struct hash_elem **last = &table->table[hash];

  while (ptr) 
  {
    if ((data == ptr->data || !data)
	&& table->cmp_string (ptr->key, key) == 0)
    {
      *last = ptr->next;
      if (destroy)
	destroy (ptr->data);
      FREE (&ptr);
      
      ptr = *last;
    }
    else
    {
      last = &ptr->next;
      ptr = ptr->next;
    }
  }
}

/* ptr		pointer to the hash table to be freed
 * destroy()	function to call to free the ->data member (optional) 
 */
void hash_destroy (HASH **ptr, void (*destroy) (void *))
{
  int i;
  HASH *pptr = *ptr;
  struct hash_elem *elem, *tmp;

  for (i = 0 ; i < pptr->nelem; i++)
  {
    for (elem = pptr->table[i]; elem; )
    {
      tmp = elem;
      elem = elem->next;
      if (destroy)
	destroy (tmp->data);
      FREE (&tmp);
    }
  }
  FREE (&pptr->table);
  FREE (ptr);		/* __FREE_CHECKED__ */
}
