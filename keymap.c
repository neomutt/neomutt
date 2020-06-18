/**
 * @file
 * Manage keymappings
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2010-2011 Michael R. Elkins <me@mutt.org>
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
 * @page keymap Manage keymappings
 *
 * Manage keymappings
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "keymap.h"
#include "functions.h"
#include "init.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "opcodes.h"
#include "options.h"
#include "ncrypt/lib.h"
#ifndef USE_SLANG_CURSES
#include <strings.h>
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

/**
 * Menus - Menu name lookup table
 */
const struct Mapping Menus[] = {
  { "alias", MENU_ALIAS },
  { "attach", MENU_ATTACH },
  { "browser", MENU_FOLDER },
  { "compose", MENU_COMPOSE },
  { "editor", MENU_EDITOR },
  { "index", MENU_MAIN },
  { "pager", MENU_PAGER },
  { "postpone", MENU_POSTPONE },
  { "pgp", MENU_PGP },
  { "smime", MENU_SMIME },
#ifdef CRYPT_BACKEND_GPGME
  { "key_select_pgp", MENU_KEY_SELECT_PGP },
  { "key_select_smime", MENU_KEY_SELECT_SMIME },
#endif
#ifdef MIXMASTER
  { "mix", MENU_MIX },
#endif
  { "query", MENU_QUERY },
  { "generic", MENU_GENERIC },
  { NULL, 0 },
};

/**
 * KeyNames - Key name lookup table
 */
static struct Mapping KeyNames[] = {
  { "<PageUp>", KEY_PPAGE },
  { "<PageDown>", KEY_NPAGE },
  { "<Up>", KEY_UP },
  { "<Down>", KEY_DOWN },
  { "<Right>", KEY_RIGHT },
  { "<Left>", KEY_LEFT },
  { "<Delete>", KEY_DC },
  { "<BackSpace>", KEY_BACKSPACE },
  { "<Insert>", KEY_IC },
  { "<Home>", KEY_HOME },
  { "<End>", KEY_END },
  { "<Enter>", '\n' },
  { "<Return>", '\r' },
  { "<Esc>", '\033' }, // Escape
  { "<Tab>", '\t' },
  { "<Space>", ' ' },
#ifdef KEY_BTAB
  { "<BackTab>", KEY_BTAB },
#endif
#ifdef KEY_NEXT
  { "<Next>", KEY_NEXT },
#endif
#ifdef NCURSES_VERSION
  /* extensions supported by ncurses.  values are filled in during initialization */

  /* CTRL+key */
  { "<C-Up>", -1 },
  { "<C-Down>", -1 },
  { "<C-Left>", -1 },
  { "<C-Right>", -1 },
  { "<C-Home>", -1 },
  { "<C-End>", -1 },
  { "<C-Next>", -1 },
  { "<C-Prev>", -1 },

  /* SHIFT+key */
  { "<S-Up>", -1 },
  { "<S-Down>", -1 },
  { "<S-Left>", -1 },
  { "<S-Right>", -1 },
  { "<S-Home>", -1 },
  { "<S-End>", -1 },
  { "<S-Next>", -1 },
  { "<S-Prev>", -1 },

  /* ALT+key */
  { "<A-Up>", -1 },
  { "<A-Down>", -1 },
  { "<A-Left>", -1 },
  { "<A-Right>", -1 },
  { "<A-Home>", -1 },
  { "<A-End>", -1 },
  { "<A-Next>", -1 },
  { "<A-Prev>", -1 },
#endif /* NCURSES_VERSION */
  { NULL, 0 },
};

int LastKey;        ///< contains the last key the user pressed
keycode_t AbortKey; ///< code of key to abort prompts, normally Ctrl-G

struct Keymap *Keymaps[MENU_MAX];

#ifdef NCURSES_VERSION
/**
 * struct Extkey - Map key names from NeoMutt's style to Curses style
 */
struct Extkey
{
  const char *name;
  const char *sym;
};

static const struct Extkey ExtKeys[] = {
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

  { 0, 0 },
};
#endif

/**
 * alloc_keys - Allocate space for a sequence of keys
 * @param len  Number of keys
 * @param keys Array of keys
 * @retval ptr Sequence of keys
 */
static struct Keymap *alloc_keys(size_t len, keycode_t *keys)
{
  struct Keymap *p = mutt_mem_calloc(1, sizeof(struct Keymap));
  p->len = len;
  p->keys = mutt_mem_malloc(len * sizeof(keycode_t));
  memcpy(p->keys, keys, len * sizeof(keycode_t));
  return p;
}

/**
 * parse_fkey - Parse a function key string
 * @param s String to parse
 * @retval num Number of the key
 *
 * Given "<f8>", it will return 8.
 */
static int parse_fkey(char *s)
{
  char *t = NULL;
  int n = 0;

  if ((s[0] != '<') || (tolower(s[1]) != 'f'))
    return -1;

  for (t = s + 2; *t && isdigit((unsigned char) *t); t++)
  {
    n *= 10;
    n += *t - '0';
  }

  if (*t != '>')
    return -1;
  return n;
}

/**
 * parse_keycode - Parse a numeric keycode
 * @param s String to parse
 * @retval num Number of the key
 *
 * This function parses the string `<NNN>` and uses the octal value as the key
 * to bind.
 */
static int parse_keycode(const char *s)
{
  char *end_char = NULL;
  long int result = strtol(s + 1, &end_char, 8);
  /* allow trailing whitespace, eg.  < 1001 > */
  while (IS_SPACE(*end_char))
    end_char++;
  /* negative keycodes don't make sense, also detect overflow */
  if ((*end_char != '>') || (result < 0) || (result == LONG_MAX))
  {
    return -1;
  }

  return result;
}

/**
 * parsekeys - Parse a key string into key codes
 * @param str Key string
 * @param d   Array for key codes
 * @param max Maximum length of key sequence
 * @retval num Length of key sequence
 */
