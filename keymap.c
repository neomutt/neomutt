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
 * @page neo_keymap Manage keymappings
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
#include <strings.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "keymap.h"
#include "color/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "functions.h"
#include "globals.h"
#include "mutt_logging.h"
#include "opcodes.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

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
#ifdef KEY_ENTER
  { "<KeypadEnter>", KEY_ENTER },
#else
  { "<KeypadEnter>", '\n' },
#endif
  { "<Esc>", '\033' }, // Escape
  { "<Tab>", '\t' },
  { "<Space>", ' ' },
#ifdef KEY_BTAB
  { "<BackTab>", KEY_BTAB },
#endif
#ifdef KEY_NEXT
  { "<Next>", KEY_NEXT },
#endif
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
  { NULL, 0 },
};

keycode_t AbortKey; ///< code of key to abort prompts, normally Ctrl-G

/// Array of key mappings, one for each #MenuType
struct KeymapList Keymaps[MENU_MAX];

/**
 * struct Extkey - Map key names from NeoMutt's style to Curses style
 */
struct Extkey
{
  const char *name; ///< NeoMutt key name
  const char *sym;  ///< Curses key name
};

/**
 * ExtKeys - Mapping between NeoMutt and Curses key names
 */
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

/**
 * mutt_keymap_free - Free a Keymap
 * @param ptr Keymap to free
 */
static void mutt_keymap_free(struct Keymap **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Keymap *km = *ptr;
  FREE(&km->macro);
  FREE(&km->desc);
  FREE(&km->keys);

  FREE(ptr);
}

/**
 * mutt_keymaplist_free - Free a List of Keymaps
 * @param km_list List of Keymaps to free
 */
static void mutt_keymaplist_free(struct KeymapList *km_list)
{
  struct Keymap *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, km_list, entries, tmp)
  {
    STAILQ_REMOVE(km_list, np, Keymap, entries);
    mutt_keymap_free(&np);
  }
}

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
  p->keys = mutt_mem_calloc(len, sizeof(keycode_t));
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
  while (isspace(*end_char))
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
  char buf[128] = { 0 };
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
 * km_compare_keys - Compare two keymaps' keyscodes and return the bigger one
 * @param k1    first keymap to compare
 * @param k2    second keymap to compare
 * @param pos   position where the two keycodes differ
 * @retval ptr Keymap with a bigger ASCII keycode
 */
static struct Keymap *km_compare_keys(struct Keymap *k1, struct Keymap *k2, size_t *pos)
{
  *pos = 0;

  while (*pos < k1->len && *pos < k2->len)
  {
    if (k1->keys[*pos] < k2->keys[*pos])
      return k2;
    else if (k1->keys[*pos] > k2->keys[*pos])
      return k1;
    else
      *pos = *pos + 1;
  }

  return NULL;
}

/**
 * km_bind_err - Set up a key binding
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @param op    Operation, e.g. OP_DELETE
 * @param macro Macro string
 * @param desc Description of macro (OPTIONAL)
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Insert a key sequence into the specified map.
 * The map is sorted by ASCII value (lowest to highest)
 */
static enum CommandResult km_bind_err(const char *s, enum MenuType mtype, int op,
                                      char *macro, char *desc, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_SUCCESS;
  struct Keymap *last = NULL, *np = NULL, *compare = NULL;
  keycode_t buf[MAX_SEQ];
  size_t pos = 0, lastpos = 0;

  size_t len = parsekeys(s, buf, MAX_SEQ);

  struct Keymap *map = alloc_keys(len, buf);
  map->op = op;
  map->macro = mutt_str_dup(macro);
  map->desc = mutt_str_dup(desc);

  /* find position to place new keymap */
  STAILQ_FOREACH(np, &Keymaps[mtype], entries)
  {
    compare = km_compare_keys(map, np, &pos);

    if (compare == map) /* map's keycode is bigger */
    {
      last = np;
      lastpos = pos;
      if (pos > np->eq)
        pos = np->eq;
    }
    else if (compare == np) /* np's keycode is bigger, found insert location */
    {
      map->eq = pos;
      break;
    }
    else /* equal keycodes */
    {
      /* Don't warn on overwriting a 'noop' binding */
      if ((np->len != len) && (np->op != OP_NULL))
      {
        static const char *guide_link = "https://neomutt.org/guide/configuration.html#bind-warnings";
        /* Overwrite with the different lengths, warn */
        char old_binding[128] = { 0 };
        char new_binding[128] = { 0 };
        km_expand_key(old_binding, sizeof(old_binding), map);
        km_expand_key(new_binding, sizeof(new_binding), np);
        char *err_msg = _("Binding '%s' will alias '%s'  Before, try: 'bind %s %s noop'");
        if (err)
        {
          /* err was passed, put the string there */
          buf_printf(err, err_msg, old_binding, new_binding,
                     mutt_map_get_name(mtype, MenuNames), new_binding);
          buf_add_printf(err, "  %s", guide_link);
        }
        else
        {
          struct Buffer *tmp = buf_pool_get();
          buf_printf(tmp, err_msg, old_binding, new_binding,
                     mutt_map_get_name(mtype, MenuNames), new_binding);
          buf_add_printf(tmp, "  %s", guide_link);
          mutt_error("%s", buf_string(tmp));
          buf_pool_release(&tmp);
        }
        rc = MUTT_CMD_WARNING;
      }

      map->eq = np->eq;
      STAILQ_REMOVE(&Keymaps[mtype], np, Keymap, entries);
      mutt_keymap_free(&np);
      break;
    }
  }

  if (map->op == OP_NULL)
  {
    mutt_keymap_free(&map);
  }
  else
  {
    if (last) /* if queue has at least one entry */
    {
      if (STAILQ_NEXT(last, entries))
        STAILQ_INSERT_AFTER(&Keymaps[mtype], last, map, entries);
      else /* last entry in the queue */
        STAILQ_INSERT_TAIL(&Keymaps[mtype], map, entries);
      last->eq = lastpos;
    }
    else /* queue is empty, so insert from head */
    {
      STAILQ_INSERT_HEAD(&Keymaps[mtype], map, entries);
    }
  }

  return rc;
}

