/**
 * @file
 * Manage keymappings
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2010 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_KEYMAP_H
#define MUTT_KEYMAP_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "menu/lib.h"

#define MUTT_UNBIND  1<<0
#define MUTT_UNMACRO 1<<1
/* maximal length of a key binding sequence used for buffer in km_bindkey */
#define MAX_SEQ 8

/// Type for key storage, the rest of neomutt works fine with int type
typedef short keycode_t;

void init_extended_keys(void);

/**
 * struct Keymap - A keyboard mapping
 *
 * entry in the keymap tree
 */
struct Keymap
{
  char *macro;                  ///< macro expansion (op == OP_MACRO)
  char *desc;                   ///< description of a macro for the help menu
  short op;                     ///< operation to perform
  short eq;                     ///< number of leading keys equal to next entry
  short len;                    ///< length of key sequence (unit: sizeof (keycode_t))
  keycode_t *keys;              ///< key sequence
  STAILQ_ENTRY(Keymap) entries; ///< next key in map
};

STAILQ_HEAD(KeymapList, Keymap);

/**
 * struct KeyEvent - An event such as a keypress
 */
struct KeyEvent
{
  int ch; ///< Raw key pressed
  int op; ///< Function opcode, e.g. OP_HELP
};

int km_expand_key(char *s, size_t len, struct Keymap *map);
struct Keymap *km_find_func(enum MenuType menu, int func);
void km_init(void);
void km_error_key(enum MenuType menu);
void mutt_what_key(void);
void mutt_init_abort_key(void);
int main_config_observer(struct NotifyCallback *nc);

enum CommandResult km_bind(char *s, enum MenuType menu, int op, char *macro, char *desc);
int km_dokey(enum MenuType menu);
struct KeyEvent km_dokey_event(enum MenuType menu);

extern struct KeymapList Keymaps[]; ///< Array of Keymap keybindings, one for each Menu

extern keycode_t AbortKey; ///< key to abort edits etc, normally Ctrl-G

extern const struct Mapping Menus[];

/**
 * struct MenuFuncOp - Mapping between a function and an operation
 */
struct MenuFuncOp
{
  const char *name; ///< Name of the function
  int op;           ///< Operation, e.g. OP_DELETE
};

/**
 * struct MenuOpSeq - Mapping between an operation and a key sequence
 */
struct MenuOpSeq
{
  int op;           ///< Operation, e.g. OP_DELETE
  const char *seq;  ///< Default key binding
};

/**
 * struct EventBinding - A key binding Event
 */
struct EventBinding
{
  enum MenuType menu; ///< Menu, e.g. #MENU_PAGER
  const char *key;    ///< Key string being bound (for new bind/macro)
  int op;             ///< Operation the key's bound to (for bind), e.g. OP_DELETE
};

/**
 * enum NotifyBinding - Key Binding notification types
 *
 * Observers of #NT_BINDING will be passed an #EventBinding.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifyBinding
{
  NT_BINDING_ADD = 1,    ///< Key binding has been added
  NT_BINDING_DELETE,     ///< Key binding has been deleted
  NT_BINDING_DELETE_ALL, ///< All key bindings have been deleted

  NT_MACRO_ADD,          ///< Key macro has been added
  NT_MACRO_DELETE,       ///< Key macro has been deleted
  NT_MACRO_DELETE_ALL,   ///< All key macros have been deleted
};

const struct MenuFuncOp *km_get_table(enum MenuType mtype);
const char *mutt_get_func(const struct MenuFuncOp *bindings, int op);

void mutt_keys_free(void);

enum CommandResult mutt_parse_bind   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_exec   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_macro  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_push   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_unbind (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_unmacro(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

#endif /* MUTT_KEYMAP_H */
