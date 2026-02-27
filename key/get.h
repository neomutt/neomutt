/**
 * @file
 * Get a key from the user
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_KEY_GET_H
#define MUTT_KEY_GET_H

#include <stdint.h>
#include "mutt/lib.h"
#include "menu/lib.h"
#include "keymap.h"

struct MenuDefinition;

typedef uint8_t GetChFlags;             ///< Flags for mutt_getch(), e.g. #GETCH_NO_FLAGS
#define GETCH_NO_FLAGS              0   ///< No flags are set
#define GETCH_IGNORE_MACRO    (1 << 0)  ///< Don't use MacroEvents
#define GETCH_NO_COUNTER      (1 << 1)  ///< km_dokey(): disable numeric count prefix parsing

/// Maximum number of digits in a key count prefix, e.g. `123j`
#define KEY_COUNT_MAX_DIGITS 6
/// Maximum number of keys in a key sequence, e.g. `abc`
#define KEY_SEQ_MAX_LEN 8

typedef uint8_t KeyGatherFlags;         ///< Flags for gather_functions(), e.g. #KEY_GATHER_NO_MATCH
#define KEY_GATHER_NO_MATCH         0   ///< No bindings match the search string
#define KEY_GATHER_MATCH      (1 << 0)  ///< Binding matches the search string
#define KEY_GATHER_LONGER     (1 << 1)  ///< No bindings match, but longer strings might

typedef uint8_t MenuFuncFlags;          ///< Flags, e.g. #MFF_DEPRECATED
#define MFF_NO_FLAGS               0    ///< No flags are set
#define MFF_DEPRECATED       (1 << 1)   ///< Redraw the pager

/**
 * struct KeyEvent - An event such as a keypress
 */
struct KeyEvent
{
  int ch;    ///< Raw key pressed
  int op;    ///< Function opcode, e.g. OP_HELP
  int count; ///< Optional count prefix, e.g. 3 for `3j`
};
ARRAY_HEAD(KeyEventArray, struct KeyEvent);

extern struct KeyEventArray MacroEvents;
extern struct KeyEventArray UngetKeyEvents;

/**
 * struct KeymapMatch - Result of Matching Keybinding
 *
 * As the user presses keys, we search the MenuDefinition for matching keybindings.
 */
struct KeymapMatch
{
  enum MenuType  mtype;       ///< Menu Type, e.g. #MENU_INDEX
  KeyGatherFlags flags;       ///< Flags, e.g. #KEY_GATHER_MATCH
  struct Keymap *keymap;      ///< Keymap defining `bind` or `macro
};
ARRAY_HEAD(KeymapMatchArray, struct KeymapMatch);

void             array_add                   (struct KeyEventArray *a, int ch, int op);
struct KeyEvent *array_pop                   (struct KeyEventArray *a);
void             array_to_endcond            (struct KeyEventArray *a);
KeyGatherFlags   gather_functions            (const struct MenuDefinition *md, const keycode_t *keys, int key_len, struct KeymapMatchArray *kma);
void             generic_tokenize_push_string(char *s);
struct KeyEvent  km_dokey                    (enum MenuType mtype, GetChFlags flags);
void             km_error_key                (enum MenuType mtype);
void             mutt_flushinp               (void);
void             mutt_flush_macro_to_endcond (void);
struct KeyEvent  mutt_getch                  (GetChFlags flags);
int              mutt_monitor_getch          (void);
void             mutt_push_macro_event       (int ch, int op);
void             mutt_unget_ch               (int ch);
void             mutt_unget_op               (int op);

#endif /* MUTT_KEY_GET_H */
