/**
 * @file
 * Expandable array
 *
 * @authors
 * Copyright (C) 2019 Tran Manh Tu <xxlaguna93@gmail.com>
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
 * @page help_vector Expandable array
 *
 * Expandable array
 */

#include <stdio.h>
#include <stdlib.h>
#include "mutt/mutt.h"
#include "vector.h"

#define VECTOR_INIT_CAPACITY 16

/**
 * vector_free - Free a Vector
 * @param v         Vector to free
 * @param item_free Function to free the Vector's contents
 */
void vector_free(struct Vector **v, vector_item_free_t item_free)
{
  if (!v || !*v)
    return;

  for (size_t i = 0; i < (*v)->size; i++)
  {
    if (item_free)
      item_free(&((*v)->data[i]));
    else
      FREE(&((*v)->data[i]));
  }
  FREE(&(*v)->data);
  FREE(v);
}

/**
 * vector_shrink - Resize a Vector to save space
 * @param v Vector to resize
 */
void vector_shrink(struct Vector *v)
{
  if (!v)
    return;

  mutt_mem_realloc(v->data, v->size * v->item_size);
  v->capa = v->size;
}

/**
 * vector_new - Create a new Vector
 * @param item_size Size of items in Vector
 * @retval ptr      New Vector
 */
struct Vector *vector_new(size_t item_size)
{
  struct Vector *v = NULL;
  if (item_size == 0)
    return NULL;

  v = mutt_mem_malloc(sizeof(struct Vector));
  v->item_size = item_size;
  v->size = 0;
  v->capa = VECTOR_INIT_CAPACITY;
  v->data = mutt_mem_calloc(v->capa, sizeof(void *) * v->item_size);

  return v;
}

/**
 * vector_append - Add an item to the Vector
 * @param v    Vector to add to
 * @param item Item to add
 */
void vector_append(struct Vector *v, void *item)
{
  if (!v || !item)
    return;

  if (v->size >= v->capa)
  {
    v->capa = (v->capa == 0) ? VECTOR_INIT_CAPACITY : (v->capa * 2);
    mutt_mem_realloc(v->data, v->capa * v->item_size);
  }

  v->data[v->size] = item;
  v->size++;
}

/**
 * vector_new_append - Append a new item to a Vector
 * @param v         Vector to append to
 * @param item_size Size of item to add
 * @param item      Item to add to Vector
 */
void vector_new_append(struct Vector **v, size_t item_size, void *item)
{
  if ((item_size == 0) || !item)
    return;

  if (!v || !*v)
    *v = vector_new(item_size);

  vector_append(*v, item);
}
/**
 * vector_get - Get an item from a Vector
 * @param v     Vector to use
 * @param index Index in Vector
 * @param copy  Function to copy item (may be NULL)
 * @retval ptr  Item selected
 * @retval NULL Invalid index
 */
void *vector_get(struct Vector *v, size_t index, vector_item_copy_t copy)
{
  if (!v || (index >= v->size))
    return NULL;

  return ((copy) ? copy(v->data[index]) : v->data[index]);
}

/**
 * vector_clone - Copy a Vector
 * @param v      Vector to copy
 * @param shrink true if the Vector should be minimised
 * @param copy   Function to copy a Vector item
 * @retval ptr   Duplicated Vector
 */
struct Vector *vector_clone(struct Vector *v, bool shrink, vector_item_copy_t copy)
{
  if (!v)
    return NULL;

  struct Vector *clone = vector_new(v->item_size);
  for (size_t i = 0; i < v->size; i++)
    vector_append(clone, vector_get(v, i, copy));

  if (shrink)
    vector_shrink(clone);

  return clone;
}

/**
 * vector_sort - Sort a Vector
 * @param v       Vector to sort
 * @param compare Function to compare two items
 */
void vector_sort(struct Vector *v, int (*compare)(const void *, const void *))
{
  if (!v)
    return;

  qsort(v->data, v->size, sizeof(void *), compare);
}
