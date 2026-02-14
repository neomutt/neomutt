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
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "get.h"
#include "menu/lib.h"
#include "globals.h"
#include "init.h"
#include "keymap.h"
#include "menu.h"
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

/// XXX
static const int MaxKeyLoop = 64;

// It's not possible to unget more than one char under some curses libs,
// so roll our own input buffering routines.

/// These are used for macros and exec/push commands.
/// They can be temporarily ignored by passing #GETCH_IGNORE_MACRO
struct KeyEventArray MacroEvents = ARRAY_HEAD_INITIALIZER;

/// These are used in all other "normal" situations,
/// and are not ignored when passing #GETCH_IGNORE_MACRO
struct KeyEventArray UngetKeyEvents = ARRAY_HEAD_INITIALIZER;

/**
 * mutt_flushinp - Empty all the keyboard buffers
 */
void mutt_flushinp(void)
{
  ARRAY_SHRINK(&UngetKeyEvents, ARRAY_SIZE(&UngetKeyEvents));
  ARRAY_SHRINK(&MacroEvents, ARRAY_SIZE(&MacroEvents));
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
  array_add(&UngetKeyEvents, ch, OP_NULL);
}

/**
 * mutt_unget_op - Return an operation to the input buffer
 * @param op Operation, e.g. OP_DELETE
 *
 * This puts events into the `UngetKeyEvents` buffer
 */
void mutt_unget_op(int op)
{
  array_add(&UngetKeyEvents, 0, op);
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
  array_add(&MacroEvents, ch, op);
}

/**
 * mutt_flush_macro_to_endcond - Drop a macro from the input buffer
 *
 * All the macro text is deleted until an OP_END_COND command,
 * or the buffer is empty.
 */
