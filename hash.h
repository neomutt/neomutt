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

#ifndef _HASH_H
#define _HASH_H

struct hash_elem
{
  const char *key;
  void *data;
  struct hash_elem *next;
};

typedef struct
{
  int nelem;
  struct hash_elem **table;
  unsigned int (*hash_string)(const unsigned char *, unsigned int);
  int (*cmp_string)(const char *, const char *);
}
HASH;

#define hash_find(table, key) hash_find_hash(table, table->hash_string ((unsigned char *)key, table->nelem), key)

#define hash_delete(table,key,data,destroy) hash_delete_hash(table, table->hash_string ((unsigned char *)key, table->nelem), key, data, destroy)

HASH *hash_create (int nelem, int lower);
int hash_insert (HASH * table, const char *key, void *data, int allow_dup);
void *hash_find_hash (const HASH * table, int hash, const char *key);
void hash_delete_hash (HASH * table, int hash, const char *key, const void *data,
		       void (*destroy) (void *));
void hash_destroy (HASH ** hash, void (*destroy) (void *));

#endif