static size_t parsekeys(const char *str, keycode_t *d, size_t max)
{
  int n;
  size_t len = max;
  char buf[128];
  char c;
  char *t = NULL;

  mutt_str_copy(buf, str, sizeof(buf));
  char *s = buf;

  while (*s && len)
  {
    *d = '\0';
    if ((*s == '<') && (t = strchr(s, '>')))
    {
      t++;
      c = *t;
      *t = '\0';

      n = mutt_map_get_value(s, KeyNames);
      if (n != -1)
      {
        s = t;
        *d = n;
      }
      else if ((n = parse_fkey(s)) > 0)
      {
        s = t;
        *d = KEY_F(n);
      }
      else if ((n = parse_keycode(s)) > 0)
      {
        s = t;
        *d = n;
      }

      *t = c;
    }

    if (!*d)
    {
      *d = (unsigned char) *s;
      s++;
    }
    d++;
    len--;
  }

  return max - len;
}

/**
 * km_bind_err - Set up a key binding
 * @param s     Key string
 * @param menu  Menu id, e.g. #MENU_EDITOR
 * @param op    Operation, e.g. OP_DELETE
 * @param macro Macro string
 * @param desc Description of macro (OPTIONAL)
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Insert a key sequence into the specified map.
 * The map is sorted by ASCII value (lowest to highest)
 */
