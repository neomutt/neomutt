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

union hash_key
{
  const char *strkey;
  unsigned int intkey;
};

struct hash_elem
{
  union hash_key key;
  void *data;
  struct hash_elem *next;
};

typedef struct
{
  int nelem;
  struct hash_elem **table;
  unsigned int (*gen_hash)(union hash_key, unsigned int);
  int (*cmp_key)(union hash_key, union hash_key);
}
HASH;

HASH *hash_create (int nelem, int lower);
HASH *int_hash_create (int nelem);

int hash_insert (HASH * table, const char *key, void *data, int allow_dup);
int int_hash_insert (HASH *table, unsigned int key, void *data, int allow_dup);

void *hash_find (const HASH *table, const char *key);
void *int_hash_find (const HASH *table, unsigned int key);

struct hash_elem *hash_find_bucket (const HASH *table, const char *key);

void hash_delete (HASH * table, const char *key, const void *data,
                  void (*destroy) (void *));
void int_hash_delete (HASH * table, unsigned int key, const void *data,
                  void (*destroy) (void *));

void hash_destroy (HASH ** hash, void (*destroy) (void *));

#endif
