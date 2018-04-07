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

#ifndef _MUTT_KEYMAP_H
#define _MUTT_KEYMAP_H

#include <stddef.h>
#include "mutt/mutt.h"

/* maximal length of a key binding sequence used for buffer in km_bindkey */
#define MAX_SEQ 8

/* type for key storage, the rest of neomutt works fine with int type */
typedef short keycode_t;

int km_bind(char *s, int menu, int op, char *macro, char *descr);
int km_dokey(int menu);

void init_extended_keys(void);

/**
 * struct Keymap - A keyboard mapping
 *
 * entry in the keymap tree
 */
struct Keymap
{
  char *macro;         /**< macro expansion (op == OP_MACRO) */
  char *descr;         /**< description of a macro for the help menu */
  struct Keymap *next; /**< next key in map */
  short op;            /**< operation to perform */
  short eq;            /**< number of leading keys equal to next entry */
  short len;           /**< length of key sequence (unit: sizeof (keycode_t)) */
  keycode_t *keys;     /**< key sequence */
};

int km_expand_key(char *s, size_t len, struct Keymap *map);
struct Keymap *km_find_func(int menu, int func);
void km_init(void);
void km_error_key(int menu);
void mutt_what_key(void);

/**
 * enum MenuTypes - Types of GUI selections
 */
enum MenuTypes
{
  MENU_ALIAS,
  MENU_ATTACH,
  MENU_COMPOSE,
  MENU_EDITOR,
  MENU_FOLDER,
  MENU_GENERIC,
  MENU_MAIN,
  MENU_PAGER,
  MENU_POST,
  MENU_QUERY,

  MENU_PGP,
  MENU_SMIME,

#ifdef CRYPT_BACKEND_GPGME
  MENU_KEY_SELECT_PGP,
  MENU_KEY_SELECT_SMIME,
#endif

#ifdef MIXMASTER
  MENU_MIX,
#endif

  MENU_MAX
};

/* the keymap trees (one for each menu) */
extern struct Keymap *Keymaps[];

/* dokey() records the last real key pressed  */
extern int LastKey;

extern const struct Mapping Menus[];

/**
 * struct Binding - Mapping between a user key and a function
 */
struct Binding
{
  char *name; /**< name of the function */
  int op;     /**< function id number */
  char *seq;  /**< default key binding */
};

const struct Binding *km_get_table(int menu);

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

void mutt_free_keys(void);

#endif /* _MUTT_KEYMAP_H */
