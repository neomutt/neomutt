/**
 * @file
 * Manage keymappings
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_key Key mappings
 *
 * Manage keymappings
 *
 * | File                | Description           |
 * | :------------------ | :-------------------- |
 * | key/dump.c:         | @subpage key_dump     |
 * | key/get.c:          | @subpage key_get      |
 * | key/init.c:         | @subpage key_init     |
 * | key/lib.c:          | @subpage key_lib      |
 * | key/parse.c:        | @subpage key_parse    |
 */

#ifndef MUTT_KEY_LIB_H
#define MUTT_KEY_LIB_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "menu/lib.h"

#define MUTT_UNBIND   (1 << 0)  ///< Parse 'unbind'  command
#define MUTT_UNMACRO  (1 << 1)  ///< Parse 'unmacro' command

typedef uint8_t GetChFlags;            ///< Flags for mutt_getch(), e.g. #GETCH_NO_FLAGS
#define GETCH_NO_FLAGS             0   ///< No flags are set
#define GETCH_IGNORE_MACRO   (1 << 0)  ///< Don't use MacroEvents

/// Type for key storage, the rest of neomutt works fine with int type
typedef short keycode_t;

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
  keycode_t *keys;              ///< key sequence
  STAILQ_ENTRY(Keymap) entries; ///< Linked list
};

STAILQ_HEAD(KeymapList, Keymap);

/**
 * struct KeymapSeq - Array of KeymapList
 */
struct KeymapSeq
{
//  struct Keymaps *name;
  void *name;
};

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

extern struct KeymapList Keymaps[]; ///< Array of Keymap keybindings, one for each Menu
extern struct Mapping KeyNames[];

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
 * struct MenuFuncOps - An array of MenuFuncOp
 */
struct MenuFuncOps
{
  void *name;
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

enum CommandResult mutt_parse_bind   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_exec   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_macro  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_push   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_unbind (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_unmacro(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

enum CommandResult       km_bind                    (char *s, enum MenuType menu, int op, char *macro, char *desc);
int                      km_dokey                   (enum MenuType menu, GetChFlags flags);
struct KeyEvent          km_dokey_event             (enum MenuType menu, GetChFlags flags);
void                     km_error_key               (enum MenuType menu);
int                      km_expand_key              (char *s, size_t len, struct Keymap *map);
int                      km_expand_key_string       (char *str, char *buf, size_t buflen);
struct Keymap *          km_find_func               (enum MenuType menu, int func);
const struct MenuFuncOp *km_get_table               (enum MenuType mtype);
void                     km_init                    (void);
const char *             km_keyname                 (int c);
void                     init_extended_keys         (void);
int                      main_config_observer       (struct NotifyCallback *nc);
void                     mutt_flush_macro_to_endcond(void);
void                     mutt_init_abort_key        (void);
void                     mutt_keys_cleanup          (void);
void                     mw_what_key                (void);

struct KeyEvent km_dokey_event2(void *hsmap[], const void *hsfuncs[], GetChFlags flags);
int km_dokey2(void *hsmap[], const void *hsfuncs[], GetChFlags flags);

// Private to libkey
struct Keymap *    alloc_keys                  (size_t len, keycode_t *keys);
enum CommandResult dump_bind_macro             (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
void               generic_tokenize_push_string(char *s);
int                get_op                      (const struct MenuFuncOp *funcs, const char *start, size_t len);
enum CommandResult km_bindkey                  (const char *s, enum MenuType mtype, int op);
struct Keymap *    km_compare_keys             (struct Keymap *k1, struct Keymap *k2, size_t *pos);
const char *       mutt_get_func               (const struct MenuFuncOp *bindings, int op);
void               mutt_keymap_free            (struct Keymap **ptr);
size_t             parsekeys                   (const char *str, keycode_t *d, size_t max);

#endif /* MUTT_KEY_LIB_H */
