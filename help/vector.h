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

#include <stdbool.h>
#include <stddef.h>

#ifndef MUTT_HELP_VECTOR_H
#define MUTT_HELP_VECTOR_H

/**
 * struct Vector - Generic array to hold several elements
 */
struct Vector
{
  size_t item_size; ///< Size of a single element
  size_t size;      ///< Vector length
  size_t capa;      ///< Vector capacity
  void **data;      ///< Internal Vector data pointers
};

/**
 * typedef vector_item_free_t - Custom function to free a Vector item
 * @param ptr Vector item to be freed
 */
typedef void (*vector_item_free_t)(void **ptr);

/**
 * typedef vector_item_copy_t - Custom function to duplicate a Vector item
 * @param item Vector item to be copied
 * @retval ptr New copy of Vector item
 */
typedef void *(*vector_item_copy_t)(const void *item);

void           vector_append    (struct Vector *v, void *item);
struct Vector *vector_clone     (struct Vector *v, bool shrink, vector_item_copy_t copy);
void           vector_free      (struct Vector **v, vector_item_free_t item_free);
void *         vector_get       (struct Vector *v, size_t index, vector_item_copy_t copy);
struct Vector *vector_new       (size_t item_size);
void           vector_new_append(struct Vector **v, size_t item_size, void *item);
void           vector_shrink    (struct Vector *v);
void           vector_sort      (struct Vector *v, int (*compare)(const void *, const void *));

#endif /* MUTT_HELP_VECTOR_H */