static enum CommandResult km_bind_err(const char *s, enum MenuType menu, int op,
                                      char *macro, char *desc, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_SUCCESS;
  struct Keymap *last = NULL, *next = NULL;
  keycode_t buf[MAX_SEQ];
  size_t pos = 0, lastpos = 0;

  size_t len = parsekeys(s, buf, MAX_SEQ);

  struct Keymap *map = alloc_keys(len, buf);
  map->op = op;
  map->macro = mutt_str_dup(macro);
  map->desc = mutt_str_dup(desc);

  struct Keymap *tmp = Keymaps[menu];

  while (tmp)
  {
    if ((pos >= len) || (pos >= tmp->len))
    {
      /* map and tmp match so overwrite */
      do
      {
        /* Don't warn on overwriting a 'noop' binding */
        if ((tmp->len != len) && (tmp->op != OP_NULL))
        {
          /* Overwrite with the different lengths, warn */
          /* TODO: MAX_SEQ here is wrong */
          char old_binding[MAX_SEQ];
          char new_binding[MAX_SEQ];
          km_expand_key(old_binding, MAX_SEQ, map);
          km_expand_key(new_binding, MAX_SEQ, tmp);
          if (err)
          {
            /* err was passed, put the string there */
            snprintf(
                err->data, err->dsize,
                _("Binding '%s' will alias '%s'  Before, try: 'bind %s %s "
                  "noop'  "
                  "https://neomutt.org/guide/configuration.html#bind-warnings"),
                old_binding, new_binding, mutt_map_get_name(menu, Menus), new_binding);
          }
          else
          {
            mutt_error(
                _("Binding '%s' will alias '%s'  Before, try: 'bind %s %s "
                  "noop'  "
                  "https://neomutt.org/guide/configuration.html#bind-warnings"),
                old_binding, new_binding, mutt_map_get_name(menu, Menus), new_binding);
          }
          rc = MUTT_CMD_WARNING;
        }
        len = tmp->eq;
        next = tmp->next;
        FREE(&tmp->macro);
        FREE(&tmp->keys);
        FREE(&tmp->desc);
        FREE(&tmp);
        tmp = next;
      } while (tmp && len >= pos);
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
  {
    Keymaps[menu] = map;
  }

  return rc;
}

/**
 * km_bind - Bind a key to a macro
 * @param s     Key string
 * @param menu  Menu id, e.g. #MENU_EDITOR
 * @param op    Operation, e.g. OP_DELETE
 * @param macro Macro string
 * @param desc Description of macro (OPTIONAL)
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult km_bind(char *s, enum MenuType menu, int op, char *macro, char *desc)
{
  return km_bind_err(s, menu, op, macro, desc, NULL);
}

/**
 * km_bindkey_err - Bind a key in a Menu to an operation (with error message)
 * @param s    Key string
 * @param menu Menu id, e.g. #MENU_PAGER
 * @param op   Operation, e.g. OP_DELETE
 * @param err  Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult km_bindkey_err(const char *s, enum MenuType menu,
                                         int op, struct Buffer *err)
{
  return km_bind_err(s, menu, op, NULL, NULL, err);
}

/**
 * km_bindkey - Bind a key in a Menu to an operation
 * @param s    Key string
 * @param menu Menu id, e.g. #MENU_PAGER
 * @param op   Operation, e.g. OP_DELETE
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult km_bindkey(const char *s, enum MenuType menu, int op)
{
  return km_bindkey_err(s, menu, op, NULL);
}

/**
 * get_op - Get the function by its name
 * @param bindings Key bindings table
 * @param start    Name of function to find
 * @param len      Length of string to match
 * @retval num Operation, e.g. OP_DELETE
 */
static int get_op(const struct Binding *bindings, const char *start, size_t len)
{
  for (int i = 0; bindings[i].name; i++)
  {
    if (mutt_istrn_equal(start, bindings[i].name, len) &&
        (mutt_str_len(bindings[i].name) == len))
    {
      return bindings[i].op;
    }
  }

  return OP_NULL;
}

/**
 * mutt_get_func - Get the name of a function
 * @param bindings Key bindings table
 * @param op       Operation, e.g. OP_DELETE
 * @retval ptr  Name of function
 * @retval NULL Operation not found
 *
 * @note This returns a static string.
 */
const char *mutt_get_func(const struct Binding *bindings, int op)
{
  for (int i = 0; bindings[i].name; i++)
  {
    if (bindings[i].op == op)
      return bindings[i].name;
  }

  return NULL;
}

/**
 * generic_tokenize_push_string - Parse and queue a 'push' command
 * @param s            String to push into the key queue
 * @param generic_push Callback function to add events to macro queue
 *
 * Parses s for `<function>` syntax and adds the whole sequence to either the
 * macro or unget buffer.  This function is invoked by the next two defines
 * below.
 */
static void generic_tokenize_push_string(char *s, void (*generic_push)(int, int))
{
  char *pp = NULL;
  char *p = s + mutt_str_len(s) - 1;
  size_t l;
  int i, op = OP_NULL;

  while (p >= s)
  {
    /* if we see something like "<PageUp>", look to see if it is a real
     * function name and return the corresponding value */
    if (*p == '>')
    {
      for (pp = p - 1; pp >= s && *pp != '<'; pp--)
        ; // do nothing

      if (pp >= s)
      {
        i = parse_fkey(pp);
        if (i > 0)
        {
          generic_push(KEY_F(i), 0);
          p = pp - 1;
          continue;
        }

        l = p - pp + 1;
        for (i = 0; KeyNames[i].name; i++)
        {
          if (mutt_istrn_equal(pp, KeyNames[i].name, l))
            break;
        }
        if (KeyNames[i].name)
        {
          /* found a match */
          generic_push(KeyNames[i].value, 0);
          p = pp - 1;
          continue;
        }

        /* See if it is a valid command
         * skip the '<' and the '>' when comparing */
        for (enum MenuType j = 0; Menus[j].name; j++)
        {
          const struct Binding *binding = km_get_table(Menus[j].value);
          if (binding)
          {
            op = get_op(binding, pp + 1, l - 2);
            if (op != OP_NULL)
              break;
          }
        }

        if (op != OP_NULL)
        {
          generic_push(0, op);
          p = pp - 1;
          continue;
        }
      }
    }
    generic_push((unsigned char) *p--, 0); /* independent 8 bits chars */
  }
}

/**
 * retry_generic - Try to find the key in the generic menu bindings
 * @param menu    Menu id, e.g. #MENU_PAGER
 * @param keys    Array of keys to return to the input queue
 * @param keyslen Number of keys in the array
 * @param lastkey Last key pressed (to return to input queue)
 * @retval num Operation, e.g. OP_DELETE
 */
static int retry_generic(enum MenuType menu, keycode_t *keys, int keyslen, int lastkey)
{
  if ((menu != MENU_EDITOR) && (menu != MENU_GENERIC) && (menu != MENU_PAGER))
  {
    if (lastkey)
      mutt_unget_event(lastkey, 0);
    for (; keyslen; keyslen--)
      mutt_unget_event(keys[keyslen - 1], 0);
    return km_dokey(MENU_GENERIC);
  }
  if (menu != MENU_EDITOR)
  {
    /* probably a good idea to flush input here so we can abort macros */
    mutt_flushinp();
  }
  return OP_NULL;
}

/**
 * km_dokey - Determine what a keypress should do
 * @param menu Menu ID, e.g. #MENU_EDITOR
 * @retval >0      Function to execute
 * @retval OP_NULL No function bound to key sequence
 * @retval -1      Error occurred while reading input
 * @retval -2      A timeout or sigwinch occurred
 */
int km_dokey(enum MenuType menu)
{
  struct KeyEvent tmp;
  struct Keymap *map = Keymaps[menu];
  int pos = 0;
  int n = 0;

  if (!map && (menu != MENU_EDITOR))
    return retry_generic(menu, NULL, 0, 0);

  while (true)
  {
    int i = (C_Timeout > 0) ? C_Timeout : 60;
#ifdef USE_IMAP
    /* keepalive may need to run more frequently than C_Timeout allows */
    if (C_ImapKeepalive)
    {
      if (C_ImapKeepalive >= i)
        imap_keepalive();
      else
      {
        while (C_ImapKeepalive && (C_ImapKeepalive < i))
        {
          mutt_getch_timeout(C_ImapKeepalive * 1000);
          tmp = mutt_getch();
          mutt_getch_timeout(-1);
          /* If a timeout was not received, or the window was resized, exit the
           * loop now.  Otherwise, continue to loop until reaching a total of
           * $timeout seconds.  */
          if ((tmp.ch != -2) || SigWinch)
            goto gotkey;
#ifdef USE_INOTIFY
          if (MonitorFilesChanged)
            goto gotkey;
#endif
          i -= C_ImapKeepalive;
          imap_keepalive();
        }
      }
    }
#endif

    mutt_getch_timeout(i * 1000);
    tmp = mutt_getch();
    mutt_getch_timeout(-1);

#ifdef USE_IMAP
  gotkey:
#endif
    /* hide timeouts, but not window resizes, from the line editor. */
    if ((menu == MENU_EDITOR) && (tmp.ch == -2) && !SigWinch)
      continue;

    LastKey = tmp.ch;
    if (LastKey < 0)
      return LastKey;

    /* do we have an op already? */
    if (tmp.op)
    {
      const char *func = NULL;
      const struct Binding *bindings = NULL;

      /* is this a valid op for this menu? */
      if ((bindings = km_get_table(menu)) && (func = mutt_get_func(bindings, tmp.op)))
        return tmp.op;

      if ((menu == MENU_EDITOR) && mutt_get_func(OpEditor, tmp.op))
        return tmp.op;

      if ((menu != MENU_EDITOR) && (menu != MENU_PAGER))
      {
        /* check generic menu */
        bindings = OpGeneric;
        func = mutt_get_func(bindings, tmp.op);
        if (func)
          return tmp.op;
      }

      /* Sigh. Valid function but not in this context.
       * Find the literal string and push it back */
      for (i = 0; Menus[i].name; i++)
      {
        bindings = km_get_table(Menus[i].value);
        if (bindings)
        {
          func = mutt_get_func(bindings, tmp.op);
          if (func)
          {
            mutt_unget_event('>', 0);
            mutt_unget_string(func);
            mutt_unget_event('<', 0);
            break;
          }
        }
      }
      /* continue to chew */
      if (func)
        continue;
    }

    if (!map)
      return tmp.op;

    /* Nope. Business as usual */
    while (LastKey > map->keys[pos])
    {
      if ((pos > map->eq) || !map->next)
        return retry_generic(menu, map->keys, pos, LastKey);
      map = map->next;
    }

    if (LastKey != map->keys[pos])
      return retry_generic(menu, map->keys, pos, LastKey);

    if (++pos == map->len)
    {
      if (map->op != OP_MACRO)
        return map->op;

      /* OptIgnoreMacroEvents turns off processing the MacroEvents buffer
       * in mutt_getch().  Generating new macro events during that time would
       * result in undesired behavior once the option is turned off.
       *
       * Originally this returned -1, however that results in an unbuffered
       * username or password prompt being aborted.  Returning OP_NULL allows
       * mutt_enter_string_full() to display the keybinding pressed instead.
       *
       * It may be unexpected for a macro's keybinding to be returned,
       * but less so than aborting the prompt.  */
      if (OptIgnoreMacroEvents)
      {
        return OP_NULL;
      }

      if (n++ == 10)
      {
        mutt_flushinp();
        mutt_error(_("Macro loop detected"));
        return -1;
      }

      generic_tokenize_push_string(map->macro, mutt_push_macro_event);
      map = Keymaps[menu];
      pos = 0;
    }
  }

  /* not reached */
}

/**
 * create_bindings - Attach a set of keybindings to a Menu
 * @param map  Key bindings
 * @param menu Menu id, e.g. #MENU_PAGER
 */
static void create_bindings(const struct Binding *map, enum MenuType menu)
{
  for (int i = 0; map[i].name; i++)
    if (map[i].seq)
      km_bindkey(map[i].seq, menu, map[i].op);
}

/**
 * km_keyname - Get the human name for a key
 * @param c Key code
 * @retval ptr Name of the key
 *
 * @note This returns a pointer to a static buffer.
 */
static const char *km_keyname(int c)
{
  static char buf[35];

  const char *p = mutt_map_get_name(c, KeyNames);
  if (p)
    return p;

  if ((c < 256) && (c > -128) && iscntrl((unsigned char) c))
  {
    if (c < 0)
      c += 256;

    if (c < 128)
    {
      buf[0] = '^';
      buf[1] = (c + '@') & 0x7f;
      buf[2] = '\0';
    }
    else
      snprintf(buf, sizeof(buf), "\\%d%d%d", c >> 6, (c >> 3) & 7, c & 7);
  }
  else if ((c >= KEY_F0) && (c < KEY_F(256))) /* this maximum is just a guess */
    sprintf(buf, "<F%d>", c - KEY_F0);
  else if (IsPrint(c))
    snprintf(buf, sizeof(buf), "%c", (unsigned char) c);
  else
    snprintf(buf, sizeof(buf), "\\x%hx", (unsigned short) c);
  return buf;
}

/**
 * mutt_init_abort_key - Parse the abort_key config string
 *
 * Parse the string into C_AbortKey and put the keycode into AbortKey.
 */
void mutt_init_abort_key(void)
{
  keycode_t buf[2];
  size_t len = parsekeys(C_AbortKey, buf, mutt_array_size(buf));
  if (len == 0)
  {
    mutt_error(_("Abort key is not set, defaulting to Ctrl-G"));
    AbortKey = ctrl('G');
    return;
  }
  if (len > 1)
  {
    mutt_warning(
        _("Specified abort key sequence (%s) will be truncated to first key"), C_AbortKey);
  }
  AbortKey = buf[0];
}

/**
 * mutt_abort_key_config_observer - Listen for abort_key config changes - Implements ::observer_t
 */
int mutt_abort_key_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_str_equal(ec->name, "abort_key"))
    return 0;

  mutt_init_abort_key();
  return 0;
}