/**
 * km_bind - Bind a key to a macro
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @param op    Operation, e.g. OP_DELETE
 * @param macro Macro string
 * @param desc Description of macro (OPTIONAL)
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult km_bind(char *s, enum MenuType mtype, int op, char *macro, char *desc)
{
  return km_bind_err(s, mtype, op, macro, desc, NULL);
}

/**
 * km_bindkey_err - Bind a key in a Menu to an operation (with error message)
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param op    Operation, e.g. OP_DELETE
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult km_bindkey_err(const char *s, enum MenuType mtype,
                                         int op, struct Buffer *err)
{
  return km_bind_err(s, mtype, op, NULL, NULL, err);
}

/**
 * km_bindkey - Bind a key in a Menu to an operation
 * @param s     Key string
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param op    Operation, e.g. OP_DELETE
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult km_bindkey(const char *s, enum MenuType mtype, int op)
{
  return km_bindkey_err(s, mtype, op, NULL);
}

/**
 * get_op - Get the function by its name
 * @param funcs Functions table
 * @param start Name of function to find
 * @param len   Length of string to match
 * @retval num Operation, e.g. OP_DELETE
 */
static int get_op(const struct MenuFuncOp *funcs, const char *start, size_t len)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (mutt_istrn_equal(start, funcs[i].name, len) && (mutt_str_len(funcs[i].name) == len))
    {
      return funcs[i].op;
    }
  }

  return OP_NULL;
}

/**
 * mutt_get_func - Get the name of a function
 * @param funcs Functions table
 * @param op    Operation, e.g. OP_DELETE
 * @retval ptr  Name of function
 * @retval NULL Operation not found
 *
 * @note This returns a static string.
 */
