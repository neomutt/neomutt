/*
 * Copyright (C) 1996-2000,2002,2010-2011 Michael R. Elkins <me@mutt.org>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "mapping.h"
#include "mutt_crypt.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "functions.h"

const struct mapping_t Menus[] = {
 { "alias",	MENU_ALIAS },
 { "attach",	MENU_ATTACH },
 { "browser",	MENU_FOLDER },
 { "compose",	MENU_COMPOSE },
 { "editor",	MENU_EDITOR },
 { "index",	MENU_MAIN },
 { "pager",	MENU_PAGER },
 { "postpone",	MENU_POST },
 { "pgp",	MENU_PGP },
 { "smime",	MENU_SMIME },
#ifdef CRYPT_BACKEND_GPGME
 { "key_select_pgp",	MENU_KEY_SELECT_PGP },
 { "key_select_smime",	MENU_KEY_SELECT_SMIME },
#endif

#ifdef MIXMASTER
  { "mix", 	MENU_MIX },
#endif
  

 { "query",	MENU_QUERY },
 { "generic",	MENU_GENERIC },
 { NULL,	0 }
};

#define mutt_check_menu(s) mutt_getvaluebyname(s, Menus)

static struct mapping_t KeyNames[] = {
  { "<PageUp>",	KEY_PPAGE },
  { "<PageDown>",	KEY_NPAGE },
  { "<Up>",	KEY_UP },
  { "<Down>",	KEY_DOWN },
  { "<Right>",	KEY_RIGHT },
  { "<Left>",	KEY_LEFT },
  { "<Delete>",	KEY_DC },
  { "<BackSpace>",KEY_BACKSPACE },
  { "<Insert>",	KEY_IC },
  { "<Home>",	KEY_HOME },
  { "<End>",	KEY_END },
#ifdef KEY_ENTER
  { "<Enter>",	KEY_ENTER },
#endif
  { "<Return>",	MUTT_ENTER_C },
  { "<Esc>",	'\033' },
  { "<Tab>",	'\t' },
  { "<Space>",	' ' },
#ifdef KEY_BTAB
  { "<BackTab>", KEY_BTAB },
#endif
#ifdef KEY_NEXT
  { "<Next>",    KEY_NEXT },
#endif  
#ifdef NCURSES_VERSION
  /* extensions supported by ncurses.  values are filled in during initialization */

  /* CTRL+key */
  { "<C-Up>",	-1 },
  { "<C-Down>",	-1 },
  { "<C-Left>", -1 },
  { "<C-Right>",	-1 },
  { "<C-Home>",	-1 },
  { "<C-End>",	-1 },
  { "<C-Next>",	-1 },
  { "<C-Prev>",	-1 },

  /* SHIFT+key */
  { "<S-Up>",	-1 },
  { "<S-Down>",	-1 },
  { "<S-Left>", -1 },
  { "<S-Right>",	-1 },
  { "<S-Home>",	-1 },
  { "<S-End>",	-1 },
  { "<S-Next>",	-1 },
  { "<S-Prev>",	-1 },

  /* ALT+key */
  { "<A-Up>",	-1 },
  { "<A-Down>",	-1 },
  { "<A-Left>", -1 },
  { "<A-Right>",	-1 },
  { "<A-Home>",	-1 },
  { "<A-End>",	-1 },
  { "<A-Next>",	-1 },
  { "<A-Prev>",	-1 },
#endif /* NCURSES_VERSION */
  { NULL,	0 }
};

/* contains the last key the user pressed */
int LastKey;

struct keymap_t *Keymaps[MENU_MAX];

static struct keymap_t *allocKeys (int len, keycode_t *keys)
{
  struct keymap_t *p;

  p = safe_calloc (1, sizeof (struct keymap_t));
  p->len = len;
  p->keys = safe_malloc (len * sizeof (keycode_t));
  memcpy (p->keys, keys, len * sizeof (keycode_t));
  return (p);
}

static int parse_fkey(char *s)
{
  char *t;
  int n = 0;

  if(s[0] != '<' || ascii_tolower(s[1]) != 'f')
    return -1;

  for(t = s + 2; *t && isdigit((unsigned char) *t); t++)
  {
    n *= 10;
    n += *t - '0';
  }

  if(*t != '>')
    return -1;
  else
    return n;
}