/**
 * km_expand_key - Get the key string bound to a Keymap
 * @param s   Buffer for the key string
 * @param len Length of buffer
 * @param map Keybinding map
 * @retval 1 Success
 * @retval 0 Error
 */
int km_expand_key(char *s, size_t len, struct Keymap *map)
{
  if (!map)
    return 0;

  int p = 0;

  while (true)
  {
    mutt_str_copy(s, km_keyname(map->keys[p]), len);
    const size_t l = mutt_str_len(s);
    len -= l;

    if ((++p >= map->len) || !len)
      return 1;

    s += l;
  }

  /* not reached */
}

/**
 * km_find_func - Find a function's mapping in a Menu
 * @param menu Menu id, e.g. #MENU_PAGER
 * @param func Function, e.g. OP_DELETE
 * @retval ptr Keymap for the function
 */
struct Keymap *km_find_func(enum MenuType menu, int func)
{
  struct Keymap *map = Keymaps[menu];

  for (; map; map = map->next)
    if (map->op == func)
      break;
  return map;
}

#ifdef NCURSES_VERSION
/**
 * find_ext_name - Find the curses name for a key
 * @param key Key name
 * @retval ptr Curses name
 *
 * Look up NeoMutt's name for a key and find the ncurses extended name for it.
 *
 * @note This returns a static string.
 */
static const char *find_ext_name(const char *key)
{
  for (int j = 0; ExtKeys[j].name; j++)
  {
    if (strcasecmp(key, ExtKeys[j].name) == 0)
      return ExtKeys[j].sym;
  }
  return 0;
}
#endif /* NCURSES_VERSION */

/**
 * init_extended_keys - Initialise map of ncurses extended keys
 *
 * Determine the keycodes for ncurses extended keys and fill in the KeyNames array.
 *
 * This function must be called *after* initscr(), or tigetstr() returns -1.
 * This creates a bit of a chicken-and-egg problem because km_init() is called
 * prior to start_curses().  This means that the default keybindings can't
 * include any of the extended keys because they won't be defined until later.
 */