const char *mutt_get_func(const struct MenuFuncOp *funcs, int op)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (funcs[i].op == op)
      return funcs[i].name;
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
        for (enum MenuType j = 0; MenuNames[j].name; j++)
        {
          const struct MenuFuncOp *funcs = km_get_table(MenuNames[j].value);
          if (funcs)
          {
            op = get_op(funcs, pp + 1, l - 2);
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
 * @param mtype   Menu type, e.g. #MENU_PAGER
 * @param keys    Array of keys to return to the input queue
 * @param keyslen Number of keys in the array
 * @param lastkey Last key pressed (to return to input queue)
 * @retval num Operation, e.g. OP_DELETE
 */
static struct KeyEvent retry_generic(enum MenuType mtype, keycode_t *keys,
                                     int keyslen, int lastkey)
{
  if (lastkey)
    mutt_unget_ch(lastkey);
  for (; keyslen; keyslen--)
    mutt_unget_ch(keys[keyslen - 1]);

  if ((mtype != MENU_EDITOR) && (mtype != MENU_GENERIC) && (mtype != MENU_PAGER))
  {
    return km_dokey_event(MENU_GENERIC);
  }
  if ((mtype != MENU_EDITOR) && (mtype != MENU_GENERIC))
  {
    /* probably a good idea to flush input here so we can abort macros */
    mutt_flushinp();
  }

  return (struct KeyEvent){ mutt_getch().ch, OP_NULL };
}

/**
 * km_dokey_event - Determine what a keypress should do
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @retval ptr Event
 */
struct KeyEvent km_dokey_event(enum MenuType mtype)
{
  struct KeyEvent event = { 0, OP_NULL };
  struct Keymap *map = STAILQ_FIRST(&Keymaps[mtype]);
  int pos = 0;
  int n = 0;

  if (!map && (mtype != MENU_EDITOR))
    return retry_generic(mtype, NULL, 0, 0);

#ifdef USE_IMAP
  const short c_imap_keep_alive = cs_subset_number(NeoMutt->sub, "imap_keep_alive");
#endif

  const short c_timeout = cs_subset_number(NeoMutt->sub, "timeout");
  while (true)
  {
    int i = (c_timeout > 0) ? c_timeout : 60;
#ifdef USE_IMAP
    /* keep_alive may need to run more frequently than `$timeout` allows */
    if (c_imap_keep_alive != 0)
    {
      if (c_imap_keep_alive >= i)
      {
        imap_keep_alive();
      }
      else
      {
        while (c_imap_keep_alive < i)
        {
          event = mutt_getch_timeout(c_imap_keep_alive * 1000);
          /* If a timeout was not received, or the window was resized, exit the
           * loop now.  Otherwise, continue to loop until reaching a total of
           * $timeout seconds.  */
          if ((event.op != OP_TIMEOUT) || SigWinch)
            goto gotkey;
#ifdef USE_INOTIFY
          if (MonitorFilesChanged)
            goto gotkey;
#endif
          i -= c_imap_keep_alive;
          imap_keep_alive();
        }
      }
    }
#endif

    event = mutt_getch_timeout(i * 1000);

#ifdef USE_IMAP
  gotkey:
#endif
    /* hide timeouts, but not window resizes, from the line editor. */
    if ((mtype == MENU_EDITOR) && (event.op == OP_TIMEOUT) && !SigWinch)
      continue;

    if ((event.op == OP_TIMEOUT) || (event.op == OP_ABORT))
    {
      return event;
    }

    /* do we have an op already? */
    if (event.op != OP_NULL)
    {
      const char *func = NULL;
      const struct MenuFuncOp *funcs = NULL;

      /* is this a valid op for this menu type? */
      if ((funcs = km_get_table(mtype)) && (func = mutt_get_func(funcs, event.op)))
        return event;

      if ((mtype != MENU_EDITOR) && (mtype != MENU_PAGER) && (mtype != MENU_GENERIC))
      {
        /* check generic menu type */
        funcs = OpGeneric;
        func = mutt_get_func(funcs, event.op);
        if (func)
          return event;
      }

      /* Sigh. Valid function but not in this context.
       * Find the literal string and push it back */
      for (i = 0; MenuNames[i].name; i++)
      {
        funcs = km_get_table(MenuNames[i].value);
        if (funcs)
        {
          func = mutt_get_func(funcs, event.op);
          if (func)
          {
            mutt_unget_ch('>');
            mutt_unget_string(func);
            mutt_unget_ch('<');
            break;
          }
        }
      }
      /* continue to chew */
      if (func)
        continue;
    }

    if (!map)
      return event;

    /* Nope. Business as usual */
    while (event.ch > map->keys[pos])
    {
      if ((pos > map->eq) || !STAILQ_NEXT(map, entries))
        return retry_generic(mtype, map->keys, pos, event.ch);
      map = STAILQ_NEXT(map, entries);
    }

    if (event.ch != map->keys[pos])
      return retry_generic(mtype, map->keys, pos, event.ch);

    if (++pos == map->len)
    {
      if (map->op != OP_MACRO)
        return (struct KeyEvent){ event.ch, map->op };

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
        return (struct KeyEvent){ event.ch, OP_NULL };
      }

      if (n++ == 10)
      {
        mutt_flushinp();
        mutt_error(_("Macro loop detected"));
        return (struct KeyEvent){ '\0', OP_ABORT };
      }

      generic_tokenize_push_string(map->macro, mutt_push_macro_event);
      map = STAILQ_FIRST(&Keymaps[mtype]);
      pos = 0;
    }
  }

  /* not reached */
}

/**
 * km_dokey - Determine what a keypress should do
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @retval >0      Function to execute
 * @retval OP_NULL No function bound to key sequence
 * @retval -1      Error occurred while reading input
 * @retval -2      A timeout or sigwinch occurred
 */
int km_dokey(enum MenuType mtype)
{
  return km_dokey_event(mtype).op;
}

/**
 * create_bindings - Attach a set of keybindings to a Menu
 * @param map   Key bindings
 * @param mtype Menu type, e.g. #MENU_PAGER
 */
static void create_bindings(const struct MenuOpSeq *map, enum MenuType mtype)
{
  STAILQ_INIT(&Keymaps[mtype]);

  for (int i = 0; map[i].op != OP_NULL; i++)
    if (map[i].seq)
      km_bindkey(map[i].seq, mtype, map[i].op);
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
    {
      snprintf(buf, sizeof(buf), "\\%d%d%d", c >> 6, (c >> 3) & 7, c & 7);
    }
  }
  else if ((c >= KEY_F0) && (c < KEY_F(256))) /* this maximum is just a guess */
  {
    snprintf(buf, sizeof(buf), "<F%d>", c - KEY_F0);
  }
  else if (IsPrint(c))
  {
    snprintf(buf, sizeof(buf), "%c", (unsigned char) c);
  }
  else
  {
    snprintf(buf, sizeof(buf), "\\x%hx", (unsigned short) c);
  }
  return buf;
}

/**
 * mutt_init_abort_key - Parse the abort_key config string
 *
 * Parse the string into `$abort_key` and put the keycode into AbortKey.
 */
void mutt_init_abort_key(void)
{
  keycode_t buf[2];
  const char *const c_abort_key = cs_subset_string(NeoMutt->sub, "abort_key");
  size_t len = parsekeys(c_abort_key, buf, mutt_array_size(buf));
  if (len == 0)
  {
    mutt_error(_("Abort key is not set, defaulting to Ctrl-G"));
    AbortKey = ctrl('G');
    return;
  }
  if (len > 1)
  {
    mutt_warning(_("Specified abort key sequence (%s) will be truncated to first key"),
                 c_abort_key);
  }
  AbortKey = buf[0];
}

