/**
 * @file
 * Get a key from the user
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page key_get Get a key from the user
 *
 * Get a key from the user
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "get.h"
#include "menu/lib.h"
#include "globals.h"
#include "keymap.h"
#include "menu.h"
#include "module_data.h"
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

/// XXX
static const int MaxKeyLoop = 10;

// It's not possible to unget more than one char under some curses libs,
// so roll our own input buffering routines.

/// MacroEvents moved to KeyModuleData
/// UngetKeyEvents moved to KeyModuleData

/**
 * mutt_flushinp - Empty all the keyboard buffers
 */
void mutt_flushinp(void)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);
  ARRAY_SHRINK(&mod_data->unget_key_events, ARRAY_SIZE(&mod_data->unget_key_events));
  ARRAY_SHRINK(&mod_data->macro_events, ARRAY_SIZE(&mod_data->macro_events));
  flushinp();
}

/**
 * array_pop - Remove an event from the array
 * @param a Array
 * @retval ptr Event
 */
struct KeyEvent *array_pop(struct KeyEventArray *a)
{
  if (ARRAY_EMPTY(a))
  {
    return NULL;
  }

  struct KeyEvent *event = ARRAY_LAST(a);
  ARRAY_SHRINK(a, 1);
  return event;
}

/**
 * array_add - Add an event to the end of the array
 * @param a  Array
 * @param ch Character
 * @param op Operation, e.g. OP_DELETE
 */
void array_add(struct KeyEventArray *a, int ch, int op)
{
  struct KeyEvent event = { ch, op, 0 };
  ARRAY_ADD(a, event);
}

/**
 * array_to_endcond - Clear the array until an OP_END_COND
 * @param a Array
 */
void array_to_endcond(struct KeyEventArray *a)
{
  while (!ARRAY_EMPTY(a))
  {
    if (array_pop(a)->op == OP_END_COND)
    {
      return;
    }
  }
}

/**
 * mutt_unget_ch - Return a keystroke to the input buffer
 * @param ch Key press
 *
 * This puts events into the `UngetKeyEvents` buffer
 */
void mutt_unget_ch(int ch)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);
  array_add(&mod_data->unget_key_events, ch, OP_NULL);
}

/**
 * mutt_unget_op - Return an operation to the input buffer
 * @param op Operation, e.g. OP_DELETE
 *
 * This puts events into the `UngetKeyEvents` buffer
 */
void mutt_unget_op(int op)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);
  array_add(&mod_data->unget_key_events, 0, op);
}

/**
 * mutt_push_macro_event - Add the character/operation to the macro buffer
 * @param ch Character to add
 * @param op Operation to add
 *
 * Adds the ch/op to the macro buffer.
 * This should be used for macros, push, and exec commands only.
 */
void mutt_push_macro_event(int ch, int op)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);
  array_add(&mod_data->macro_events, ch, op);
}

/**
 * mutt_flush_macro_to_endcond - Drop a macro from the input buffer
 *
 * All the macro text is deleted until an OP_END_COND command,
 * or the buffer is empty.
 */
void mutt_flush_macro_to_endcond(void)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);
  array_to_endcond(&mod_data->macro_events);
}

#ifdef USE_INOTIFY
/**
 * mutt_monitor_getch_timeout - Get a character and poll the filesystem monitor
 * @param timeout_ms Timeout in milliseconds
 * @retval num Character pressed
 * @retval ERR Timeout
 */
static int mutt_monitor_getch_timeout(int timeout_ms)
{
  /* ncurses has its own internal buffer, so before we perform a poll,
   * we need to make sure there isn't a character waiting */
  timeout(0);
  int ch = getch();
  timeout(timeout_ms);
  if (ch == ERR)
  {
    if (mutt_monitor_poll() != 0)
      ch = ERR;
    else
      ch = getch();
  }
  return ch;
}
#endif /* USE_INOTIFY */

/**
 * mutt_getch_timeout - Read a character from the input buffer with timeout
 * @param flags      Flags, e.g. #GETCH_IGNORE_MACRO
 * @param timeout_ms Timeout in milliseconds
 * @retval obj KeyEvent to process
 */
static struct KeyEvent mutt_getch_timeout(GetChFlags flags, int timeout_ms)
{
  static const struct KeyEvent event_abort = { 0, OP_ABORT, 0 };
  static const struct KeyEvent event_repaint = { 0, OP_REPAINT, 0 };
  static const struct KeyEvent event_timeout = { 0, OP_TIMEOUT, 0 };