void init_extended_keys(void)
{
#ifdef NCURSES_VERSION
  use_extended_names(true);

  for (int j = 0; KeyNames[j].name; j++)
  {
    if (KeyNames[j].value == -1)
    {
      const char *keyname = find_ext_name(KeyNames[j].name);

      if (keyname)
      {
        char *s = tigetstr((char *) keyname);
        if (s && ((long) (s) != -1))
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

/**
 * km_init - Initialise all the menu keybindings
 */
void km_init(void)
{
  memset(Keymaps, 0, sizeof(struct Keymap *) * MENU_MAX);

  create_bindings(OpAttach, MENU_ATTACH);
  create_bindings(OpBrowser, MENU_FOLDER);
  create_bindings(OpCompose, MENU_COMPOSE);
  create_bindings(OpMain, MENU_MAIN);
  create_bindings(OpPager, MENU_PAGER);
  create_bindings(OpPost, MENU_POSTPONE);
  create_bindings(OpQuery, MENU_QUERY);
  create_bindings(OpAlias, MENU_ALIAS);

  if (WithCrypto & APPLICATION_PGP)
    create_bindings(OpPgp, MENU_PGP);

  if (WithCrypto & APPLICATION_SMIME)
    create_bindings(OpSmime, MENU_SMIME);

#ifdef CRYPT_BACKEND_GPGME
  create_bindings(OpPgp, MENU_KEY_SELECT_PGP);
  create_bindings(OpSmime, MENU_KEY_SELECT_SMIME);
#endif

#ifdef MIXMASTER
  create_bindings(OpMix, MENU_MIX);

  km_bindkey("<space>", MENU_MIX, OP_GENERIC_SELECT_ENTRY);
  km_bindkey("h", MENU_MIX, OP_MIX_CHAIN_PREV);
  km_bindkey("l", MENU_MIX, OP_MIX_CHAIN_NEXT);
#endif

#ifdef USE_AUTOCRYPT
  create_bindings(OpAutocryptAcct, MENU_AUTOCRYPT_ACCT);
#endif

  /* bindings for the line editor */
  create_bindings(OpEditor, MENU_EDITOR);

  km_bindkey("<up>", MENU_EDITOR, OP_EDITOR_HISTORY_UP);
  km_bindkey("<down>", MENU_EDITOR, OP_EDITOR_HISTORY_DOWN);
  km_bindkey("<left>", MENU_EDITOR, OP_EDITOR_BACKWARD_CHAR);
  km_bindkey("<right>", MENU_EDITOR, OP_EDITOR_FORWARD_CHAR);
  km_bindkey("<home>", MENU_EDITOR, OP_EDITOR_BOL);
  km_bindkey("<end>", MENU_EDITOR, OP_EDITOR_EOL);
  km_bindkey("<backspace>", MENU_EDITOR, OP_EDITOR_BACKSPACE);
  km_bindkey("<delete>", MENU_EDITOR, OP_EDITOR_DELETE_CHAR);
  km_bindkey("\177", MENU_EDITOR, OP_EDITOR_BACKSPACE);

  /* generic menu keymap */
  create_bindings(OpGeneric, MENU_GENERIC);

  km_bindkey("<home>", MENU_GENERIC, OP_FIRST_ENTRY);
  km_bindkey("<end>", MENU_GENERIC, OP_LAST_ENTRY);
  km_bindkey("<pagedown>", MENU_GENERIC, OP_NEXT_PAGE);
  km_bindkey("<pageup>", MENU_GENERIC, OP_PREV_PAGE);
  km_bindkey("<right>", MENU_GENERIC, OP_NEXT_PAGE);
  km_bindkey("<left>", MENU_GENERIC, OP_PREV_PAGE);
  km_bindkey("<up>", MENU_GENERIC, OP_PREV_ENTRY);
  km_bindkey("<down>", MENU_GENERIC, OP_NEXT_ENTRY);
  km_bindkey("1", MENU_GENERIC, OP_JUMP);
  km_bindkey("2", MENU_GENERIC, OP_JUMP);
  km_bindkey("3", MENU_GENERIC, OP_JUMP);
  km_bindkey("4", MENU_GENERIC, OP_JUMP);
  km_bindkey("5", MENU_GENERIC, OP_JUMP);
  km_bindkey("6", MENU_GENERIC, OP_JUMP);
  km_bindkey("7", MENU_GENERIC, OP_JUMP);
  km_bindkey("8", MENU_GENERIC, OP_JUMP);
  km_bindkey("9", MENU_GENERIC, OP_JUMP);

  km_bindkey("<return>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
  km_bindkey("<enter>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);

  /* Miscellaneous extra bindings */

  km_bindkey(" ", MENU_MAIN, OP_DISPLAY_MESSAGE);
  km_bindkey("<up>", MENU_MAIN, OP_MAIN_PREV_UNDELETED);
  km_bindkey("<down>", MENU_MAIN, OP_MAIN_NEXT_UNDELETED);
  km_bindkey("J", MENU_MAIN, OP_NEXT_ENTRY);
  km_bindkey("K", MENU_MAIN, OP_PREV_ENTRY);
  km_bindkey("x", MENU_MAIN, OP_EXIT);

  km_bindkey("<return>", MENU_MAIN, OP_DISPLAY_MESSAGE);
  km_bindkey("<enter>", MENU_MAIN, OP_DISPLAY_MESSAGE);

  km_bindkey("x", MENU_PAGER, OP_EXIT);
  km_bindkey("i", MENU_PAGER, OP_EXIT);
  km_bindkey("<backspace>", MENU_PAGER, OP_PREV_LINE);
  km_bindkey("<pagedown>", MENU_PAGER, OP_NEXT_PAGE);
  km_bindkey("<pageup>", MENU_PAGER, OP_PREV_PAGE);
  km_bindkey("<up>", MENU_PAGER, OP_MAIN_PREV_UNDELETED);
  km_bindkey("<right>", MENU_PAGER, OP_MAIN_NEXT_UNDELETED);
  km_bindkey("<down>", MENU_PAGER, OP_MAIN_NEXT_UNDELETED);
  km_bindkey("<left>", MENU_PAGER, OP_MAIN_PREV_UNDELETED);
  km_bindkey("<home>", MENU_PAGER, OP_PAGER_TOP);
  km_bindkey("<end>", MENU_PAGER, OP_PAGER_BOTTOM);
  km_bindkey("1", MENU_PAGER, OP_JUMP);
  km_bindkey("2", MENU_PAGER, OP_JUMP);
  km_bindkey("3", MENU_PAGER, OP_JUMP);
  km_bindkey("4", MENU_PAGER, OP_JUMP);
  km_bindkey("5", MENU_PAGER, OP_JUMP);
  km_bindkey("6", MENU_PAGER, OP_JUMP);
  km_bindkey("7", MENU_PAGER, OP_JUMP);
  km_bindkey("8", MENU_PAGER, OP_JUMP);
  km_bindkey("9", MENU_PAGER, OP_JUMP);

  km_bindkey("<return>", MENU_PAGER, OP_NEXT_LINE);
  km_bindkey("<enter>", MENU_PAGER, OP_NEXT_LINE);

  km_bindkey("<return>", MENU_ALIAS, OP_GENERIC_SELECT_ENTRY);
  km_bindkey("<enter>", MENU_ALIAS, OP_GENERIC_SELECT_ENTRY);
  km_bindkey("<space>", MENU_ALIAS, OP_TAG);

  km_bindkey("<return>", MENU_ATTACH, OP_VIEW_ATTACH);
  km_bindkey("<enter>", MENU_ATTACH, OP_VIEW_ATTACH);
  km_bindkey("<return>", MENU_COMPOSE, OP_VIEW_ATTACH);
  km_bindkey("<enter>", MENU_COMPOSE, OP_VIEW_ATTACH);

  /* edit-to (default "t") hides generic tag-entry in Compose menu
   * This will bind tag-entry to  "T" in the Compose menu */
  km_bindkey("T", MENU_COMPOSE, OP_TAG);
}

/**
 * km_error_key - Handle an unbound key sequence
 * @param menu Menu id, e.g. #MENU_PAGER
 */
void km_error_key(enum MenuType menu)
{
  char buf[128];
  int p, op;

  struct Keymap *key = km_find_func(menu, OP_HELP);
  if (!key && (menu != MENU_EDITOR) && (menu != MENU_PAGER))
    key = km_find_func(MENU_GENERIC, OP_HELP);
  if (!key)
  {
    mutt_error(_("Key is not bound"));
    return;
  }

  /* Make sure the key is really the help key in this menu.
   *
   * OP_END_COND is used as a barrier to ensure nothing extra
   * is left in the unget buffer.
   *
   * Note that km_expand_key() + tokenize_unget_string() should
   * not be used here: control sequences are expanded to a form
   * (e.g. "^H") not recognized by km_dokey(). */
  mutt_unget_event(0, OP_END_COND);
  p = key->len;
  while (p--)
    mutt_unget_event(key->keys[p], 0);

  /* Note, e.g. for the index menu:
   *   bind generic ?   noop
   *   bind generic ,a  help
   *   bind index   ,ab quit
   * The index keybinding shadows the generic binding.
   * OP_END_COND will be read and returned as the op.
   *
   *   bind generic ?   noop
   *   bind generic dq  help
   *   bind index   d   delete-message
   * OP_DELETE will be returned as the op, leaving "q" + OP_END_COND
   * in the unget buffer.
   */
  op = km_dokey(menu);
  if (op != OP_END_COND)
    mutt_flush_unget_to_endcond();
  if (op != OP_HELP)
  {
    mutt_error(_("Key is not bound"));
    return;
  }

  km_expand_key(buf, sizeof(buf), key);
  mutt_error(_("Key is not bound.  Press '%s' for help."), buf);
}

/**
 * mutt_parse_push - Parse the 'push' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_push(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  mutt_extract_token(buf, s, MUTT_TOKEN_CONDENSE);
  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), "push");
    return MUTT_CMD_ERROR;
  }

  generic_tokenize_push_string(buf->data, mutt_push_macro_event);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_keymap - Parse a user-config key binding
 * @param menu      Array for results
 * @param s         Buffer containing config string
 * @param max_menus Total number of menus
 * @param num_menus Number of menus this config applies to
 * @param err       Buffer for error messages
 * @param bind      If true 'bind', otherwise 'macro'
 * @retval ptr Key string for the binding
 *
 * Expects to see: <menu-string>,<menu-string>,... <key-string>
 *
 * @note Caller needs to free the returned string
 */
static char *parse_keymap(enum MenuType *menu, struct Buffer *s, int max_menus,
                          int *num_menus, struct Buffer *err, bool bind)
{
  struct Buffer buf;
  int i = 0;
  char *q = NULL;

  mutt_buffer_init(&buf);

  /* menu name */
  mutt_extract_token(&buf, s, MUTT_TOKEN_NO_FLAGS);
  char *p = buf.data;
  if (MoreArgs(s))
  {
    while (i < max_menus)
    {
      q = strchr(p, ',');
      if (q)
        *q = '\0';

      int val = mutt_map_get_value(p, Menus);
      if (val == -1)
      {
        mutt_buffer_printf(err, _("%s: no such menu"), p);
        goto error;
      }
      menu[i] = val;
      i++;
      if (q)
        p = q + 1;
      else
        break;
    }
    *num_menus = i;
    /* key sequence */
    mutt_extract_token(&buf, s, MUTT_TOKEN_NO_FLAGS);

    if (buf.data[0] == '\0')
    {
      mutt_buffer_printf(err, _("%s: null key sequence"), bind ? "bind" : "macro");
    }
    else if (MoreArgs(s))
      return buf.data;
  }
  else
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), bind ? "bind" : "macro");
  }
error:
  FREE(&buf.data);
  return NULL;
}

/**
 * try_bind - Try to make a key binding
 * @param key      Key name
 * @param menu     Menu id, e.g. #MENU_PAGER
 * @param func     Function name
 * @param bindings Key bindings table
 * @param err      Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult try_bind(char *key, enum MenuType menu, char *func,
                                   const struct Binding *bindings, struct Buffer *err)
{
  for (int i = 0; bindings[i].name; i++)
  {
    if (mutt_str_equal(func, bindings[i].name))
    {
      return km_bindkey_err(key, menu, bindings[i].op, err);
    }
  }
  if (err)
  {
    mutt_buffer_printf(err, _("Function '%s' not available for menu '%s'"),
                       func, mutt_map_get_name(menu, Menus));
  }
  return MUTT_CMD_ERROR; /* Couldn't find an existing function with this name */
}

/**
 * km_get_table - Lookup a menu's keybindings
 * @param menu Menu id, e.g. #MENU_EDITOR
 * @retval ptr Array of keybindings
 */
const struct Binding *km_get_table(enum MenuType menu)
{
  switch (menu)
  {
    case MENU_ALIAS:
      return OpAlias;
    case MENU_ATTACH:
      return OpAttach;
#ifdef USE_AUTOCRYPT
    case MENU_AUTOCRYPT_ACCT:
      return OpAutocryptAcct;
#endif
    case MENU_COMPOSE:
      return OpCompose;
    case MENU_EDITOR:
      return OpEditor;
    case MENU_FOLDER:
      return OpBrowser;
    case MENU_GENERIC:
      return OpGeneric;
#ifdef CRYPT_BACKEND_GPGME
    case MENU_KEY_SELECT_PGP:
      return OpPgp;
    case MENU_KEY_SELECT_SMIME:
      return OpSmime;
#endif
    case MENU_MAIN:
      return OpMain;
#ifdef MIXMASTER
    case MENU_MIX:
      return OpMix;
#endif
    case MENU_PAGER:
      return OpPager;
    case MENU_PGP:
      return (WithCrypto & APPLICATION_PGP) ? OpPgp : NULL;
    case MENU_POSTPONE:
      return OpPost;
    case MENU_QUERY:
      return OpQuery;
    default:
      return NULL;
  }
}

/**
 * mutt_parse_bind - Parse the 'bind' command - Implements Command::parse()
 *
 * bind menu-name `<key_sequence>` function-name
 */
enum CommandResult mutt_parse_bind(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  const struct Binding *bindings = NULL;
  enum MenuType menu[sizeof(Menus) / sizeof(struct Mapping) - 1];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  char *key = parse_keymap(menu, s, mutt_array_size(menu), &num_menus, err, true);
  if (!key)
    return MUTT_CMD_ERROR;

  /* function to execute */
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), "bind");
    rc = MUTT_CMD_ERROR;
  }
  else if (mutt_istr_equal("noop", buf->data))
  {
    for (int i = 0; i < num_menus; i++)
    {
      km_bindkey(key, menu[i], OP_NULL); /* the 'unbind' command */
    }
  }
  else
  {
    for (int i = 0; i < num_menus; i++)
    {
      /* The pager and editor menus don't use the generic map,
       * however for other menus try generic first. */
      if ((menu[i] != MENU_PAGER) && (menu[i] != MENU_EDITOR) && (menu[i] != MENU_GENERIC))
      {
        rc = try_bind(key, menu[i], buf->data, OpGeneric, err);
        if (rc == 0)
          continue;
        if (rc == -2)
          break;
      }

      /* Clear any error message, we're going to try again */
      err->data[0] = '\0';
      bindings = km_get_table(menu[i]);
      if (bindings)
      {
        rc = try_bind(key, menu[i], buf->data, bindings, err);
      }
    }
  }
  FREE(&key);
  return rc;
}