/**
 * main_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
int main_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "abort_key"))
    return 0;

  mutt_init_abort_key();
  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * km_expand_key_string - Get a human-readable key string
 * @param str    Raw key string
 * @param buf    Buffer for the key string
 * @param buflen Length of buffer
 * @retval num Length of string
 */
static int km_expand_key_string(char *str, char *buf, size_t buflen)
{
  size_t len = 0;
  for (; *str; str++)
  {
    const char *key = km_keyname(*str);
    size_t keylen = mutt_str_len(key);

    mutt_str_copy(buf, key, buflen);
    buf += keylen;
    buflen -= keylen;
    len += keylen;
  }

  return len;
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
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param func  Function, e.g. OP_DELETE
 * @retval ptr Keymap for the function
 */
struct Keymap *km_find_func(enum MenuType mtype, int func)
{
  struct Keymap *np = NULL;
  STAILQ_FOREACH(np, &Keymaps[mtype], entries)
  {
    if (np->op == func)
      break;
  }
  return np;
}

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
#ifdef HAVE_USE_EXTENDED_NAMES
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
  memset(Keymaps, 0, sizeof(struct KeymapList) * MENU_MAX);

  create_bindings(AliasDefaultBindings, MENU_ALIAS);
  create_bindings(AttachDefaultBindings, MENU_ATTACH);
#ifdef USE_AUTOCRYPT
  create_bindings(AutocryptAcctDefaultBindings, MENU_AUTOCRYPT_ACCT);
#endif
  create_bindings(BrowserDefaultBindings, MENU_FOLDER);
  create_bindings(DialogDefaultBindings, MENU_DIALOG);
  create_bindings(ComposeDefaultBindings, MENU_COMPOSE);
  create_bindings(EditorDefaultBindings, MENU_EDITOR);
  create_bindings(GenericDefaultBindings, MENU_GENERIC);
  create_bindings(IndexDefaultBindings, MENU_INDEX);
#ifdef MIXMASTER
  create_bindings(MixDefaultBindings, MENU_MIX);
#endif
  create_bindings(PagerDefaultBindings, MENU_PAGER);
  create_bindings(PostDefaultBindings, MENU_POSTPONE);
  create_bindings(QueryDefaultBindings, MENU_QUERY);

  if (WithCrypto & APPLICATION_PGP)
    create_bindings(PgpDefaultBindings, MENU_PGP);
  if (WithCrypto & APPLICATION_SMIME)
    create_bindings(SmimeDefaultBindings, MENU_SMIME);

#ifdef CRYPT_BACKEND_GPGME
  create_bindings(PgpDefaultBindings, MENU_KEY_SELECT_PGP);
  create_bindings(SmimeDefaultBindings, MENU_KEY_SELECT_SMIME);
#endif
}

/**
 * km_error_key - Handle an unbound key sequence
 * @param mtype Menu type, e.g. #MENU_PAGER
 */
void km_error_key(enum MenuType mtype)
{
  char buf[128] = { 0 };
  int p, op;

  struct Keymap *key = km_find_func(mtype, OP_HELP);
  if (!key && (mtype != MENU_EDITOR) && (mtype != MENU_PAGER))
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
  mutt_unget_op(OP_END_COND);
  p = key->len;
  while (p--)
    mutt_unget_ch(key->keys[p]);

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
  op = km_dokey(mtype);
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
 * mutt_parse_push - Parse the 'push' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_push(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  parse_extract_token(buf, s, TOKEN_CONDENSE);
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "push");
    return MUTT_CMD_ERROR;
  }

  generic_tokenize_push_string(buf->data, mutt_push_macro_event);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_keymap - Parse a user-config key binding
 * @param mtypes    Array for results
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
static char *parse_keymap(enum MenuType *mtypes, struct Buffer *s, int max_menus,
                          int *num_menus, struct Buffer *err, bool bind)
{
  struct Buffer buf;
  int i = 0;
  char *q = NULL;

  buf_init(&buf);

  /* menu name */
  parse_extract_token(&buf, s, TOKEN_NO_FLAGS);
  char *p = buf.data;
  if (MoreArgs(s))
  {
    while (i < max_menus)
    {
      q = strchr(p, ',');
      if (q)
        *q = '\0';

      int val = mutt_map_get_value(p, MenuNames);
      if (val == -1)
      {
        buf_printf(err, _("%s: no such menu"), p);
        goto error;
      }
      mtypes[i] = val;
      i++;
      if (q)
        p = q + 1;
      else
        break;
    }
    *num_menus = i;
    /* key sequence */
    parse_extract_token(&buf, s, TOKEN_NO_FLAGS);

    if (buf.data[0] == '\0')
    {
      buf_printf(err, _("%s: null key sequence"), bind ? "bind" : "macro");
    }
    else if (MoreArgs(s))
    {
      return buf.data;
    }
  }
  else
  {
    buf_printf(err, _("%s: too few arguments"), bind ? "bind" : "macro");
  }
error:
  FREE(&buf.data);
  return NULL;
}