void mutt_flush_macro_to_endcond(void)
{
  array_to_endcond(&MacroEvents);
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

  struct KeyEvent *event_key = array_pop(&UngetKeyEvents);
  if (event_key)
    return *event_key;

  if (!(flags & GETCH_IGNORE_MACRO))
  {
    event_key = array_pop(&MacroEvents);
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

  if (ch == AbortKey)
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
 * @param mtype Menu type, e.g. #MENU_PAGER
 */
void km_error_key(enum MenuType mtype)
{
  struct Keymap *key = km_find_func(mtype, OP_HELP);
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
        for (i = 0; KeyNames[i].name; i++)
        {
          if (mutt_istrn_equal(pp, KeyNames[i].name, l))
            break;
        }
        if (KeyNames[i].name)
        {
          /* found a match */
          mutt_push_macro_event(KeyNames[i].value, 0);
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
      if (key_len > km->len)
        continue;

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
 * enum DokeyState - Internal state for km_dokey()
 */
enum DokeyState
{
  DKS_START = 1, ///< Initial state, no input received yet
  DKS_COUNTER,   ///< Reading count prefix digits
  DKS_NEED_MORE, ///< Prefix matches an exact and/or longer keybinding
};

/**
 * km_dokey - Determine what a keypress should do
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @param flags Flags, e.g. #GETCH_IGNORE_MACRO
 * @retval ptr Event
 */
struct KeyEvent km_dokey(enum MenuType mtype, GetChFlags flags)
{
  struct KeyEvent event = { 0, OP_NULL, 0 };
  enum DokeyState state = DKS_START;
  int count = 0;
  int count_digits = 0;
  int key_len = 0;
  struct Keymap *pending_exact = NULL;
  const struct MenuDefinition *md = NULL;
  keycode_t keys[KEY_SEQ_MAX_LEN] = { 0 };

  const int c_key_timeout_initial = 1000;
  const int c_key_timeout_progress = 700;

  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (md->id == mtype)
      break;
  }

  for (int n = 0; n < MaxKeyLoop; n++)
  {
    const int timeout_ms = (state == DKS_START) ? c_key_timeout_initial : c_key_timeout_progress;
    event = mutt_getch_timeout(flags, timeout_ms);
    mutt_debug(LL_DEBUG1, "KEY: \n");

    // abort, timeout, repaint
    if (event.op < OP_NULL)
    {
      if (event.op == OP_TIMEOUT)
      {
        if ((state == DKS_NEED_MORE) && pending_exact)
        {
          if (pending_exact->op != OP_MACRO)
          {
            return (struct KeyEvent) { 0, pending_exact->op, count };
          }

          if (flags & GETCH_IGNORE_MACRO)
          {
            return (struct KeyEvent) { 0, OP_NULL, 0 };
          }

          generic_tokenize_push_string(pending_exact->macro);
          state = DKS_START;
          count = 0;
          count_digits = 0;
          key_len = 0;
          pending_exact = NULL;
          memset(keys, 0, sizeof(keys));
          continue;
        }
      }

      mutt_debug(LL_DEBUG1, "KEY: getch() %s\n", opcodes_get_name(event.op));
      return event;
    }

    // macro op pushed into queue (e.g. from `exec`)
    if (event.op > OP_NULL)
    {
      return event;
    }

    mutt_debug(LL_DEBUG1, "KEY: getch() '%c'\n", isprint(event.ch) ? event.ch : '?');

    if (!(flags & GETCH_NO_COUNTER) && (state != DKS_NEED_MORE) &&
        (event.ch >= '0') && (event.ch <= '9'))
    {
      if (count_digits >= KEY_COUNT_MAX_DIGITS)
      {
        return (struct KeyEvent) { event.ch, OP_NULL, 0 };
      }

      const int digit = event.ch - '0';
      if ((count > (INT_MAX / 10)) ||
          ((count == (INT_MAX / 10)) && (digit > (INT_MAX % 10))))
      {
        return (struct KeyEvent) { event.ch, OP_NULL, 0 };
      }

      count = (count * 10) + digit;
      count_digits++;
      state = DKS_COUNTER;
      continue;
    }

    if (key_len >= KEY_SEQ_MAX_LEN)
    {
      return (struct KeyEvent) { event.ch, OP_NULL, 0 };
    }

    keys[key_len] = event.ch;
    key_len++;

    struct KeymapMatchArray kma = ARRAY_HEAD_INITIALIZER;
    KeyGatherFlags kfg = gather_functions(md, keys, key_len, &kma);

    mutt_debug(LL_DEBUG1, "KEY: flags = %x\n", kfg);

    bool has_exact = false;
    bool has_longer = false;
    pending_exact = NULL;

    struct KeymapMatch *kmatch = NULL;
    ARRAY_FOREACH(kmatch, &kma)
    {
      if (kmatch->flags == KEY_GATHER_MATCH)
      {
        has_exact = true;
        if (!pending_exact)
          pending_exact = kmatch->keymap;
      }
      else if (kmatch->flags == KEY_GATHER_LONGER)
      {
        has_longer = true;
      }
    }

    if (!has_exact && !has_longer)
    {
      mutt_debug(LL_DEBUG1, "KEY: FAIL1: ('%c', %s)\n",
                 isprint(event.ch) ? event.ch : '?', opcodes_get_name(event.op));
      ARRAY_FREE(&kma);
      return event;
    }

    if (has_exact && has_longer)
    {
      state = DKS_NEED_MORE;
      ARRAY_FREE(&kma);
      continue;
    }

    if (!has_exact && has_longer)
    {
      state = DKS_NEED_MORE;
      ARRAY_FREE(&kma);
      continue;
    }

    if (has_exact && pending_exact)
    {
      struct Keymap *map = pending_exact;

      if (map->op != OP_MACRO)
      {
        if ((count_digits > 0) && (count == 0))
        {
          ARRAY_FREE(&kma);
          return (struct KeyEvent) { event.ch, OP_NULL, 0 };
        }

        mutt_debug(LL_DEBUG1, "KEY: SUCCESS: ('%c', %s)\n",
                   isprint(event.ch) ? event.ch : '?', opcodes_get_name(map->op));
        ARRAY_FREE(&kma);
        return (struct KeyEvent) { event.ch, map->op, count_digits > 0 ? count : 0 };
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
        return (struct KeyEvent) { event.ch, OP_NULL, 0 };
      }

      generic_tokenize_push_string(map->macro);
      state = DKS_START;
      count = 0;
      count_digits = 0;
      key_len = 0;
      pending_exact = NULL;
      memset(keys, 0, sizeof(keys));
      ARRAY_FREE(&kma);
      continue;
    }

    ARRAY_FREE(&kma);
  }

  mutt_flushinp();
  mutt_error(_("Macro loop detected"));
  return (struct KeyEvent) { '\0', OP_ABORT, 0 };
}
