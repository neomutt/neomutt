/**
 * @file
 * Keymap handling
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_KEY_KEYMAP_H
#define MUTT_KEY_KEYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"

/// Type for key storage, the rest of neomutt works fine with int type
typedef short keycode_t;

extern struct Mapping KeyNames[];

/**
 * struct Keymap - A keyboard mapping
 *
 * Macro: macro, desc, (op == OP_MACRO)
 * Binding: op
 * Both use eq, len and keys.
 */
struct Keymap
{
  char *macro;                  ///< Macro expansion (op == OP_MACRO)
  char *desc;                   ///< Description of a macro for the help menu
  short op;                     ///< Operation to perform
  short eq;                     ///< Number of leading keys equal to next entry
  short len;                    ///< Length of key sequence (unit: sizeof (keycode_t))
  keycode_t *keys;              ///< Key sequence
  STAILQ_ENTRY(Keymap) entries; ///< Linked list
};
STAILQ_HEAD(KeymapList, Keymap);

struct Keymap *keymap_alloc        (size_t len, keycode_t *keys);
void           keymap_free         (struct Keymap **pptr);
void           keymaplist_free     (struct KeymapList *kml);

struct Keymap *keymap_compare      (struct Keymap *km1, struct Keymap *km2, size_t *pos);
bool           keymap_expand_key   (struct Keymap *km, struct Buffer *buf);
void           keymap_expand_string(const char *str, struct Buffer *buf);
void           keymap_get_name     (int c, struct Buffer *buf);

int            parse_fkey          (char *str);
int            parse_keycode       (const char *str);
size_t         parse_keys          (const char *str, keycode_t *d, size_t max);

#endif /* MUTT_KEY_KEYMAP_H */