/**
 * try_bind - Try to make a key binding
 * @param key   Key name
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param func  Function name
 * @param funcs Functions table
 * @param err   Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult try_bind(char *key, enum MenuType mtype, char *func,
                                   const struct MenuFuncOp *funcs, struct Buffer *err)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (mutt_str_equal(func, funcs[i].name))
    {
      return km_bindkey_err(key, mtype, funcs[i].op, err);
    }
  }
  if (err)
  {
    buf_printf(err, _("Function '%s' not available for menu '%s'"), func,
               mutt_map_get_name(mtype, MenuNames));
  }
  return MUTT_CMD_ERROR; /* Couldn't find an existing function with this name */
}

/**
 * km_get_table - Lookup a Menu's functions
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @retval ptr Array of functions
 */
const struct MenuFuncOp *km_get_table(enum MenuType mtype)
{
  switch (mtype)
  {
    case MENU_ALIAS:
      return OpAlias;
    case MENU_ATTACH:
      return OpAttach;
#ifdef USE_AUTOCRYPT
    case MENU_AUTOCRYPT_ACCT:
      return OpAutocrypt;
#endif
    case MENU_COMPOSE:
      return OpCompose;
    case MENU_DIALOG:
      return OpDialog;
    case MENU_EDITOR:
      return OpEditor;
    case MENU_FOLDER:
      return OpBrowser;
    case MENU_GENERIC:
      return OpGeneric;
    case MENU_INDEX:
      return OpIndex;
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
    case MENU_PAGER:
      return OpPager;
    case MENU_PGP:
      return (WithCrypto & APPLICATION_PGP) ? OpPgp : NULL;
    case MENU_POSTPONE:
      return OpPostpone;
    case MENU_QUERY:
      return OpQuery;
    default:
      return NULL;
  }
}

/**
 * dump_bind - Dumps all the binds maps of a menu into a buffer
 * @param buf   Output buffer
 * @param menu  Menu to dump
 * @retval true  Menu is empty
 * @retval false Menu is not empty
 */
static bool dump_bind(struct Buffer *buf, struct Mapping *menu)
{
  bool empty = true;
  struct Keymap *map = NULL;

  STAILQ_FOREACH(map, &Keymaps[menu->value], entries)
  {
    if (map->op == OP_MACRO)
      continue;

    char key_binding[128] = { 0 };
    const char *fn_name = NULL;

    km_expand_key(key_binding, sizeof(key_binding), map);
    if (map->op == OP_NULL)
    {
      buf_add_printf(buf, "bind %s %s noop\n", menu->name, key_binding);
      continue;
    }

    /* The pager and editor menus don't use the generic map,
     * however for other menus try generic first. */
    if ((menu->value != MENU_PAGER) && (menu->value != MENU_EDITOR) &&
        (menu->value != MENU_GENERIC))
    {
      fn_name = mutt_get_func(OpGeneric, map->op);
    }

    /* if it's one of the menus above or generic doesn't find
     * the function, try with its own menu. */
    if (!fn_name)
    {
      const struct MenuFuncOp *funcs = km_get_table(menu->value);
      if (!funcs)
        continue;

      fn_name = mutt_get_func(funcs, map->op);
    }

    buf_add_printf(buf, "bind %s %s %s\n", menu->name, key_binding, fn_name);
    empty = false;
  }

  return empty;
}

/**
 * dump_all_binds - Dumps all the binds inside every menu
 * @param buf  Output buffer
 */
static void dump_all_binds(struct Buffer *buf)
{
  for (int i = 0; i < MENU_MAX; i++)
  {
    const char *menu_name = mutt_map_get_name(i, MenuNames);
    struct Mapping menu = { menu_name, i };

    const bool empty = dump_bind(buf, &menu);

    /* Add a new line for readability between menus. */
    if (!empty && (i < (MENU_MAX - 1)))
      buf_addch(buf, '\n');
  }
}

/**
 * dump_macro - Dumps all the macros maps of a menu into a buffer
 * @param buf   Output buffer
 * @param menu  Menu to dump
 * @retval true  Menu is empty
 * @retval false Menu is not empty
 */
static bool dump_macro(struct Buffer *buf, struct Mapping *menu)
{
  bool empty = true;
  struct Keymap *map = NULL;

  STAILQ_FOREACH(map, &Keymaps[menu->value], entries)
  {
    if (map->op != OP_MACRO)
      continue;

    char key_binding[128] = { 0 };
    km_expand_key(key_binding, sizeof(key_binding), map);

    struct Buffer tmp = buf_make(0);
    escape_string(&tmp, map->macro);

    if (map->desc)
    {
      buf_add_printf(buf, "macro %s %s \"%s\" \"%s\"\n", menu->name,
                     key_binding, tmp.data, map->desc);
    }
    else
    {
      buf_add_printf(buf, "macro %s %s \"%s\"\n", menu->name, key_binding, tmp.data);
    }

    buf_dealloc(&tmp);
    empty = false;
  }

  return empty;
}

/**
 * dump_all_macros - Dumps all the macros inside every menu
 * @param buf  Output buffer
 */
