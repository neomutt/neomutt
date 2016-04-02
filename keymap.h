/*
 * Copyright (C) 1996-2000,2002,2010 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#ifndef KEYMAP_H
#define KEYMAP_H

#include "mapping.h"

/* maximal length of a key binding sequence used for buffer in km_bindkey */
#define MAX_SEQ 8

/* type for key storage, the rest of mutt works fine with int type */
typedef short keycode_t;

void km_bind (char *, int, int, char *, char *);
void km_bindkey (char *, int, int);
int km_dokey (int);

void init_extended_keys(void);

/* entry in the keymap tree */
struct keymap_t
{
  char *macro;           /* macro expansion (op == OP_MACRO) */
  char *descr;           /* description of a macro for the help menu */
  struct keymap_t *next; /* next key in map */
  short op;              /* operation to perform */
  short eq;              /* number of leading keys equal to next entry */
  short len;             /* length of key sequence (unit: sizeof (keycode_t)) */
  keycode_t *keys;       /* key sequence */
};

int km_expand_key (char *, size_t, struct keymap_t *);
struct keymap_t *km_find_func (int, int);
void km_init (void);
void km_error_key (int);
void mutt_what_key (void);

enum
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
extern struct keymap_t *Keymaps[];

/* dokey() records the last real key pressed  */
extern int LastKey;

extern const struct mapping_t Menus[];

struct binding_t
{
  char *name;	/* name of the function */
  int op;	/* function id number */
  char *seq;	/* default key binding */
};

const struct binding_t *km_get_table (int menu);

extern const struct binding_t OpGeneric[];
extern const struct binding_t OpPost[];
extern const struct binding_t OpMain[];
extern const struct binding_t OpAttach[];
extern const struct binding_t OpPager[];
extern const struct binding_t OpCompose[];
extern const struct binding_t OpBrowser[];
extern const struct binding_t OpEditor[];
extern const struct binding_t OpQuery[];
extern const struct binding_t OpAlias[];

extern const struct binding_t OpPgp[];

extern const struct binding_t OpSmime[];

#ifdef MIXMASTER
extern const struct binding_t OpMix[];
#endif

#include "keymap_defs.h"

#endif /* KEYMAP_H */