/*
 * This function parses the string <NNN> and uses the octal value as the key
 * to bind.
 */
static int parse_keycode (const char *s)
{
  char *endChar;
  long int result = strtol(s+1, &endChar, 8);
  /* allow trailing whitespace, eg.  < 1001 > */
  while (ISSPACE(*endChar))
    ++endChar;
  /* negative keycodes don't make sense, also detect overflow */
  if (*endChar != '>' || result < 0 || result == LONG_MAX) {
    return -1;
  }

  return result;
}

static int parsekeys (const char *str, keycode_t *d, int max)
{
  int n, len = max;
  char buff[SHORT_STRING];
  char c;
  char *s, *t;

  strfcpy(buff, str, sizeof(buff));
  s = buff;
  
  while (*s && len)
  {
    *d = '\0';
    if(*s == '<' && (t = strchr(s, '>')))
    {
      t++; c = *t; *t = '\0';
      
      if ((n = mutt_getvaluebyname (s, KeyNames)) != -1)
      {
	s = t;
	*d = n;
      }
      else if ((n = parse_fkey(s)) > 0)
      {
	s = t;
	*d = KEY_F (n);
      }
      else if ((n = parse_keycode(s)) > 0)
      {
	s = t;
	*d = n;
      }
      
      *t = c;
    }

    if(!*d)
    {
      *d = (unsigned char)*s;
      s++;
    }
    d++;
    len--;
  }

  return (max - len);
}

/* insert a key sequence into the specified map.  the map is sorted by ASCII
 * value (lowest to highest)
 */
void km_bind (char *s, int menu, int op, char *macro, char *descr)
{
  struct keymap_t *map, *tmp, *last = NULL, *next;
  keycode_t buf[MAX_SEQ];
  int len, pos = 0, lastpos = 0;

  len = parsekeys (s, buf, MAX_SEQ);

  map = allocKeys (len, buf);
  map->op = op;
  map->macro = safe_strdup (macro);
  map->descr = safe_strdup (descr);

  tmp = Keymaps[menu];

  while (tmp)
  {
    if (pos >= len || pos >= tmp->len)
    {
      /* map and tmp match, but have different lengths, so overwrite */
      do
      {
	len = tmp->eq;
	next = tmp->next;
	FREE (&tmp->macro);
	FREE (&tmp->keys);
	FREE (&tmp->descr);
	FREE (&tmp);
	tmp = next;
      }
      while (tmp && len >= pos);
      map->eq = len;
      break;
    }
    else if (buf[pos] == tmp->keys[pos])
      pos++;
    else if (buf[pos] < tmp->keys[pos])
    {
      /* found location to insert between last and tmp */
      map->eq = pos;
      break;
    }
    else /* buf[pos] > tmp->keys[pos] */
    {
      last = tmp;
      lastpos = pos;
      if (pos > tmp->eq)
	pos = tmp->eq;
      tmp = tmp->next;
    }
  }

  map->next = tmp;
  if (last)
  {
    last->next = map;
    last->eq = lastpos;
  }
  else
    Keymaps[menu] = map;
}

void km_bindkey (char *s, int menu, int op)
{
  km_bind (s, menu, op, NULL, NULL);
}

static int get_op (const struct binding_t *bindings, const char *start, size_t len)
{
  int i;

  for (i = 0; bindings[i].name; i++)
  {
    if (!ascii_strncasecmp (start, bindings[i].name, len) &&   
	mutt_strlen (bindings[i].name) == len)
      return bindings[i].op;
  }

  return OP_NULL;
}

static char *get_func (const struct binding_t *bindings, int op)
{
  int i;

  for (i = 0; bindings[i].name; i++)
  {
    if (bindings[i].op == op)
      return bindings[i].name;
  }

  return NULL;
}

/* Parses s for <function> syntax and adds the whole sequence to
 * either the macro or unget buffer.  This function is invoked by the next
 * two defines below.
 */