/**
 * parse_menu - Parse menu-names into an array
 * @param menu     Array for results
 * @param s        String containing menu-names
 * @param err      Buffer for error messages
 *
 * Expects to see: <menu-string>[,<menu-string>]
 */
static void *parse_menu(bool *menu, char *s, struct Buffer *err)
{
  char *menu_names_dup = mutt_str_dup(s);
  char *marker = menu_names_dup;
  char *menu_name = NULL;

  while ((menu_name = strsep(&marker, ",")))
  {
    int value = mutt_map_get_value(menu_name, Menus);
    if (value == -1)
    {
      mutt_buffer_printf(err, _("%s: no such menu"), menu_name);
      break;
    }
    else
      menu[value] = true;
  }

  FREE(&menu_names_dup);
  return NULL;
}

/**
 * km_unbind_all - Free all the keys in the supplied Keymap
 * @param map  Keymap mapping
 * @param mode Undo bind or macro, e.g. #MUTT_UNBIND, #MUTT_UNMACRO
 *
 * Iterate through Keymap and free keys defined either by "macro" or "bind".
 */
static void km_unbind_all(struct Keymap **map, unsigned long mode)
{
  struct Keymap *next = NULL;
  struct Keymap *first = NULL;
  struct Keymap *last = NULL;
  struct Keymap *cur = *map;

  while (cur)
  {
    next = cur->next;
    if (((mode & MUTT_UNBIND) && !cur->macro) || ((mode & MUTT_UNMACRO) && cur->macro))
    {
      FREE(&cur->macro);
      FREE(&cur->keys);
      FREE(&cur->desc);
      FREE(&cur);
    }
    else if (!first)
    {
      first = cur;
      last = cur;
    }
    else if (last)
    {
      last->next = cur;
      last = cur;
    }
    else
    {
      last = cur;
    }
    cur = next;
  }

  if (last)
    last->next = NULL;

  *map = first;
}

