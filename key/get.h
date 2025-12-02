/**
 * @file
 * Get a key from the user
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

#ifndef MUTT_KEY_GET_H
#define MUTT_KEY_GET_H

#include <stdint.h>
#include "mutt/lib.h"
#include "menu/lib.h"
#include "keymap.h"

typedef uint8_t GetChFlags;             ///< Flags for mutt_getch(), e.g. #GETCH_NO_FLAGS
#define GETCH_NO_FLAGS              0   ///< No flags are set
#define GETCH_IGNORE_MACRO    (1 << 0)  ///< Don't use MacroEvents

typedef uint8_t MenuFuncFlags;          ///< Flags, e.g. #MFF_DEPRECATED
#define MFF_NO_FLAGS               0    ///< No flags are set
#define MFF_DEPRECATED       (1 << 1)   ///< Redraw the pager

/**
 * struct KeyEvent - An event such as a keypress
 */
struct KeyEvent
{
  int ch; ///< Raw key pressed
  int op; ///< Function opcode, e.g. OP_HELP
};
ARRAY_HEAD(KeyEventArray, struct KeyEvent);

extern struct KeyEventArray MacroEvents;
extern struct KeyEventArray UngetKeyEvents;

void             array_add                   (struct KeyEventArray *a, int ch, int op);
struct KeyEvent *array_pop                   (struct KeyEventArray *a);
void             array_to_endcond            (struct KeyEventArray *a);
void             generic_tokenize_push_string(char *s);
void             mutt_flushinp               (void);
void             mutt_flush_macro_to_endcond (void);
struct KeyEvent  mutt_getch                  (GetChFlags flags);
void             mutt_push_macro_event       (int ch, int op);
void             mutt_unget_ch               (int ch);
void             mutt_unget_op               (int op);
void             mutt_unget_string           (const char *s);

#endif /* MUTT_KEY_GET_H */