static void generic_tokenize_push_string (char *s, void (*generic_push) (int, int))
{
  char *pp, *p = s + mutt_strlen (s) - 1;
  size_t l;
  int i, op = OP_NULL;

  while (p >= s)
  {
    /* if we see something like "<PageUp>", look to see if it is a real
       function name and return the corresponding value */
    if (*p == '>')
    {
      for (pp = p - 1; pp >= s && *pp != '<'; pp--)
	;
      if (pp >= s)
      {
	if ((i = parse_fkey (pp)) > 0)
	{
	  generic_push (KEY_F (i), 0);
	  p = pp - 1;
	  continue;
	}

	l = p - pp + 1;
	for (i = 0; KeyNames[i].name; i++)
	{
	  if (!ascii_strncasecmp (pp, KeyNames[i].name, l))
	    break;
	}
	if (KeyNames[i].name)
	{
	  /* found a match */
	  generic_push (KeyNames[i].value, 0);
	  p = pp - 1;
	  continue;
	}

	/* See if it is a valid command
	 * skip the '<' and the '>' when comparing */
	for (i = 0; Menus[i].name; i++)
	{
	  const struct binding_t *binding = km_get_table (Menus[i].value);
	  if (binding)
	  {
	    op = get_op (binding, pp + 1, l - 2);
	    if (op != OP_NULL)
	      break;
	  }
	}

	if (op != OP_NULL)
	{
	  generic_push (0, op);
	  p = pp - 1;
	  continue;
	}
      }
    }
    generic_push ((unsigned char)*p--, 0);	/* independent 8 bits chars */
  }
}

/* This should be used for macros, push, and exec commands only. */
#define tokenize_push_macro_string(s) generic_tokenize_push_string (s, mutt_push_macro_event)
/* This should be used for other unget operations. */
#define tokenize_unget_string(s) generic_tokenize_push_string (s, mutt_unget_event)

static int retry_generic (int menu, keycode_t *keys, int keyslen, int lastkey)
{
  if (menu != MENU_EDITOR && menu != MENU_GENERIC && menu != MENU_PAGER)
  {
    if (lastkey)
      mutt_unget_event (lastkey, 0);
    for (; keyslen; keyslen--)
      mutt_unget_event (keys[keyslen - 1], 0);
    return (km_dokey (MENU_GENERIC));
  }
  if (menu != MENU_EDITOR)
  {
    /* probably a good idea to flush input here so we can abort macros */
    mutt_flushinp ();
  }
  return OP_NULL;
}

/* return values:
 *	>0		function to execute
 *	OP_NULL		no function bound to key sequence
 *	-1		error occurred while reading input
 */
int km_dokey (int menu)
{
  event_t tmp;
  struct keymap_t *map = Keymaps[menu];
  int pos = 0;
  int n = 0;
  int i;

  if (!map)
    return (retry_generic (menu, NULL, 0, 0));

  FOREVER
  {
    i = Timeout > 0 ? Timeout : 60;
#ifdef USE_IMAP
    /* keepalive may need to run more frequently than Timeout allows */
    if (ImapKeepalive)
    {
      if (ImapKeepalive >= i)
      	imap_keepalive ();
      else
	while (ImapKeepalive && ImapKeepalive < i)
	{
	  timeout (ImapKeepalive * 1000);
	  tmp = mutt_getch ();
	  timeout (-1);
	  /* If a timeout was not received, or the window was resized, exit the
	   * loop now.  Otherwise, continue to loop until reaching a total of
	   * $timeout seconds.
	   */
	  if (tmp.ch != -2 || SigWinch)
	    goto gotkey;
	  i -= ImapKeepalive;
	  imap_keepalive ();
	}
    }
#endif

    timeout (i * 1000);
    tmp = mutt_getch();
    timeout (-1);

    /* hide timeouts from line editor */
    if (menu == MENU_EDITOR && tmp.ch == -2)
      continue;

#ifdef USE_IMAP
  gotkey:
#endif
    LastKey = tmp.ch;
    if (LastKey < 0)
      return -1;

    /* do we have an op already? */
    if (tmp.op)
    {
      char *func = NULL;
      const struct binding_t *bindings;

      /* is this a valid op for this menu? */
      if ((bindings = km_get_table (menu)) &&
	  (func = get_func (bindings, tmp.op)))
	return tmp.op;

      if (menu == MENU_EDITOR && get_func (OpEditor, tmp.op))
	return tmp.op;

      if (menu != MENU_EDITOR && menu != MENU_PAGER)
      {
	/* check generic menu */
	bindings = OpGeneric; 
	if ((func = get_func (bindings, tmp.op)))
	  return tmp.op;
      }

      /* Sigh. Valid function but not in this context.
       * Find the literal string and push it back */
      for (i = 0; Menus[i].name; i++)
      {
	bindings = km_get_table (Menus[i].value);
	if (bindings)
	{
	  func = get_func (bindings, tmp.op);
	  if (func)
	  {
	    mutt_unget_event ('>', 0);
	    mutt_unget_string (func);
	    mutt_unget_event ('<', 0);
	    break;
	  }
	}
      }
      /* continue to chew */
      if (func)
	continue;
    }

    /* Nope. Business as usual */
    while (LastKey > map->keys[pos])
    {
      if (pos > map->eq || !map->next)
	return (retry_generic (menu, map->keys, pos, LastKey));
      map = map->next;
    }

    if (LastKey != map->keys[pos])
      return (retry_generic (menu, map->keys, pos, LastKey));

    if (++pos == map->len)
    {

      if (map->op != OP_MACRO)
	return map->op;

      if (option (OPTIGNOREMACROEVENTS))
      {
	mutt_error _("Macros are currently disabled.");
	return -1;
      }

      if (n++ == 10)
      {
	mutt_flushinp ();
	mutt_error _("Macro loop detected.");
	return -1;
      }

      tokenize_push_macro_string (map->macro);
      map = Keymaps[menu];
      pos = 0;
    }
  }

  /* not reached */
}