/**
 * mutt_parse_unbind - Parse the 'unbind' command - Implements Command::parse()
 *
 * Command unbinds:
 * - one binding in one menu-name
 * - one binding in all menu-names
 * - all bindings in all menu-names
 *
 * unbind `<menu-name[,...]|*>` [`<key_sequence>`]
 */
enum CommandResult mutt_parse_unbind(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  bool menu[MENU_MAX] = { 0 };
  bool all_keys = false;
  char *key = NULL;

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf->data, "*"))
  {
    for (enum MenuType i = 0; i < MENU_MAX; i++)
      menu[i] = true;
  }
  else
    parse_menu(menu, buf->data, err);

  if (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    key = buf->data;
  }
  else
    all_keys = true;

  if (MoreArgs(s))
  {
    const char *cmd = (data & MUTT_UNMACRO) ? "unmacro" : "unbind";

    mutt_buffer_printf(err, _("%s: too many arguments"), cmd);
    return MUTT_CMD_ERROR;
  }

  for (enum MenuType i = 0; i < MENU_MAX; i++)
  {
    if (!menu[i])
      continue;
    if (all_keys)
    {
      km_unbind_all(&Keymaps[i], data);
      km_bindkey("<enter>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
      km_bindkey("<return>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
      km_bindkey("<enter>", MENU_MAIN, OP_DISPLAY_MESSAGE);
      km_bindkey("<return>", MENU_MAIN, OP_DISPLAY_MESSAGE);
      km_bindkey("<backspace>", MENU_EDITOR, OP_EDITOR_BACKSPACE);
      km_bindkey("\177", MENU_EDITOR, OP_EDITOR_BACKSPACE);
      km_bindkey(":", MENU_GENERIC, OP_ENTER_COMMAND);
      km_bindkey(":", MENU_PAGER, OP_ENTER_COMMAND);
      if (i != MENU_EDITOR)
      {
        km_bindkey("?", i, OP_HELP);
        km_bindkey("q", i, OP_EXIT);
      }
    }
    else
      km_bindkey(key, i, OP_NULL);
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_macro - Parse the 'macro' command - Implements Command::parse()
 *
 * macro `<menu>` `<key>` `<macro>` `<description>`
 */
enum CommandResult mutt_parse_macro(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  enum MenuType menu[sizeof(Menus) / sizeof(struct Mapping) - 1];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_ERROR;
  char *seq = NULL;

  char *key = parse_keymap(menu, s, mutt_array_size(menu), &num_menus, err, false);
  if (!key)
    return MUTT_CMD_ERROR;

  mutt_extract_token(buf, s, MUTT_TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (buf->data[0] == '\0')
  {
    mutt_buffer_strcpy(err, _("macro: empty key sequence"));
  }
  else
  {
    if (MoreArgs(s))
    {
      seq = mutt_str_dup(buf->data);
      mutt_extract_token(buf, s, MUTT_TOKEN_CONDENSE);

      if (MoreArgs(s))
      {
        mutt_buffer_printf(err, _("%s: too many arguments"), "macro");
      }
      else
      {
        for (int i = 0; i < num_menus; i++)
        {
          rc = km_bind(key, menu[i], OP_MACRO, seq, buf->data);
        }
      }

      FREE(&seq);
    }
    else
    {
      for (int i = 0; i < num_menus; i++)
      {
        rc = km_bind(key, menu[i], OP_MACRO, buf->data, NULL);
      }
    }
  }
  FREE(&key);
  return rc;
}

/**
 * mutt_parse_exec - Parse the 'exec' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_exec(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  int ops[128];
  int nops = 0;
  const struct Binding *bindings = NULL;
  char *function = NULL;

  if (!MoreArgs(s))
  {
    mutt_buffer_strcpy(err, _("exec: no arguments"));
    return MUTT_CMD_ERROR;
  }

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    function = buf->data;

    bindings = km_get_table(CurrentMenu);
    if (!bindings && (CurrentMenu != MENU_PAGER))
      bindings = OpGeneric;

    ops[nops] = get_op(bindings, function, mutt_str_len(function));
    if ((ops[nops] == OP_NULL) && (CurrentMenu != MENU_PAGER))
      ops[nops] = get_op(OpGeneric, function, mutt_str_len(function));

    if (ops[nops] == OP_NULL)
    {
      mutt_flushinp();
      mutt_error(_("%s: no such function"), function);
      return MUTT_CMD_ERROR;
    }
    nops++;
  } while (MoreArgs(s) && nops < mutt_array_size(ops));

  while (nops)
    mutt_push_macro_event(0, ops[--nops]);

  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_what_key - Ask the user to press a key
 *
 * Displays the octal value back to the user.
 */
void mutt_what_key(void)
{
  int ch;

  mutt_window_mvprintw(MuttMessageWindow, 0, 0, _("Enter keys (%s to abort): "),
                       km_keyname(AbortKey));
  do
  {
    ch = getch();
    if ((ch != ERR) && (ch != AbortKey))
    {
      mutt_message(_("Char = %s, Octal = %o, Decimal = %d"), km_keyname(ch), ch, ch);
    }
  } while (ch != ERR && ch != AbortKey);

  mutt_flushinp();
  mutt_clear_error();
}

/**
 * mutt_keys_free - Free the key maps
 */
void mutt_keys_free(void)
{
  struct Keymap *map = NULL;
  struct Keymap *next = NULL;

  for (int i = 0; i < MENU_MAX; i++)
  {
    for (map = Keymaps[i]; map; map = next)
    {
      next = map->next;

      FREE(&map->macro);
      FREE(&map->desc);
      FREE(&map->keys);
      FREE(&map);
    }

    Keymaps[i] = NULL;
  }
}