  if (!OptGui)
    return event_abort;

  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);

  struct KeyEvent *event_key = array_pop(&mod_data->unget_key_events);
  if (event_key)
    return *event_key;

  if (!(flags & GETCH_IGNORE_MACRO))
  {
    event_key = array_pop(&mod_data->macro_events);
    if (event_key)
      return *event_key;
  }

  int ch;
  SigInt = false;
  mutt_sig_allow_interrupt(true);
  timeout(timeout_ms);
#ifdef USE_INOTIFY
  ch = mutt_monitor_getch_timeout(timeout_ms);
#else
  ch = getch();
#endif
  mutt_sig_allow_interrupt(false);

  if (SigInt)
  {
    mutt_query_exit();
    return event_abort;
  }

  if (ch == KEY_RESIZE)
  {
    timeout(0);
    while ((ch = getch()) == KEY_RESIZE)
    {
      // do nothing
    }
  }

  if (ch == ERR)
  {
    if (!isatty(STDIN_FILENO)) // terminal was lost
      mutt_exit(1);

    if (SigWinch)
    {
      SigWinch = false;
      notify_send(NeoMutt->notify_resize, NT_RESIZE, 0, NULL);
      return event_repaint;
    }

    notify_send(NeoMutt->notify_timeout, NT_TIMEOUT, 0, NULL);
    return event_timeout;
  }

  if (ch == mod_data->abort_key)
    return event_abort;

  if (ch & 0x80)
  {
    const bool c_meta_key = cs_subset_bool(NeoMutt->sub, "meta_key");
    if (c_meta_key)
    {
      /* send ALT-x as ESC-x */
      ch &= ~0x80;
      mutt_unget_ch(ch);
      return (struct KeyEvent) { '\033', OP_NULL, 0 }; // Escape
    }
  }

  return (struct KeyEvent) { ch, OP_NULL, 0 };
}

/**
 * mutt_getch - Read a character from the input buffer
 * @param flags Flags, e.g. #GETCH_IGNORE_MACRO
 * @retval obj KeyEvent to process
 *
 * The priority for reading events is:
 * 1. UngetKeyEvents buffer
 * 2. MacroEvents buffer
 * 3. Keyboard
 *
 * This function can return:
 * - Abort   `{ 0, OP_ABORT,   0 }`
 * - Repaint `{ 0, OP_REPAINT, 0 }`
 * - Timeout `{ 0, OP_TIMEOUT, 0 }`
 */
struct KeyEvent mutt_getch(GetChFlags flags)
{
  return mutt_getch_timeout(flags, 1000);
}

/**
 * km_error_key - Handle an unbound key sequence
 * @param md Menu Definition
 */
void km_error_key(const struct MenuDefinition *md)
{
  if (!md)
    return;

  struct Keymap *key = km_find_func(md, OP_HELP);
  if (!key)
  {
    mutt_error(_("Key is not bound"));
    return;
  }

  struct Buffer *buf = buf_pool_get();
  keymap_expand_key(key, buf);
  mutt_error(_("Key is not bound.  Press '%s' for help."), buf_string(buf));
  buf_pool_release(&buf);
}

/**
 * generic_tokenize_push_string - Parse and queue a 'push' command
 * @param s String to push into the key queue
 *
 * Parses s for `<function>` syntax and adds the whole sequence the macro buffer.
 */
void generic_tokenize_push_string(char *s)
{
  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);
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
          mutt_push_macro_event(KEY_F(i), 0);
          p = pp - 1;
          continue;
        }

        l = p - pp + 1;
        for (i = 0; mod_data->key_names[i].name; i++)
        {
          if (mutt_istrn_equal(pp, mod_data->key_names[i].name, l))
            break;
        }
        if (mod_data->key_names[i].name)
        {
          /* found a match */
          mutt_push_macro_event(mod_data->key_names[i].value, 0);
          p = pp - 1;
          continue;
        }

        /* See if it is a valid command
         * skip the '<' and the '>' when comparing */
        struct Buffer *buf = buf_pool_get();
        buf_strcpy_n(buf, pp + 1, l - 2);

        for (enum MenuType j = 0; j < MENU_MAX; j++)
        {
          op = km_get_op_menu(j, buf_string(buf));
          if (op != OP_NULL)
            break;
        }

        buf_pool_release(&buf);

        if (op != OP_NULL)
        {
          mutt_push_macro_event(0, op);
          p = pp - 1;
          continue;
        }
      }
    }
    mutt_push_macro_event((unsigned char) *p--, 0); /* independent 8 bits chars */
  }
}

/**
 * gather_functions - Find functions whose keybindings match
 * @param[in]  md      Menu Definition of all valid functions
 * @param[in]  keys    User-entered key string to match
 * @param[in]  key_len Length of key string
 * @param[out] kma     Array for the results
 * @retval flags Results, e.g. #KEY_GATHER_MATCH
 */