static void create_bindings (const struct binding_t *map, int menu)
{
  int i;

  for (i = 0 ; map[i].name ; i++)
    if (map[i].seq)
      km_bindkey (map[i].seq, menu, map[i].op);
}

static const char *km_keyname (int c)
{
  static char buf[10];
  const char *p;

  if ((p = mutt_getnamebyvalue (c, KeyNames)))
    return p;

  if (c < 256 && c > -128 && iscntrl ((unsigned char) c))
  {
    if (c < 0)
      c += 256;

    if (c < 128)
    {
      buf[0] = '^';
      buf[1] = (c + '@') & 0x7f;
      buf[2] = 0;
    }
    else
      snprintf (buf, sizeof (buf), "\\%d%d%d", c >> 6, (c >> 3) & 7, c & 7);
  }
  else if (c >= KEY_F0 && c < KEY_F(256)) /* this maximum is just a guess */
    sprintf (buf, "<F%d>", c - KEY_F0);
  else if (IsPrint (c))
    snprintf (buf, sizeof (buf), "%c", (unsigned char) c);
  else
    snprintf (buf, sizeof (buf), "\\x%hx", (unsigned short) c);
  return (buf);
}

int km_expand_key (char *s, size_t len, struct keymap_t *map)
{
  size_t l;
  int p = 0;

  if (!map)
    return (0);

  FOREVER
  {
    strfcpy (s, km_keyname (map->keys[p]), len);
    len -= (l = mutt_strlen (s));

    if (++p >= map->len || !len)
      return (1);

    s += l;
  }

  /* not reached */
}

struct keymap_t *km_find_func (int menu, int func)
{
  struct keymap_t *map = Keymaps[menu];

  for (; map; map = map->next)
    if (map->op == func)
      break;
  return (map);
}

#ifdef NCURSES_VERSION
struct extkey {
  const char *name;
  const char *sym;
};

static const struct extkey ExtKeys[] = {
  { "<c-up>", "kUP5" },
  { "<s-up>", "kUP" },
  { "<a-up>", "kUP3" },

  { "<s-down>", "kDN" },
  { "<a-down>", "kDN3" },
  { "<c-down>", "kDN5" },

  { "<c-right>", "kRIT5" },
  { "<s-right>", "kRIT" },
  { "<a-right>", "kRIT3" },

  { "<s-left>", "kLFT" },
  { "<a-left>", "kLFT3" },
  { "<c-left>", "kLFT5" },

  { "<s-home>", "kHOM" },
  { "<a-home>", "kHOM3" },
  { "<c-home>", "kHOM5" },

  { "<s-end>", "kEND" },
  { "<a-end>", "kEND3" },
  { "<c-end>", "kEND5" },

  { "<s-next>", "kNXT" },
  { "<a-next>", "kNXT3" },
  { "<c-next>", "kNXT5" },

  { "<s-prev>", "kPRV" },
  { "<a-prev>", "kPRV3" },
  { "<c-prev>", "kPRV5" },

  { 0, 0 }
};

