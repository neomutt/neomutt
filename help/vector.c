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

#include <stdio.h>
#include <stdlib.h>
#include "vector.h"
#include "mutt/mutt.h"

/**
 * vector_free - Free a list of Help documents
 * @param list      List to free
 * @param item_free Function to free the list contents
 */
void vector_free(struct Vector **list, void (*item_free)(void **))
{
  if (!list || !*list)
    return;

  for (size_t i = 0; i < (*list)->size; i++)
  {
    item_free(&((*list)->data[i]));
  }
  FREE(&(*list)->data);
  FREE(list);
}

/**
 * vector_shrink - Resize a List of Help documents to save space
 * @param list List to resize
 */
void vector_shrink(struct Vector *list)
{
  if (!list)
    return;

  mutt_mem_realloc(list->data, list->size * list->item_size);
  list->capa = list->size;
}

/**
 * vector_new - Create a new list of Help documents
 * @param item_size Size of items in list
 * @retval ptr New Help list
 */
struct Vector *vector_new(size_t item_size)
{
  struct Vector *list = NULL;
  if (item_size == 0)
    return NULL;

  list = mutt_mem_malloc(sizeof(struct Vector));
  list->item_size = item_size;
  list->size = 0;
  list->capa = VECTOR_INIT_CAPACITY;
  list->data = mutt_mem_calloc(list->capa, sizeof(void *) * list->item_size);

  return list;
}

/**
 * vector_append - Add an item to the Help document list
 * @param list List to add to
 * @param item Item to add
 */
void vector_append(struct Vector *list, void *item)
{
  if (!list || !item)
    return;

  if (list->size >= list->capa)
  {
    list->capa = (list->capa == 0) ? VECTOR_INIT_CAPACITY : (list->capa * 2);
    mutt_mem_realloc(list->data, list->capa * list->item_size);
  }

  list->data[list->size] = mutt_mem_calloc(1, list->item_size);
  list->data[list->size] = item;
  list->size++;
}

/**
 * vector_new_append - Append a new item to a Help document list
 * @param list      List to append to
 * @param item_size Size of item to add
 * @param item      Item to add to list
 */
void vector_new_append(struct Vector **list, size_t item_size, void *item)
{
  if ((item_size == 0) || !item)
    return;

  if (!list || !*list)
    *list = vector_new(item_size);

  vector_append(*list, item);
}

/**
 * vector_get - Get an item from a Help document list
 * @param list  List to use
 * @param index Index in list
 * @param copy  Function to copy item (may be NULL)
 * @retval ptr  Item selected
 * @retval NULL Invalid index
 */
void *vector_get(struct Vector *list, size_t index, void *(*copy)(const void *) )
{
  if (!list || (index >= list->size))
    return NULL;

  return ((copy) ? copy(list->data[index]) : list->data[index]);
}

/**
 * vector_clone - Copy a list of Help documents
 * @param list   List to copy
 * @param shrink true if the list should be minimised
 * @param copy   Function to copy a list item
 * @retval ptr Duplicated list of Help documents
 */
struct Vector *vector_clone(struct Vector *list, bool shrink, void *(*copy)(const void *) )
{
  if (!list)
    return NULL;

  struct Vector *clone = vector_new(list->item_size);
  for (size_t i = 0; i < list->size; i++)
    vector_append(clone, vector_get(list, i, copy));

  if (shrink)
    vector_shrink(clone);

  return clone;
}

/**
 * vector_sort - Sort a list of Help documents
 * @param list    List to sort
 * @param compare Function to compare two items
 */
void vector_sort(struct Vector *list, int (*compare)(const void *, const void *))
{
  if (!list)
    return;

  qsort(list->data, list->size, sizeof(void *), compare);
}