static void dump_all_macros(struct Buffer *buf)
{
  for (int i = 0; i < MENU_MAX; i++)
  {
    const char *menu_name = mutt_map_get_name(i, MenuNames);
    struct Mapping menu = { menu_name, i };

    const bool empty = dump_macro(buf, &menu);

    /* Add a new line for legibility between menus. */
    if (!empty && (i < (MENU_MAX - 1)))
      buf_addch(buf, '\n');
  }
}

/**
 * dump_bind_macro - Parse 'bind' and 'macro' commands - Implements ICommand::parse()
 */
static enum CommandResult dump_bind_macro(struct Buffer *buf, struct Buffer *s,
                                          intptr_t data, struct Buffer *err)
{
  FILE *fp_out = NULL;
  bool dump_all = false, bind = (data == 0);

  if (!MoreArgs(s))
    dump_all = true;
  else
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  if (MoreArgs(s))
  {
    /* More arguments potentially means the user is using the
     * ::command_t :bind command thus we delegate the task. */
    return MUTT_CMD_ERROR;
  }

  struct Buffer filebuf = buf_make(4096);
  if (dump_all || mutt_istr_equal(buf->data, "all"))
  {
    if (bind)
      dump_all_binds(&filebuf);
    else
      dump_all_macros(&filebuf);
  }
  else
  {
    const int menu_index = mutt_map_get_value(buf->data, MenuNames);
    if (menu_index == -1)
    {
      // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
      buf_printf(err, _("%s: no such menu"), buf->data);
      buf_dealloc(&filebuf);
      return MUTT_CMD_ERROR;
    }

    struct Mapping menu = { buf->data, menu_index };
    if (bind)
      dump_bind(&filebuf, &menu);
    else
      dump_macro(&filebuf, &menu);
  }

  if (buf_is_empty(&filebuf))
  {
    // L10N: '%s' is the name of the menu, e.g. 'index' or 'pager',
    //       it might also be 'all' when all menus are affected.
    buf_printf(err, bind ? _("%s: no binds for this menu") : _("%s: no macros for this menu"),
               dump_all ? "all" : buf->data);
    buf_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  fp_out = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    buf_dealloc(&filebuf);
    buf_pool_release(&tempfile);
    return MUTT_CMD_ERROR;
  }
  fputs(filebuf.data, fp_out);

  mutt_file_fclose(&fp_out);
  buf_dealloc(&filebuf);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = (bind) ? "bind" : "macro";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);

  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_bind - Parse the 'bind' command - Implements Command::parse() - @ingroup command_parse
 *
 * bind menu-name `<key_sequence>` function-name
 */
enum CommandResult mutt_parse_bind(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `s` because dump_bind_macro() might change it
    char *dptr = s->dptr;
    if (dump_bind_macro(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!buf_is_empty(err))
      return MUTT_CMD_ERROR;
    s->dptr = dptr;
  }

  const struct MenuFuncOp *funcs = NULL;
  enum MenuType mtypes[MenuNamesLen];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  char *key = parse_keymap(mtypes, s, mutt_array_size(mtypes), &num_menus, err, true);
  if (!key)
    return MUTT_CMD_ERROR;

  /* function to execute */
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "bind");
    rc = MUTT_CMD_ERROR;
  }
  else if (mutt_istr_equal("noop", buf->data))
  {
    for (int i = 0; i < num_menus; i++)
    {
      km_bindkey(key, mtypes[i], OP_NULL); /* the 'unbind' command */
      funcs = km_get_table(mtypes[i]);
      if (funcs)
      {
        char keystr[32] = { 0 };
        km_expand_key_string(key, keystr, sizeof(keystr));
        const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
        mutt_debug(LL_NOTIFY, "NT_BINDING_DELETE: %s %s\n", mname, keystr);

        int op = get_op(OpGeneric, buf->data, mutt_str_len(buf->data));
        struct EventBinding ev_b = { mtypes[i], key, op };
        notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_DELETE, &ev_b);
      }
    }
  }
  else
  {
    for (int i = 0; i < num_menus; i++)
    {
      /* The pager and editor menus don't use the generic map,
       * however for other menus try generic first. */
      if ((mtypes[i] != MENU_PAGER) && (mtypes[i] != MENU_EDITOR) && (mtypes[i] != MENU_GENERIC))
      {
        rc = try_bind(key, mtypes[i], buf->data, OpGeneric, err);
        if (rc == MUTT_CMD_SUCCESS)
        {
          char keystr[32] = { 0 };
          km_expand_key_string(key, keystr, sizeof(keystr));
          const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
          mutt_debug(LL_NOTIFY, "NT_BINDING_NEW: %s %s\n", mname, keystr);

          int op = get_op(OpGeneric, buf->data, mutt_str_len(buf->data));
          struct EventBinding ev_b = { mtypes[i], key, op };
          notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_ADD, &ev_b);
          continue;
        }
        if (rc == MUTT_CMD_WARNING)
          break;
      }

      /* Clear any error message, we're going to try again */
      err->data[0] = '\0';
      funcs = km_get_table(mtypes[i]);
      if (funcs)
      {
        rc = try_bind(key, mtypes[i], buf->data, funcs, err);
        if (rc == MUTT_CMD_SUCCESS)
        {
          char keystr[32] = { 0 };
          km_expand_key_string(key, keystr, sizeof(keystr));
          const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
          mutt_debug(LL_NOTIFY, "NT_BINDING_NEW: %s %s\n", mname, keystr);

          int op = get_op(funcs, buf->data, mutt_str_len(buf->data));
          struct EventBinding ev_b = { mtypes[i], key, op };
          notify_send(NeoMutt->notify, NT_BINDING, NT_BINDING_ADD, &ev_b);
          continue;
        }
      }
    }
  }
  FREE(&key);
  return rc;
}