/* Look up Mutt's name for a key and find the ncurses extended name for it */
static const char *find_ext_name(const char *key)
{
  int j;

  for (j = 0; ExtKeys[j].name; ++j)
  {
    if (strcasecmp(key, ExtKeys[j].name) == 0)
      return ExtKeys[j].sym;
  }
  return 0;
}
#endif /* NCURSES_VERSION */

/* Determine the keycodes for ncurses extended keys and fill in the KeyNames array.
 *
 * This function must be called *after* initscr(), or tigetstr() returns -1.  This
 * creates a bit of a chicken-and-egg problem because km_init() is called prior to
 * start_curses().  This means that the default keybindings can't include any of the
 * extended keys because they won't be defined until later.
 */
void init_extended_keys(void)
{
#ifdef NCURSES_VERSION
  int j;

  use_extended_names(TRUE);

  for (j = 0; KeyNames[j].name; ++j)
  {
    if (KeyNames[j].value == -1)
    {
      const char *keyname = find_ext_name(KeyNames[j].name);

      if (keyname)
      {
        char *s = tigetstr((char *)keyname);
	if (s && (long)(s) != -1)
	{
	  int code = key_defined(s);
	  if (code > 0)
	    KeyNames[j].value = code;
	}
      }
    }
  }
#endif
}