KeyGatherFlags gather_functions(const struct MenuDefinition *md, const keycode_t *keys,
                                int key_len, struct KeymapMatchArray *kma)
{
  if (!md || !keys || !kma)
    return 0;

  struct SubMenu **smp = NULL;
  KeyGatherFlags flags = KEY_GATHER_NO_MATCH;

  ARRAY_FOREACH(smp, &md->submenus)
  {
    struct Keymap *km = NULL;

    STAILQ_FOREACH(km, &(*smp)->keymaps, entries)
    {
      bool match = true;

      for (int i = 0; i < key_len; i++)
      {
        if (keys[i] != km->keys[i])
        {
          match = false;
          break;
        }
      }

      if (match)
      {
        KeyGatherFlags fmatch = KEY_GATHER_NO_MATCH;
        if (km->len == key_len)
          fmatch = KEY_GATHER_MATCH;
        else
          fmatch = KEY_GATHER_LONGER;

        flags |= fmatch;

        struct KeymapMatch kmatch = { md->id, fmatch, km };
        ARRAY_ADD(kma, kmatch);
      }
    }
  }

  return flags;
}

/**
 * km_dokey - Determine what a keypress should do
 * @param md    Menu Definition
 * @param flags Flags, e.g. #GETCH_IGNORE_MACRO
 * @retval ptr Event
 */
struct KeyEvent km_dokey(const struct MenuDefinition *md, GetChFlags flags)
{
  struct KeyEvent event = { 0, OP_NULL, 0 };
  int pos = 0;
  keycode_t keys[MAX_SEQ] = { 0 };

  if (!md)
    return event;

  for (int n = 0; n < MaxKeyLoop; n++)
  {
    event = mutt_getch(flags);
    mutt_debug(LL_DEBUG1, "KEY: \n");

    // abort, timeout, repaint
    if (event.op < OP_NULL)
    {
      mutt_debug(LL_DEBUG1, "KEY: getch() %s\n", opcodes_get_name(event.op));
      return event;
    }

    mutt_debug(LL_DEBUG1, "KEY: getch() '%c'\n", isprint(event.ch) ? event.ch : '?');

    keys[pos] = event.ch;
    struct KeymapMatchArray kma = ARRAY_HEAD_INITIALIZER;
    KeyGatherFlags kfg = gather_functions(md, keys, pos + 1, &kma);

    mutt_debug(LL_DEBUG1, "KEY: flags = %x\n", kfg);

    if (kfg == KEY_GATHER_NO_MATCH)
    {
      mutt_debug(LL_DEBUG1, "KEY: \033[1;31mFAIL1: ('%c', %s)\033[0m\n",
                 isprint(event.ch) ? event.ch : '?', opcodes_get_name(event.op));
      return event;
    }

    if ((kfg & KEY_GATHER_MATCH) == KEY_GATHER_MATCH)
    {
      struct KeymapMatch *kmatch = NULL;

      ARRAY_FOREACH(kmatch, &kma)
      {
        if (kmatch->flags == KEY_GATHER_MATCH)
        {
          struct Keymap *map = kmatch->keymap;

          if (map->op != OP_MACRO)
          {
            mutt_debug(LL_DEBUG1, "KEY: \033[1;32mSUCCESS: ('%c', %s)\033[0m\n",
                       isprint(event.ch) ? event.ch : '?', opcodes_get_name(map->op));
            ARRAY_FREE(&kma);
            return (struct KeyEvent) { event.ch, map->op };
          }

          /* #GETCH_IGNORE_MACRO turns off processing the MacroEvents buffer
           * in mutt_getch().  Generating new macro events during that time would
           * result in undesired behavior once the option is turned off.
           *
           * Originally this returned -1, however that results in an unbuffered
           * username or password prompt being aborted.  Returning OP_NULL allows
           * mw_get_field() to display the keybinding pressed instead.
           *
           * It may be unexpected for a macro's keybinding to be returned,
           * but less so than aborting the prompt.  */
          if (flags & GETCH_IGNORE_MACRO)
          {
            ARRAY_FREE(&kma);
            return (struct KeyEvent) { event.ch, OP_NULL };
          }

          generic_tokenize_push_string(map->macro);
          pos = 0;
          ARRAY_FREE(&kma);
          break;
        }
      }
    }
    else
    {
      mutt_debug(LL_DEBUG1, "KEY: \033[1;33mLONGER: getch() '%c'\033[0m\n",
                 isprint(event.ch) ? event.ch : '?');
      pos++;
    }

    ARRAY_FREE(&kma);
  }

  mutt_flushinp();
  mutt_error(_("Macro loop detected"));
  return (struct KeyEvent) { '\0', OP_ABORT, 0 };
}
