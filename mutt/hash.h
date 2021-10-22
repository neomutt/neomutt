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

#ifndef MUTT_LIB_HASH_H
#define MUTT_LIB_HASH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * union HashKey - The data item stored in a HashElem
 */
union HashKey
{
  const char *strkey;  ///< String key
  unsigned int intkey; ///< Integer key
};

/**
 * struct HashElem - The item stored in a Hash Table
 */
struct HashElem
{
  int type;              ///< Type of data stored in Hash Table, e.g. #DT_STRING
  union HashKey key;     ///< Key representing the data
  void *data;            ///< User-supplied data
  struct HashElem *next; ///< Linked List
};

/**
 * @defgroup hash_hdata_free_api Hash Data Free API
 *
 * Prototype for Hash Destructor callback function
 *
 * @param type Hash Type
 * @param obj  Object to free
 * @param data Data associated with the Hash
 *
 * **Contract**
 * - @a obj is not NULL
 */
typedef void (*hash_hdata_free_t)(int type, void *obj, intptr_t data);

/**
 * @defgroup hash_gen_hash_api Hash Generator API
 *
 * Prototype for a Key hashing function
 *
 * @param key       Key to hash
 * @param num_elems Number of elements in the Hash Table
 *
 * Turn a Key (a string or an integer) into a hash id.
 * The hash id will be a number between 0 and (num_elems-1).
 */
typedef size_t (*hash_gen_hash_t)(union HashKey key, size_t num_elems);

/**
 * @defgroup hash_cmp_key_api Hash Table Compare API
 *
 * Prototype for a function to compare two Hash keys
 *
 * @param a First key to compare
 * @param b Second key to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 *
 * This works like `strcmp()`.
 */
typedef int (*hash_cmp_key_t)(union HashKey a, union HashKey b);

/**
 * struct HashTable - A Hash Table
 */
struct HashTable
{
  size_t num_elems;             ///< Number of elements in the Hash Table
  bool strdup_keys : 1;         ///< if set, the key->strkey is strdup()'d
  bool allow_dups  : 1;         ///< if set, duplicate keys are allowed
  struct HashElem **table;      ///< Array of Hash keys
  hash_gen_hash_t gen_hash;     ///< Function to generate hash id from the key
  hash_cmp_key_t cmp_key;       ///< Function to compare two Hash keys
  intptr_t hdata;               ///< Data to pass to the hdata_free() function
  hash_hdata_free_t hdata_free; ///< Function to free a Hash element
};

typedef uint8_t HashFlags;             ///< Flags for mutt_hash_new(), e.g. #MUTT_HASH_STRCASECMP
#define MUTT_HASH_NO_FLAGS          0  ///< No flags are set
#define MUTT_HASH_STRCASECMP  (1 << 0) ///< use strcasecmp() to compare keys
#define MUTT_HASH_STRDUP_KEYS (1 << 1) ///< make a copy of the keys
#define MUTT_HASH_ALLOW_DUPS  (1 << 2) ///< allow duplicate keys to be inserted

void              mutt_hash_delete        (struct HashTable *table, const char *strkey, const void *data);
struct HashElem * mutt_hash_find_bucket   (const struct HashTable *table, const char *strkey);
void *            mutt_hash_find          (const struct HashTable *table, const char *strkey);
struct HashElem * mutt_hash_find_elem     (const struct HashTable *table, const char *strkey);
void              mutt_hash_free          (struct HashTable **ptr);
struct HashElem * mutt_hash_insert        (struct HashTable *table, const char *strkey, void *data);
void              mutt_hash_int_delete    (struct HashTable *table, unsigned int intkey, const void *data);
void *            mutt_hash_int_find      (const struct HashTable *table, unsigned int intkey);
struct HashElem * mutt_hash_int_insert    (struct HashTable *table, unsigned int intkey, void *data);
struct HashTable *mutt_hash_int_new       (size_t num_elems, HashFlags flags);
struct HashTable *mutt_hash_new           (size_t num_elems, HashFlags flags);
void              mutt_hash_set_destructor(struct HashTable *table, hash_hdata_free_t fn, intptr_t fn_data);
struct HashElem * mutt_hash_typed_insert  (struct HashTable *table, const char *strkey, int type, void *data);

/**
 * struct HashWalkState - Cursor to iterate through a Hash Table
 */
struct HashWalkState
{
  size_t index;          ///< Current position in table
  struct HashElem *last; ///< Current element in linked list
};

struct HashElem *mutt_hash_walk(const struct HashTable *table, struct HashWalkState *state);

#endif /* MUTT_LIB_HASH_H */