/**
 * parse_menu - Parse menu-names into an array
 * @param menus    Array for results
 * @param s        String containing menu-names
 * @param err      Buffer for error messages
 * @retval NULL Always
 *
 * Expects to see: <menu-string>[,<menu-string>]
 */
static void *parse_menu(bool *menus, char *s, struct Buffer *err)
{
  char *menu_names_dup = mutt_str_dup(s);
  char *marker = menu_names_dup;
  char *menu_name = NULL;

  while ((menu_name = mutt_str_sep(&marker, ",")))
  {
    int value = mutt_map_get_value(menu_name, MenuNames);
    if (value == -1)
    {
      buf_printf(err, _("%s: no such menu"), menu_name);
      break;
    }
    else
    {
      menus[value] = true;
    }
  }

  FREE(&menu_names_dup);
  return NULL;
}

/**
 * km_unbind_all - Free all the keys in the supplied Keymap
 * @param km_list Keymap mapping
 * @param mode    Undo bind or macro, e.g. #MUTT_UNBIND, #MUTT_UNMACRO
 *
 * Iterate through Keymap and free keys defined either by "macro" or "bind".
 */
static void km_unbind_all(struct KeymapList *km_list, unsigned long mode)
{
  struct Keymap *np = NULL, *tmp = NULL;

  STAILQ_FOREACH_SAFE(np, km_list, entries, tmp)
  {
    if (((mode & MUTT_UNBIND) && !np->macro) || ((mode & MUTT_UNMACRO) && np->macro))
    {
      STAILQ_REMOVE(km_list, np, Keymap, entries);
      mutt_keymap_free(&np);
    }
  }
}

/**
 * mutt_parse_unbind - Parse the 'unbind' command - Implements Command::parse() - @ingroup command_parse
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
  bool menu_matches[MENU_MAX] = { 0 };
  bool all_keys = false;
  char *key = NULL;

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (mutt_str_equal(buf->data, "*"))
  {
    for (enum MenuType i = 0; i < MENU_MAX; i++)
      menu_matches[i] = true;
  }
  else
  {
    parse_menu(menu_matches, buf->data, err);
  }

  if (MoreArgs(s))
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    key = buf->data;
  }
  else
  {
    all_keys = true;
  }

  if (MoreArgs(s))
  {
    const char *cmd = (data & MUTT_UNMACRO) ? "unmacro" : "unbind";

    buf_printf(err, _("%s: too many arguments"), cmd);
    return MUTT_CMD_ERROR;
  }

  for (enum MenuType i = 0; i < MENU_MAX; i++)
  {
    if (!menu_matches[i])
      continue;
    if (all_keys)
    {
      km_unbind_all(&Keymaps[i], data);
      km_bindkey("<enter>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
      km_bindkey("<return>", MENU_GENERIC, OP_GENERIC_SELECT_ENTRY);
      km_bindkey("<enter>", MENU_INDEX, OP_DISPLAY_MESSAGE);
      km_bindkey("<return>", MENU_INDEX, OP_DISPLAY_MESSAGE);
      km_bindkey("<backspace>", MENU_EDITOR, OP_EDITOR_BACKSPACE);
      km_bindkey("\177", MENU_EDITOR, OP_EDITOR_BACKSPACE);
      km_bindkey(":", MENU_GENERIC, OP_ENTER_COMMAND);
      km_bindkey(":", MENU_PAGER, OP_ENTER_COMMAND);
      if (i != MENU_EDITOR)
      {
        km_bindkey("?", i, OP_HELP);
        km_bindkey("q", i, OP_EXIT);
      }

      const char *mname = mutt_map_get_name(i, MenuNames);
      mutt_debug(LL_NOTIFY, "NT_MACRO_DELETE_ALL: %s\n", mname);

      struct EventBinding ev_b = { i, NULL, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING,
                  (data & MUTT_UNMACRO) ? NT_MACRO_DELETE_ALL : NT_BINDING_DELETE_ALL,
                  &ev_b);
    }
    else
    {
      char keystr[32] = { 0 };
      km_expand_key_string(key, keystr, sizeof(keystr));
      const char *mname = mutt_map_get_name(i, MenuNames);
      mutt_debug(LL_NOTIFY, "NT_MACRO_DELETE: %s %s\n", mname, keystr);

      km_bindkey(key, i, OP_NULL);
      struct EventBinding ev_b = { i, key, OP_NULL };
      notify_send(NeoMutt->notify, NT_BINDING,
                  (data & MUTT_UNMACRO) ? NT_MACRO_DELETE : NT_BINDING_DELETE, &ev_b);
    }
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_macro - Parse the 'macro' command - Implements Command::parse() - @ingroup command_parse
 *
 * macro `<menu>` `<key>` `<macro>` `<description>`
 */
