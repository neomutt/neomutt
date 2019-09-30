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
#include "mutt/mutt.h"
#include "mutt_commands.h"

#define MUTT_UNBIND  1<<0
#define MUTT_UNMACRO 1<<1
/* maximal length of a key binding sequence used for buffer in km_bindkey */
#define MAX_SEQ 8

/* type for key storage, the rest of neomutt works fine with int type */
typedef short keycode_t;

void init_extended_keys(void);

/**
 * struct Keymap - A keyboard mapping
 *
 * entry in the keymap tree
 */
struct Keymap
{
  char *macro;         /**< macro expansion (op == OP_MACRO) */
  char *desc;          /**< description of a macro for the help menu */
  struct Keymap *next; /**< next key in map */
  short op;            /**< operation to perform */
  short eq;            /**< number of leading keys equal to next entry */
  short len;           /**< length of key sequence (unit: sizeof (keycode_t)) */
  keycode_t *keys;     /**< key sequence */
};

/**
 * struct KeyEvent - An event such as a keypress
 */
struct KeyEvent
{
  int ch; ///< raw key pressed
  int op; ///< function op
};

/**
 * enum MenuType - Types of GUI selections
 */
enum MenuType
{
  MENU_ALIAS,            ///< Select an email address by its alias
  MENU_ATTACH,           ///< Select an attachment
  MENU_COMPOSE,          ///< Compose an email
  MENU_EDITOR,           ///< Text entry area
  MENU_FOLDER,           ///< General file/mailbox browser
  MENU_GENERIC,          ///< Generic selection list
  MENU_MAIN,             ///< Index panel (list of emails)
  MENU_PAGER,            ///< Pager pager (email viewer)
  MENU_POSTPONE,         ///< Select a postponed email
  MENU_QUERY,            ///< Select from results of external query
  MENU_PGP,              ///< PGP encryption menu
  MENU_SMIME,            ///< SMIME encryption menu
#ifdef CRYPT_BACKEND_GPGME
  MENU_KEY_SELECT_PGP,   ///< Select a PGP key
  MENU_KEY_SELECT_SMIME, ///< Select a SMIME key
#endif
#ifdef MIXMASTER
  MENU_MIX,              ///< Create/edit a Mixmaster chain
#endif
#ifdef USE_AUTOCRYPT
  MENU_AUTOCRYPT_ACCT,
#endif
  MENU_MAX,
};

int km_expand_key(char *s, size_t len, struct Keymap *map);
struct Keymap *km_find_func(enum MenuType menu, int func);
void km_init(void);
void km_error_key(enum MenuType menu);
void mutt_what_key(void);

enum CommandResult km_bind(char *s, enum MenuType menu, int op, char *macro, char *desc);
int km_dokey(enum MenuType menu);

extern struct Keymap *Keymaps[]; ///< Array of Keymap keybindings, one for each Menu

extern int LastKey; ///< Last real key pressed, recorded by dokey()

extern const struct Mapping Menus[];

/**
 * struct Binding - Mapping between a user key and a function
 */
struct Binding
{
  const char *name; /**< name of the function */
  int op;           /**< function id number */
  const char *seq;  /**< default key binding */
};

const struct Binding *km_get_table(enum MenuType menu);
const char *mutt_get_func(const struct Binding *bindings, int op);

extern const struct Binding OpGeneric[];
extern const struct Binding OpPost[];
extern const struct Binding OpMain[];
extern const struct Binding OpAttach[];
extern const struct Binding OpPager[];
extern const struct Binding OpCompose[];
extern const struct Binding OpBrowser[];
extern const struct Binding OpEditor[];
extern const struct Binding OpQuery[];
extern const struct Binding OpAlias[];

extern const struct Binding OpPgp[];

extern const struct Binding OpSmime[];

#ifdef MIXMASTER
extern const struct Binding OpMix[];
#endif

#ifdef USE_AUTOCRYPT
extern const struct Binding OpAutocryptAcct[];
#endif

void mutt_keys_free(void);

enum CommandResult mutt_parse_bind(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_exec(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_macro(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_push(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_unbind(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_unmacro(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);

#endif /* MUTT_KEYMAP_H */
