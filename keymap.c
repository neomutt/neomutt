/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "mapping.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "functions.h"

struct mapping_t Menus[] = {
 { "alias",	MENU_ALIAS },
 { "attach",	MENU_ATTACH },
 { "browser",	MENU_FOLDER },
 { "compose",	MENU_COMPOSE },
 { "editor",	MENU_EDITOR },
 { "generic",	MENU_GENERIC },
 { "index",	MENU_MAIN },
 { "pager",	MENU_PAGER },
 { "postpone",	MENU_POST },

  

#ifdef _PGPPATH
 { "pgp",	MENU_PGP },
#endif  
  
  

 { "query",	MENU_QUERY },
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
  { "<Enter>",	KEY_ENTER },
  { "<Return>",	M_ENTER_C },
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

static int parsekeys (char *s, keycode_t *d, int max)
{
  int n, len = max;

  while (*s && len)
  {
    if ((n = mutt_getvaluebyname (s, KeyNames)) != -1)
    {
      s += strlen (s);
      *d = n;
    }
    else if (tolower (*s) == 'f' && isdigit (s[1]))
    {
      n = 0;
      for (s++; isdigit (*s) ; s++)
      {
	n *= 10;
	n += *s - '0';
      }
      *d = KEY_F(n);
    }
    else
    {
      *d = *s;
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
void km_bindkey (char *s, int menu, int op, char *macro)
{
  struct keymap_t *map, *tmp, *last = NULL, *next;
  keycode_t buf[MAX_SEQ];
  int len, pos = 0, lastpos = 0;

  len = parsekeys (s, buf, MAX_SEQ);

  map = allocKeys (len, buf);
  map->op = op;
  map->macro = safe_strdup (macro);

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
	if (tmp->macro)
	  free (tmp->macro);
	free (tmp->keys);
	free (tmp);
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

static void push_string (char *s)
{
  char *pp, *p = s + strlen (s) - 1;
  size_t l;
  int i;

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
	l = p - pp + 1;
	for (i = 0; KeyNames[i].name; i++)
	{
	  if (!strncasecmp (pp, KeyNames[i].name, l))
	    break;
	}
	if (KeyNames[i].name)
	{
	  /* found a match */
	  mutt_ungetch (KeyNames[i].value);
	  p = pp - 1;
	  continue;
	}
      }
    }
    mutt_ungetch (*p--);
  }
}

static int retry_generic (int menu, keycode_t *keys, int keyslen, int lastkey)
{
  if (menu != MENU_EDITOR && menu != MENU_GENERIC && menu != MENU_PAGER)
  {
    if (lastkey)
      mutt_ungetch (lastkey);
    for (; keyslen; keyslen--)
      mutt_ungetch (keys[keyslen - 1]);
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
 *	-1		error occured while reading input
 */
int km_dokey (int menu)
{
  struct keymap_t *map = Keymaps[menu];
  int pos = 0;
  int n = 0;

  if (!map)
    return (retry_generic (menu, NULL, 0, 0));

  FOREVER
  {
    if ((LastKey = mutt_getch ()) == ERR)
      return (-1);

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
	return (map->op);

      if (n++ == 10)
      {
	mutt_flushinp ();
	mutt_error ("Macro loop detected.");
	return (-1);
      }

      push_string (map->macro);
      map = Keymaps[menu];
      pos = 0;
    }
  }

  /* not reached */
}

static void create_bindings (struct binding_t *map, int menu)
{
  int i;

  for (i = 0 ; map[i].name ; i++)
    if (map[i].seq)
      km_bindkey (map[i].seq, menu, map[i].op, NULL);
}

char *km_keyname (int c)
{
  static char buf[5];
  char *p;

  if ((p = mutt_getnamebyvalue (c, KeyNames)))
    return p;

  switch (c)
  {
    case '\033':
      return "ESC";
    case ' ':
      return "SPC";
    case '\n':
    case '\r':
      return "RET";
    case '\t':
      return "TAB";
  }

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
    sprintf (buf, "F%d", c - KEY_F0);
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
    len -= (1 + (l = strlen (s)));

    if (++p >= map->len || !len)
      return (1);

    s += l;
    *(s++) = ' ';
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



#ifdef _PGPPATH
  create_bindings (OpPgp, MENU_PGP);
#endif


  
  /* bindings for the line editor */
  create_bindings (OpEditor, MENU_EDITOR);
  
  km_bindkey ("<up>", MENU_EDITOR, OP_EDITOR_HISTORY_UP, NULL);
  km_bindkey ("<down>", MENU_EDITOR, OP_EDITOR_HISTORY_DOWN, NULL);
  km_bindkey ("<left>", MENU_EDITOR, OP_EDITOR_BACKWARD_CHAR, NULL);
  km_bindkey ("<right>", MENU_EDITOR, OP_EDITOR_FORWARD_CHAR, NULL);
  km_bindkey ("<home>", MENU_EDITOR, OP_EDITOR_BOL, NULL);
  km_bindkey ("<end>", MENU_EDITOR, OP_EDITOR_EOL, NULL);
  km_bindkey ("<backspace>", MENU_EDITOR, OP_EDITOR_BACKSPACE, NULL);
  km_bindkey ("<delete>", MENU_EDITOR, OP_EDITOR_BACKSPACE, NULL);
  km_bindkey ("\177", MENU_EDITOR, OP_EDITOR_BACKSPACE, NULL);
  
  /* generic menu keymap */
  create_bindings (OpGeneric, MENU_GENERIC);
  
  km_bindkey ("<home>", MENU_GENERIC, OP_FIRST_ENTRY, NULL);
  km_bindkey ("<end>", MENU_GENERIC, OP_LAST_ENTRY, NULL);
  km_bindkey ("<pagedown>", MENU_GENERIC, OP_NEXT_PAGE, NULL);
  km_bindkey ("<pageup>", MENU_GENERIC, OP_PREV_PAGE, NULL);
  km_bindkey ("<right>", MENU_GENERIC, OP_NEXT_PAGE, NULL);
  km_bindkey ("<left>", MENU_GENERIC, OP_PREV_PAGE, NULL);
  km_bindkey ("<up>", MENU_GENERIC, OP_PREV_ENTRY, NULL);
  km_bindkey ("<down>", MENU_GENERIC, OP_NEXT_ENTRY, NULL);
  km_bindkey ("1", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("2", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("3", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("4", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("5", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("6", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("7", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("8", MENU_GENERIC, OP_JUMP, NULL);
  km_bindkey ("9", MENU_GENERIC, OP_JUMP, NULL);

  /* Miscellaneous extra bindings */
  
  km_bindkey (" ", MENU_MAIN, OP_DISPLAY_MESSAGE, NULL);
  km_bindkey ("<up>", MENU_MAIN, OP_MAIN_PREV_UNDELETED, NULL);
  km_bindkey ("<down>", MENU_MAIN, OP_MAIN_NEXT_UNDELETED, NULL);
  km_bindkey ("J", MENU_MAIN, OP_NEXT_ENTRY, NULL);
  km_bindkey ("K", MENU_MAIN, OP_PREV_ENTRY, NULL);
  km_bindkey ("x", MENU_MAIN, OP_EXIT, NULL);

  km_bindkey ("x", MENU_PAGER, OP_PAGER_EXIT, NULL);
  km_bindkey ("q", MENU_PAGER, OP_PAGER_EXIT, NULL);
  km_bindkey ("<backspace>", MENU_PAGER, OP_PREV_LINE, NULL);
  km_bindkey ("<pagedown>", MENU_PAGER, OP_NEXT_PAGE, NULL);
  km_bindkey ("<pageup>", MENU_PAGER, OP_PREV_PAGE, NULL);
  km_bindkey ("<up>", MENU_PAGER, OP_MAIN_PREV_UNDELETED, NULL);
  km_bindkey ("<right>", MENU_PAGER, OP_MAIN_NEXT_UNDELETED, NULL);
  km_bindkey ("<down>", MENU_PAGER, OP_MAIN_NEXT_UNDELETED, NULL);
  km_bindkey ("<left>", MENU_PAGER, OP_MAIN_PREV_UNDELETED, NULL);
  km_bindkey ("<home>", MENU_PAGER, OP_PAGER_TOP, NULL);
  km_bindkey ("<end>", MENU_PAGER, OP_PAGER_BOTTOM, NULL);
  km_bindkey ("1", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("2", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("3", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("4", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("5", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("6", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("7", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("8", MENU_PAGER, OP_JUMP, NULL);
  km_bindkey ("9", MENU_PAGER, OP_JUMP, NULL);

  km_bindkey ("<return>", MENU_ALIAS, OP_TAG, NULL);
}

void km_error_key (int menu)
{
  char buf[SHORT_STRING];

  if (km_expand_key (buf, sizeof (buf), km_find_func (menu, OP_HELP)))
    mutt_error ("Key is not bound.  Press '%s' for help.", buf);
  else
    mutt_error ("Key is not bound.  See the manual.");
}

int mutt_parse_push (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  int r = 0;

  mutt_extract_token (buf, s, M_TOKEN_CONDENSE);
  if (MoreArgs (s))
  {
    strfcpy (err->data, "push: too many arguments", err->dsize);
    r = -1;
  }
  else
    push_string (buf->data);
  return (r);
}

/* expects to see: <menu-string> <key-string> */
char *parse_keymap (int *menu, BUFFER *s, BUFFER *err)
{
  BUFFER buf;

  memset (&buf, 0, sizeof (buf));

  /* menu name */
  mutt_extract_token (&buf, s, 0);
  if (MoreArgs (s))
  {
    if ((*menu = mutt_check_menu (buf.data)) == -1)
    {
      snprintf (err->data, err->dsize, "%s: no such menu", buf.data);
    }
    else
    {
      /* key sequence */
      mutt_extract_token (&buf, s, 0);

      if (!*buf.data)
      {
	strfcpy (err->data, "null key sequence", err->dsize);
      }
      else if (MoreArgs (s))
	return (buf.data);
    }
  }
  else
  {
    strfcpy (err->data, "too few arguments", err->dsize);
  }
  FREE (&buf.data);
  return (NULL);
}

static int
try_bind (char *key, int menu, char *func, struct binding_t *bindings)
{
  int i;
  
  for (i = 0; bindings[i].name; i++)
    if (strcmp (func, bindings[i].name) == 0)
    {
      km_bindkey (key, menu, bindings[i].op, NULL);
      return (0);
    }
  return (-1);
}

struct binding_t *km_get_table (int menu)
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
    case MENU_ATTACH:
      return OpAttach;
    case MENU_EDITOR:
      return OpEditor;
    case MENU_QUERY:
      return OpQuery;



#ifdef _PGPPATH
    case MENU_PGP:
      return OpPgp;
#endif



  }
  return NULL;
}

/* bind menu-name '<key_sequence>' function-name */
int mutt_parse_bind (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  struct binding_t *bindings = NULL;
  char *key;
  int menu, r = 0;

  if ((key = parse_keymap (&menu, s, err)) == NULL)
    return (-1);

  /* function to execute */
  mutt_extract_token (buf, s, 0);
  if (MoreArgs (s))
  {
    strfcpy (err->data, "bind: too many arguments", err->dsize);
    r = -1;
  }
  else if (strcasecmp ("noop", buf->data) == 0)
    km_bindkey (key, menu, OP_NULL, NULL); /* the `unbind' command */
  else
  {
    /* First check the "generic" list of commands */
    if (menu == MENU_PAGER || menu == MENU_EDITOR || menu == MENU_GENERIC ||
	try_bind (key, menu, buf->data, OpGeneric) != 0)
    {
      /* Now check the menu-specific list of commands (if they exist) */
      bindings = km_get_table (menu);
      if (bindings && try_bind (key, menu, buf->data, bindings) != 0)
      {
	snprintf (err->data, err->dsize, "%s: no such function in map", buf->data);
	r = -1;
      }
    }
  }
  FREE (&key);
  return (r);
}

/* macro <menu> <key> <macro> */
int mutt_parse_macro (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  int menu, r = -1;
  char *key;

  if ((key = parse_keymap (&menu, s, err)) == NULL)
    return (-1);

  mutt_extract_token (buf, s, M_TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (!*buf->data)
  {
    strfcpy (err->data, "macro: empty key sequence", err->dsize);
  }
  else if (MoreArgs (s))
  {
    strfcpy (err->data, "macro: too many arguments", err->dsize);
  }
  else
  {
    km_bindkey (key, menu, OP_MACRO, buf->data);
    r = 0;
  }
  FREE (&key);
  return (r);
}
