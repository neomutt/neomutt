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
#include "mutt_curses.h"
#include "keymap.h"
#include "mapping.h"

#include <string.h>

static struct mapping_t Menus[] = {
 { "alias",	MENU_ALIAS },
 { "attach",	MENU_ATTACH },
 { "browser",	MENU_FOLDER },
 { "compose",	MENU_COMPOSE },
 { "editor",	MENU_EDITOR },
 { "generic",	MENU_GENERIC },
 { "index",	MENU_MAIN },
 { "pager",	MENU_PAGER },



#ifdef _PGPPATH
 { "pgp",	MENU_PGP },
#endif /* _PGPPATH */



 { NULL,	0 }
};

#define mutt_check_menu(s) mutt_getvaluebyname(s, Menus)

/* expects to see: <menu-string> <key-string> */
static const char *parse_keymap (int *menu,
				 char *key,
				 size_t keylen,
				 const char *s,
				 char *err,
				 size_t errlen)
{
  char buf[SHORT_STRING];
  char expn[SHORT_STRING];

  /* menu name */
  s = mutt_extract_token (buf, sizeof (buf), s, expn, sizeof (expn), 0);

  if (s)
  {
    if ((*menu = mutt_check_menu (buf)) == -1)
    {
      snprintf (err, errlen, "%s: no such menu", s);
      return (NULL);
    }

    /* key sequence */
    s = mutt_extract_token (key, keylen, s, expn, sizeof (expn), 0);

    if (s)
      return s;
  }

  strfcpy (err, "too few arguments", errlen);
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

/* bind menu-name '<key_sequence>' function-name */
int mutt_parse_bind (const char *s, unsigned long data, char *err, size_t errlen)
{
  struct binding_t *bindings = NULL;
  char buf[SHORT_STRING];
  char key[SHORT_STRING];
  char expn[SHORT_STRING];
  int menu;

  if ((s = parse_keymap (&menu, key, sizeof (key), s, err, errlen)) == NULL)
    return (-1);

  switch (menu)
  {
    case MENU_MAIN:
      bindings = OpMain;
      break;
    case MENU_GENERIC:
      bindings = OpGeneric;
      break;
    case MENU_COMPOSE:
      bindings = OpCompose;
      break;
    case MENU_PAGER:
      bindings = OpPager;
      break;
    case MENU_POST:
      bindings = OpPost;
      break;
    case MENU_FOLDER:
      bindings = OpBrowser;
      break;
    case MENU_ATTACH:
      bindings = OpAttach;
      break;
    case MENU_EDITOR:
      bindings = OpEditor;
      break;
    case MENU_ALIAS:
      bindings = OpAlias;
      break;



#ifdef _PGPPATH
    case MENU_PGP:
      bindings = OpPgp;
      break;
#endif /* _PGPPATH */



  }

  /* function to execute */
  s = mutt_extract_token (buf, sizeof (buf), s, expn, sizeof (expn), 0);
  if (s)
  {
    strfcpy (err, "too many arguments", errlen);
    return (-1);
  }

  if (strcasecmp ("noop", buf) == 0)
  {
    km_bindkey (key, menu, OP_NULL, NULL);
    return 0;
  }

  if (menu != MENU_PAGER && menu != MENU_EDITOR && menu != MENU_GENERIC)
  {
    /* First check the "generic" list of commands.  */
    if (try_bind (key, menu, buf, OpGeneric) == 0)
      return 0;
  }

  /* Now check the menu-specific list of commands (if they exist).  */
  if (bindings && try_bind (key, menu, buf, bindings) == 0)
    return 0;

  snprintf (err, errlen, "%s: no such function in map", buf);
  return (-1);
}

/* macro <menu> <key> <macro> */
int mutt_parse_macro (const char *s, unsigned long data, char *err, size_t errlen)
{
  int menu;
  char key[SHORT_STRING];
  char buf[SHORT_STRING];
  char expn[SHORT_STRING];

  if ((s = parse_keymap (&menu, key, sizeof (key), s, err, errlen)) == NULL)
    return (-1);

  s = mutt_extract_token (buf, sizeof (buf), s, expn, sizeof (expn), M_CONDENSE);
  if (s)
  {
    strfcpy (err, "too many arguments", errlen);
    return (-1);
  }

  km_bindkey (key, menu, OP_MACRO, buf);

  return 0;
}
