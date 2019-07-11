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

#ifndef MUTT_HELP_VECTOR_H
#define MUTT_HELP_VECTOR_H

#define VECTOR_INIT_CAPACITY 16

/**
 * struct Vector - Generic array to hold several elements
 */
struct Vector
{
  size_t item_size; ///< Size of a single element
  size_t size;      ///< List length
  size_t capa;      ///< List capacity
  void **data;      ///< Internal list data pointers
};

void           vector_append    (struct Vector *list, void *item);
struct Vector *vector_clone     (struct Vector *list, bool shrink, void *(*copy)(const void *));
void           vector_free      (struct Vector **list, void (*item_free)(void **));
void *         vector_get       (struct Vector *list, size_t index, void *(*copy)(const void *));
struct Vector *vector_new       (size_t item_size);
void           vector_new_append(struct Vector **list, size_t item_size, void *item);
void           vector_shrink    (struct Vector *list);
void           vector_sort      (struct Vector *list, int (*compare)(const void *, const void *));

#endif /* MUTT_HELP_VECTOR_H */