enum CommandResult mutt_parse_macro(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  if (StartupComplete)
  {
    // Save and restore the offset in `s` because dump_bind_macro() might change it
    char *dptr = s->dptr;
    if (dump_bind_macro(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!buf_is_empty(err))
      return MUTT_CMD_ERROR;
    s->dptr = dptr;
  }

  enum MenuType mtypes[MenuNamesLen];
  int num_menus = 0;
  enum CommandResult rc = MUTT_CMD_ERROR;

  char *key = parse_keymap(mtypes, s, mutt_array_size(mtypes), &num_menus, err, false);
  if (!key)
    return MUTT_CMD_ERROR;

  parse_extract_token(buf, s, TOKEN_CONDENSE);
  /* make sure the macro sequence is not an empty string */
  if (buf->data[0] == '\0')
  {
    buf_strcpy(err, _("macro: empty key sequence"));
  }
  else
  {
    if (MoreArgs(s))
    {
      char *seq = mutt_str_dup(buf->data);
      parse_extract_token(buf, s, TOKEN_CONDENSE);

      if (MoreArgs(s))
      {
        buf_printf(err, _("%s: too many arguments"), "macro");
      }
      else
      {
        for (int i = 0; i < num_menus; i++)
        {
          rc = km_bind(key, mtypes[i], OP_MACRO, seq, buf->data);
          if (rc == MUTT_CMD_SUCCESS)
          {
            char keystr[32] = { 0 };
            km_expand_key_string(key, keystr, sizeof(keystr));
            const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
            mutt_debug(LL_NOTIFY, "NT_MACRO_NEW: %s %s\n", mname, keystr);

            struct EventBinding ev_b = { mtypes[i], key, OP_MACRO };
            notify_send(NeoMutt->notify, NT_BINDING, NT_MACRO_ADD, &ev_b);
            continue;
          }
        }
      }

      FREE(&seq);
    }
    else
    {
      for (int i = 0; i < num_menus; i++)
      {
        rc = km_bind(key, mtypes[i], OP_MACRO, buf->data, NULL);
        if (rc == MUTT_CMD_SUCCESS)
        {
          char keystr[32] = { 0 };
          km_expand_key_string(key, keystr, sizeof(keystr));
          const char *mname = mutt_map_get_name(mtypes[i], MenuNames);
          mutt_debug(LL_NOTIFY, "NT_MACRO_NEW: %s %s\n", mname, keystr);

          struct EventBinding ev_b = { mtypes[i], key, OP_MACRO };
          notify_send(NeoMutt->notify, NT_BINDING, NT_MACRO_ADD, &ev_b);
          continue;
        }
      }
    }
  }
  FREE(&key);
  return rc;
}

/**
 * mutt_parse_exec - Parse the 'exec' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_exec(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  int ops[128];
  int nops = 0;
  const struct MenuFuncOp *funcs = NULL;
  char *function = NULL;

  if (!MoreArgs(s))
  {
    buf_strcpy(err, _("exec: no arguments"));
    return MUTT_CMD_ERROR;
  }

  do
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    function = buf->data;

    const enum MenuType mtype = menu_get_current_type();
    funcs = km_get_table(mtype);
    if (!funcs && (mtype != MENU_PAGER))
      funcs = OpGeneric;

    ops[nops] = get_op(funcs, function, mutt_str_len(function));
    if ((ops[nops] == OP_NULL) && (mtype != MENU_PAGER) && (mtype != MENU_GENERIC))
    {
      ops[nops] = get_op(OpGeneric, function, mutt_str_len(function));
    }

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
 * mw_what_key - Display the value of a key - @ingroup gui_mw
 *
 * This function uses the message window.
 *
 * Displays the octal value back to the user. e.g.
 * `Char = h, Octal = 150, Decimal = 104`
 *
 * Press the $abort_key (default Ctrl-G) to exit.
 */
void mw_what_key(void)
{
  int ch;

  struct MuttWindow *win = msgwin_get_window();
  if (!win)
    return;

  mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROMPT);
  mutt_window_mvprintw(win, 0, 0, _("Enter keys (%s to abort): "), km_keyname(AbortKey));
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  enum MuttCursorState old_cursor = mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

  mutt_set_timeout(1000); // 1 second
  while (true)
  {
    ch = getch();
    if (ch == AbortKey)
      break;

    if (ch == ERR) // Timeout
      continue;

    mutt_message(_("Char = %s, Octal = %o, Decimal = %d"), km_keyname(ch), ch, ch);
    mutt_window_move(win, 0, 0);
  }

  mutt_set_timeout(-1);
  mutt_curses_set_cursor(old_cursor);

  mutt_flushinp();
  mutt_clear_error();
}

/**
 * mutt_keys_cleanup - Free the key maps
 */
void mutt_keys_cleanup(void)
{
  for (int i = 0; i < MENU_MAX; i++)
  {
    mutt_keymaplist_free(&Keymaps[i]);
  }
}