void km_init (void)
{
  memset (Keymaps, 0, sizeof (struct keymap_t *) * MENU_MAX);

  create_bindings (OpAttach, MENU_ATTACH);
  create_bindings (OpBrowser, MENU_FOLDER);
  create_bindings (OpCompose, MENU_COMPOSE);
  create_bindings (OpMain, MENU_MAIN);
  create_bindings (OpPager, MENU_PAGER);
  create_bindings (OpPost, MENU_POST);
  create_bindings (OpQuery, MENU_QUERY);
  create_bindings (OpAlias, MENU_ALIAS);


  if ((WithCrypto & APPLICATION_PGP))
    create_bindings (OpPgp, MENU_PGP);

  if ((WithCrypto & APPLICATION_SMIME))
    create_bindings (OpSmime, MENU_SMIME);

#ifdef CRYPT_BACKEND_GPGME
  create_bindings (OpPgp, MENU_KEY_SELECT_PGP);
  create_bindings (OpSmime, MENU_KEY_SELECT_SMIME);
#endif

#ifdef MIXMASTER
  create_bindings (OpMix, MENU_MIX);
  
  km_bindkey ("<space>", MENU_MIX, OP_GENERIC_SELECT_ENTRY);
  km_bindkey ("h", MENU_MIX, OP_MIX_CHAIN_PREV);
  km_bindkey ("l", MENU_MIX, OP_MIX_CHAIN_NEXT);
#endif
  
  /* bindings for the line editor */
  create_bindings (OpEditor, MENU_EDITOR);
  
  km_bindkey ("<up>", MENU_EDITOR, OP_EDITOR_HISTORY_UP);
  km_bindkey ("<down>", MENU_EDITOR, OP_EDITOR_HISTORY_DOWN);
  km_bindkey ("<left>", MENU_EDITOR, OP_EDITOR_BACKWARD_CHAR);
  km_bindkey ("<right>", MENU_EDITOR, OP_EDITOR_FORWARD_CHAR);
  km_bindkey ("<home>", MENU_EDITOR, OP_EDITOR_BOL);
  km_bindkey ("<end>", MENU_EDITOR, OP_EDITOR_EOL);
  km_bindkey ("<backspace>", MENU_EDITOR, OP_EDITOR_BACKSPACE);
  km_bindkey ("<delete>", MENU_EDITOR, OP_EDITOR_BACKSPACE);
  km_bindkey ("\177", MENU_EDITOR, OP_EDITOR_BACKSPACE);
  
  /* generic menu keymap */
  create_bindings (OpGeneric, MENU_GENERIC);
  
  km_bindkey ("<home>", MENU_GENERIC, OP_FIRST_ENTRY);
  km_bindkey ("<end>", MENU_GENERIC, OP_LAST_ENTRY);
  km_bindkey ("<pagedown>", MENU_GENERIC, OP_NEXT_PAGE);
  km_bindkey ("<pageup>", MENU_GENERIC, OP_PREV_PAGE);
  km_bindkey ("<right>", MENU_GENERIC, OP_NEXT_PAGE);
  km_bindkey ("<left>", MENU_GENERIC, OP_PREV_PAGE);
  km_bindkey ("<up>", MENU_GENERIC, OP_PREV_ENTRY);
  km_bindkey ("<down>", MENU_GENERIC, OP_NEXT_ENTRY);
  km_bindkey ("1", MENU_GENERIC, OP_JUMP);
  km_bindkey ("2", MENU_GENERIC, OP_JUMP);
  km_bindkey ("3", MENU_GENERIC, OP_JUMP);
  km_bindkey ("4", MENU_GENERIC, OP_JUMP);
  km_bindkey ("5", MENU_GENERIC, OP_JUMP);
  km_bindkey ("6", MENU_GENERIC, OP_JUMP);
  km_bindkey ("7", MENU_GENERIC, OP_JUMP);
  km_bindkey ("8", MENU_GENERIC, OP_JUMP);
  km_bindkey ("9", MENU_GENERIC, OP_JUMP);

  km_bindkey ("<enter>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);

  /* Miscellaneous extra bindings */
  
  km_bindkey (" ", MENU_MAIN, OP_DISPLAY_MESSAGE);
  km_bindkey ("<up>", MENU_MAIN, OP_MAIN_PREV_UNDELETED);
  km_bindkey ("<down>", MENU_MAIN, OP_MAIN_NEXT_UNDELETED);
  km_bindkey ("J", MENU_MAIN, OP_NEXT_ENTRY);
  km_bindkey ("K", MENU_MAIN, OP_PREV_ENTRY);
  km_bindkey ("x", MENU_MAIN, OP_EXIT);

  km_bindkey ("<enter>", MENU_MAIN, OP_DISPLAY_MESSAGE);

  km_bindkey ("x", MENU_PAGER, OP_EXIT);
  km_bindkey ("i", MENU_PAGER, OP_EXIT);
  km_bindkey ("<backspace>", MENU_PAGER, OP_PREV_LINE);
  km_bindkey ("<pagedown>", MENU_PAGER, OP_NEXT_PAGE);
  km_bindkey ("<pageup>", MENU_PAGER, OP_PREV_PAGE);
  km_bindkey ("<up>", MENU_PAGER, OP_MAIN_PREV_UNDELETED);
  km_bindkey ("<right>", MENU_PAGER, OP_MAIN_NEXT_UNDELETED);
  km_bindkey ("<down>", MENU_PAGER, OP_MAIN_NEXT_UNDELETED);
  km_bindkey ("<left>", MENU_PAGER, OP_MAIN_PREV_UNDELETED);
  km_bindkey ("<home>", MENU_PAGER, OP_PAGER_TOP);
  km_bindkey ("<end>", MENU_PAGER, OP_PAGER_BOTTOM);
  km_bindkey ("1", MENU_PAGER, OP_JUMP);
  km_bindkey ("2", MENU_PAGER, OP_JUMP);
  km_bindkey ("3", MENU_PAGER, OP_JUMP);
  km_bindkey ("4", MENU_PAGER, OP_JUMP);
  km_bindkey ("5", MENU_PAGER, OP_JUMP);
  km_bindkey ("6", MENU_PAGER, OP_JUMP);
  km_bindkey ("7", MENU_PAGER, OP_JUMP);
  km_bindkey ("8", MENU_PAGER, OP_JUMP);
  km_bindkey ("9", MENU_PAGER, OP_JUMP);

  km_bindkey ("<enter>", MENU_PAGER, OP_NEXT_LINE);
  
  km_bindkey ("<return>", MENU_ALIAS, OP_GENERIC_SELECT_ENTRY);
  km_bindkey ("<enter>",  MENU_ALIAS, OP_GENERIC_SELECT_ENTRY);
  km_bindkey ("<space>", MENU_ALIAS, OP_TAG);

  km_bindkey ("<enter>", MENU_ATTACH, OP_VIEW_ATTACH);
  km_bindkey ("<enter>", MENU_COMPOSE, OP_VIEW_ATTACH);

  /* edit-to (default "t") hides generic tag-entry in Compose menu
     This will bind tag-entry to  "T" in the Compose menu */
  km_bindkey ("T", MENU_COMPOSE, OP_TAG);
}

void km_error_key (int menu)
{
  char buf[SHORT_STRING];
  struct keymap_t *key;

  if(!(key = km_find_func(menu, OP_HELP)))
    key = km_find_func(MENU_GENERIC, OP_HELP);
  
  if(!(km_expand_key(buf, sizeof(buf), key)))
  {
    mutt_error _("Key is not bound.");
    return;
  }

  /* make sure the key is really the help key in this menu */
  tokenize_unget_string (buf);
  if (km_dokey (menu) != OP_HELP)
  {
    mutt_error _("Key is not bound.");
    return;
  }

  mutt_error (_("Key is not bound.  Press '%s' for help."), buf);
  return;
}

int mutt_parse_push (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  int r = 0;

  mutt_extract_token (buf, s, MUTT_TOKEN_CONDENSE);
  if (MoreArgs (s))
  {
    strfcpy (err->data, _("push: too many arguments"), err->dsize);
    r = -1;
  }
  else
    tokenize_push_macro_string (buf->data);
  return (r);
}

/* expects to see: <menu-string>,<menu-string>,... <key-string> */
static char *parse_keymap (int *menu, BUFFER *s, int maxmenus, int *nummenus, BUFFER *err)
{
  BUFFER buf;
  int i=0;
  char *p, *q;

  mutt_buffer_init (&buf);

  /* menu name */
  mutt_extract_token (&buf, s, 0);
  p = buf.data;
  if (MoreArgs (s))
  {
    while (i < maxmenus)
    {
      q = strchr(p,',');
      if (q)
        *q = '\0';

      if ((menu[i] = mutt_check_menu (p)) == -1)
      {
         snprintf (err->data, err->dsize, _("%s: no such menu"), p);
         goto error;
      }
      ++i;
      if (q)
        p = q+1;
      else
        break;
    }
    *nummenus=i;
    /* key sequence */
    mutt_extract_token (&buf, s, 0);

    if (!*buf.data)
    {
      strfcpy (err->data, _("null key sequence"), err->dsize);
    }
    else if (MoreArgs (s))
      return (buf.data);
  }
  else
  {
    strfcpy (err->data, _("too few arguments"), err->dsize);
  }
error:
  FREE (&buf.data);
  return (NULL);
}

static int
try_bind (char *key, int menu, char *func, const struct binding_t *bindings)
{
  int i;
  
  for (i = 0; bindings[i].name; i++)
    if (mutt_strcmp (func, bindings[i].name) == 0)
    {
      km_bindkey (key, menu, bindings[i].op);
      return (0);
    }
  return (-1);
}

const struct binding_t *km_get_table (int menu)
{
  switch (menu)
  {
    case MENU_MAIN:
      return OpMain;
    case MENU_GENERIC:
      return OpGeneric;
    case MENU_COMPOSE:
      return OpCompose;
    case MENU_PAGER:
      return OpPager;
    case MENU_POST:
      return OpPost;
    case MENU_FOLDER:
      return OpBrowser;
    case MENU_ALIAS:
      return OpAlias;
    case MENU_ATTACH:
      return OpAttach;
    case MENU_EDITOR:
      return OpEditor;
    case MENU_QUERY:
      return OpQuery;

    case MENU_PGP:
      return (WithCrypto & APPLICATION_PGP)? OpPgp:NULL;

#ifdef CRYPT_BACKEND_GPGME
    case MENU_KEY_SELECT_PGP:
      return OpPgp;
    case MENU_KEY_SELECT_SMIME:
      return OpSmime;
#endif

#ifdef MIXMASTER
    case MENU_MIX:
      return OpMix;
#endif

  }
  return NULL;
}

/* bind menu-name '<key_sequence>' function-name */
int mutt_parse_bind (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  const struct binding_t *bindings = NULL;
  char *key;
  int menu[sizeof(Menus)/sizeof(struct mapping_t)-1], r = 0, nummenus, i;

  if ((key = parse_keymap (menu, s, sizeof (menu)/sizeof (menu[0]),
			   &nummenus, err)) == NULL)
    return (-1);

  /* function to execute */
  mutt_extract_token (buf, s, 0);
  if (MoreArgs (s))
  {
    strfcpy (err->data, _("bind: too many arguments"), err->dsize);
    r = -1;
  }
  else if (ascii_strcasecmp ("noop", buf->data) == 0)
  {
    for (i = 0; i < nummenus; ++i)
    {
      km_bindkey (key, menu[i], OP_NULL); /* the `unbind' command */
    }
  }
  else
  {
    for (i = 0; i < nummenus; ++i)
    {
      /* First check the "generic" list of commands */
      if (menu[i] == MENU_PAGER || menu[i] == MENU_EDITOR ||
      menu[i] == MENU_GENERIC ||
	  try_bind (key, menu[i], buf->data, OpGeneric) != 0)
      {
        /* Now check the menu-specific list of commands (if they exist) */
        bindings = km_get_table (menu[i]);
        if (bindings && try_bind (key, menu[i], buf->data, bindings) != 0)
        {
          snprintf (err->data, err->dsize, _("%s: no such function in map"), buf->data);
          r = -1;
        }
      }
    }
  }
  FREE (&key);
  return (r);
}

/* macro <menu> <key> <macro> <description> */
int mutt_parse_macro (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  int menu[sizeof(Menus)/sizeof(struct mapping_t)-1], r = -1, nummenus, i;
  char *seq = NULL;
  char *key;

  if ((key = parse_keymap (menu, s, sizeof (menu) / sizeof (menu[0]), &nummenus, err)) == NULL)
    return (-1);

  mutt_extract_token (buf, s, MUTT_TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (!*buf->data)
  {
    strfcpy (err->data, _("macro: empty key sequence"), err->dsize);
  }
  else
  {
    if (MoreArgs (s))
    {
      seq = safe_strdup (buf->data);
      mutt_extract_token (buf, s, MUTT_TOKEN_CONDENSE);

      if (MoreArgs (s))
      {
	strfcpy (err->data, _("macro: too many arguments"), err->dsize);
      }
      else
      {
        for (i = 0; i < nummenus; ++i)
        {
          km_bind (key, menu[i], OP_MACRO, seq, buf->data);
          r = 0;
        }
      }

      FREE (&seq);
    }
    else
    {
      for (i = 0; i < nummenus; ++i)
      {
        km_bind (key, menu[i], OP_MACRO, buf->data, NULL);
        r = 0;
      }
    }
  }
  FREE (&key);
  return (r);
}

/* exec function-name */
int mutt_parse_exec (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  int ops[128]; 
  int nops = 0;
  const struct binding_t *bindings = NULL;
  char *function;

  if (!MoreArgs (s))
  {
    strfcpy (err->data, _("exec: no arguments"), err->dsize);
    return (-1);
  }

  do
  {
    mutt_extract_token (buf, s, 0);
    function = buf->data;

    if ((bindings = km_get_table (CurrentMenu)) == NULL 
	&& CurrentMenu != MENU_PAGER)
      bindings = OpGeneric;

    ops[nops] = get_op (bindings, function, mutt_strlen(function));
    if (ops[nops] == OP_NULL && CurrentMenu != MENU_PAGER)
      ops[nops] = get_op (OpGeneric, function, mutt_strlen(function));

    if (ops[nops] == OP_NULL)
    {
      mutt_flushinp ();
      mutt_error (_("%s: no such function"), function);
      return (-1);
    }
    nops++;
  }
  while(MoreArgs(s) && nops < sizeof(ops)/sizeof(ops[0]));

  while(nops)
    mutt_push_macro_event (0, ops[--nops]);

  return 0;
}

/*
 * prompts the user to enter a keystroke, and displays the octal value back
 * to the user.
 */
void mutt_what_key (void)
{
  int ch;

  mutt_window_mvprintw (MuttMessageWindow, 0, 0, _("Enter keys (^G to abort): "));
  do {
    ch = getch();
    if (ch != ERR && ch != ctrl ('G'))
    {
      mutt_message(_("Char = %s, Octal = %o, Decimal = %d"),
	       km_keyname(ch), ch, ch);
    }
  }
  while (ch != ERR && ch != ctrl ('G'));

  mutt_flushinp();
  mutt_clear_error();
}
