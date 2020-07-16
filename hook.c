/**
 * @file
 * Parse and execute user-defined hooks
 *
 * @authors
 * Copyright (C) 1996-2002,2004,2007 Michael R. Elkins <me@mutt.org>, and others
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page hook Parse and execute user-defined hooks
 *
 * Parse and execute user-defined hooks
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "alias/lib.h"
#include "mutt.h"
#include "hook.h"
#include "context.h"
#include "format_flags.h"
#include "hdrline.h"
#include "init.h"
#include "mutt_attach.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/lib.h"
#include "pattern/lib.h"
#ifdef USE_COMP_MBOX
#include "compmbox/lib.h"
#endif

/* These Config Variables are only used in hook.c */
char *C_DefaultHook; ///< Config: Pattern to use for hooks that only have a simple regex
bool C_ForceName; ///< Config: Save outgoing mail in a folder of their name
bool C_SaveName; ///< Config: Save outgoing message to mailbox of recipient's name if it exists

/**
 * struct Hook - A list of user hooks
 */
struct Hook
{
  HookFlags type;              ///< Hook type
  struct Regex regex;          ///< Regular expression
  char *command;               ///< Filename, command or pattern to execute
  struct PatternList *pattern; ///< Used for fcc,save,send-hook
  TAILQ_ENTRY(Hook) entries;   ///< Linked list
};
TAILQ_HEAD(HookList, Hook);

static struct HookList Hooks = TAILQ_HEAD_INITIALIZER(Hooks);

static struct HashTable *IdxFmtHooks = NULL;
static HookFlags current_hook_type = MUTT_HOOK_NO_FLAGS;

/**
 * mutt_parse_hook - Parse the 'hook' family of commands - Implements Command::parse()
 *
 * This is used by 'account-hook', 'append-hook' and many more.
 */
enum CommandResult mutt_parse_hook(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  struct Hook *hook = NULL;
  int rc = MUTT_CMD_ERROR;
  bool pat_not = false;
  regex_t *rx = NULL;
  struct PatternList *pat = NULL;

  struct Buffer *cmd = mutt_buffer_pool_get();
  struct Buffer *pattern = mutt_buffer_pool_get();

  if (~data & MUTT_GLOBAL_HOOK) /* NOT a global hook */
  {
    if (*s->dptr == '!')
    {
      s->dptr++;
      SKIPWS(s->dptr);
      pat_not = true;
    }

    mutt_extract_token(pattern, s, MUTT_TOKEN_NO_FLAGS);

    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), buf->data);
      rc = MUTT_CMD_WARNING;
      goto cleanup;
    }
  }

  mutt_extract_token(cmd, s,
                     (data & (MUTT_FOLDER_HOOK | MUTT_SEND_HOOK | MUTT_SEND2_HOOK |
                              MUTT_ACCOUNT_HOOK | MUTT_REPLY_HOOK)) ?
                         MUTT_TOKEN_SPACE :
                         MUTT_TOKEN_NO_FLAGS);

  if (mutt_buffer_is_empty(cmd))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), buf->data);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), buf->data);
    rc = MUTT_CMD_WARNING;
    goto cleanup;
  }

  if (data & (MUTT_FOLDER_HOOK | MUTT_MBOX_HOOK))
  {
    /* Accidentally using the ^ mailbox shortcut in the .neomuttrc is a
     * common mistake */
    if ((pattern->data[0] == '^') && !CurrentFolder)
    {
      mutt_buffer_strcpy(err, _("current mailbox shortcut '^' is unset"));
      goto cleanup;
    }

    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_buffer_copy(tmp, pattern);
    mutt_buffer_expand_path_regex(tmp, true);

    /* Check for other mailbox shortcuts that expand to the empty string.
     * This is likely a mistake too */
    if (mutt_buffer_is_empty(tmp) && !mutt_buffer_is_empty(pattern))
    {
      mutt_buffer_strcpy(err, _("mailbox shortcut expanded to empty regex"));
      mutt_buffer_pool_release(&tmp);
      goto cleanup;
    }

    mutt_buffer_copy(pattern, tmp);
    mutt_buffer_pool_release(&tmp);
  }
#ifdef USE_COMP_MBOX
  else if (data & (MUTT_APPEND_HOOK | MUTT_OPEN_HOOK | MUTT_CLOSE_HOOK))
  {
    if (mutt_comp_valid_command(mutt_b2s(cmd)) == 0)
    {
      mutt_buffer_strcpy(err, _("badly formatted command string"));
      goto cleanup;
    }
  }
#endif
  else if (C_DefaultHook && (~data & MUTT_GLOBAL_HOOK) &&
           !(data & (MUTT_CHARSET_HOOK | MUTT_ICONV_HOOK | MUTT_ACCOUNT_HOOK)) &&
           (!WithCrypto || !(data & MUTT_CRYPT_HOOK)))
  {
    /* At this stage remain only message-hooks, reply-hooks, send-hooks,
     * send2-hooks, save-hooks, and fcc-hooks: All those allowing full
     * patterns. If given a simple regex, we expand $default_hook.  */
    mutt_check_simple(pattern, C_DefaultHook);
  }

  if (data & (MUTT_MBOX_HOOK | MUTT_SAVE_HOOK | MUTT_FCC_HOOK))
  {
    mutt_buffer_expand_path(cmd);
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (data & MUTT_GLOBAL_HOOK)
    {
      /* Ignore duplicate global hooks */
      if (mutt_str_equal(hook->command, mutt_b2s(cmd)))
      {
        rc = MUTT_CMD_SUCCESS;
        goto cleanup;
      }
    }
    else if ((hook->type == data) && (hook->regex.pat_not == pat_not) &&
             mutt_str_equal(mutt_b2s(pattern), hook->regex.pattern))
    {
      if (data & (MUTT_FOLDER_HOOK | MUTT_SEND_HOOK | MUTT_SEND2_HOOK | MUTT_MESSAGE_HOOK |
                  MUTT_ACCOUNT_HOOK | MUTT_REPLY_HOOK | MUTT_CRYPT_HOOK |
                  MUTT_TIMEOUT_HOOK | MUTT_STARTUP_HOOK | MUTT_SHUTDOWN_HOOK))
      {
        /* these hooks allow multiple commands with the same
         * pattern, so if we've already seen this pattern/command pair, just
         * ignore it instead of creating a duplicate */
        if (mutt_str_equal(hook->command, mutt_b2s(cmd)))
        {
          rc = MUTT_CMD_SUCCESS;
          goto cleanup;
        }
      }
      else
      {
        /* other hooks only allow one command per pattern, so update the
         * entry with the new command.  this currently does not change the
         * order of execution of the hooks, which i think is desirable since
         * a common action to perform is to change the default (.) entry
         * based upon some other information. */
        FREE(&hook->command);
        hook->command = mutt_buffer_strdup(cmd);
        rc = MUTT_CMD_SUCCESS;
        goto cleanup;
      }
    }
  }

  if (data & (MUTT_CHARSET_HOOK | MUTT_ICONV_HOOK))
  {
    /* These are managed separately by the charset code */
    enum LookupType type = (data & MUTT_CHARSET_HOOK) ? MUTT_LOOKUP_CHARSET : MUTT_LOOKUP_ICONV;
    if (mutt_ch_lookup_add(type, mutt_b2s(pattern), mutt_b2s(cmd), err))
      rc = MUTT_CMD_SUCCESS;
    goto cleanup;
  }
  else if (data & (MUTT_SEND_HOOK | MUTT_SEND2_HOOK | MUTT_SAVE_HOOK |
                   MUTT_FCC_HOOK | MUTT_MESSAGE_HOOK | MUTT_REPLY_HOOK))
  {
    PatternCompFlags comp_flags;

    if (data & (MUTT_SEND2_HOOK))
      comp_flags = MUTT_PC_SEND_MODE_SEARCH;
    else if (data & (MUTT_SEND_HOOK | MUTT_FCC_HOOK))
      comp_flags = MUTT_PC_NO_FLAGS;
    else
      comp_flags = MUTT_PC_FULL_MSG;

    pat = mutt_pattern_comp(mutt_b2s(pattern), comp_flags, err);
    if (!pat)
      goto cleanup;
  }
  else if (~data & MUTT_GLOBAL_HOOK) /* NOT a global hook */
  {
    /* Hooks not allowing full patterns: Check syntax of regex */
    rx = mutt_mem_malloc(sizeof(regex_t));
    int rc2 = REG_COMP(rx, NONULL(mutt_b2s(pattern)),
                       ((data & MUTT_CRYPT_HOOK) ? REG_ICASE : 0));
    if (rc2 != 0)
    {
      regerror(rc2, rx, err->data, err->dsize);
      FREE(&rx);
      goto cleanup;
    }
  }

  hook = mutt_mem_calloc(1, sizeof(struct Hook));
  hook->type = data;
  hook->command = mutt_buffer_strdup(cmd);
  hook->pattern = pat;
  hook->regex.pattern = mutt_buffer_strdup(pattern);
  hook->regex.regex = rx;
  hook->regex.pat_not = pat_not;
  TAILQ_INSERT_TAIL(&Hooks, hook, entries);
  rc = MUTT_CMD_SUCCESS;

cleanup:
  mutt_buffer_pool_release(&cmd);
  mutt_buffer_pool_release(&pattern);
  return rc;
}

/**
 * delete_hook - Delete a Hook
 * @param h Hook to delete
 */
static void delete_hook(struct Hook *h)
{
  FREE(&h->command);
  FREE(&h->regex.pattern);
  if (h->regex.regex)
  {
    regfree(h->regex.regex);
    FREE(&h->regex.regex);
  }
  mutt_pattern_free(&h->pattern);
  FREE(&h);
}

/**
 * mutt_delete_hooks - Delete matching hooks
 * @param type Hook type to delete, see #HookFlags
 *
 * If 0 is passed, all the hooks will be deleted.
 */
void mutt_delete_hooks(HookFlags type)
{
  struct Hook *h = NULL;
  struct Hook *tmp = NULL;

  TAILQ_FOREACH_SAFE(h, &Hooks, entries, tmp)
  {
    if ((type == MUTT_HOOK_NO_FLAGS) || (type == h->type))
    {
      TAILQ_REMOVE(&Hooks, h, entries);
      delete_hook(h);
    }
  }
}

/**
 * delete_idxfmt_hooklist - Delete a index-format-hook from the Hash Table - Implements ::hash_hdata_free_t
 */
static void delete_idxfmt_hooklist(int type, void *obj, intptr_t data)
{
  struct HookList *hl = obj;
  struct Hook *h = NULL;
  struct Hook *tmp = NULL;

  TAILQ_FOREACH_SAFE(h, hl, entries, tmp)
  {
    TAILQ_REMOVE(hl, h, entries);
    delete_hook(h);
  }

  FREE(&hl);
}

/**
 * delete_idxfmt_hooks - Delete all the index-format-hooks
 */
static void delete_idxfmt_hooks(void)
{
  mutt_hash_free(&IdxFmtHooks);
}

/**
 * mutt_parse_idxfmt_hook - Parse the 'index-format-hook' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_idxfmt_hook(struct Buffer *buf, struct Buffer *s,
                                          intptr_t data, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_ERROR;
  bool pat_not = false;

  struct Buffer *name = mutt_buffer_pool_get();
  struct Buffer *pattern = mutt_buffer_pool_get();
  struct Buffer *fmtstring = mutt_buffer_pool_get();

  if (!IdxFmtHooks)
  {
    IdxFmtHooks = mutt_hash_new(30, MUTT_HASH_STRDUP_KEYS);
    mutt_hash_set_destructor(IdxFmtHooks, delete_idxfmt_hooklist, 0);
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), buf->data);
    goto out;
  }
  mutt_extract_token(name, s, MUTT_TOKEN_NO_FLAGS);
  struct HookList *hl = mutt_hash_find(IdxFmtHooks, mutt_b2s(name));

  if (*s->dptr == '!')
  {
    s->dptr++;
    SKIPWS(s->dptr);
    pat_not = true;
  }
  mutt_extract_token(pattern, s, MUTT_TOKEN_NO_FLAGS);

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), buf->data);
    goto out;
  }
  mutt_extract_token(fmtstring, s, MUTT_TOKEN_NO_FLAGS);

  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), buf->data);
    goto out;
  }

  if (C_DefaultHook)
    mutt_check_simple(pattern, C_DefaultHook);

  /* check to make sure that a matching hook doesn't already exist */
  struct Hook *hook = NULL;
  if (hl)
  {
    TAILQ_FOREACH(hook, hl, entries)
    {
      if ((hook->regex.pat_not == pat_not) &&
          mutt_str_equal(mutt_b2s(pattern), hook->regex.pattern))
      {
        mutt_str_replace(&hook->command, mutt_b2s(fmtstring));
        rc = MUTT_CMD_SUCCESS;
        goto out;
      }
    }
  }

  /* MUTT_PC_PATTERN_DYNAMIC sets so that date ranges are regenerated during
   * matching.  This of course is slower, but index-format-hook is commonly
   * used for date ranges, and they need to be evaluated relative to "now", not
   * the hook compilation time.  */
  struct PatternList *pat = mutt_pattern_comp(
      mutt_b2s(pattern), MUTT_PC_FULL_MSG | MUTT_PC_PATTERN_DYNAMIC, err);
  if (!pat)
    goto out;

  hook = mutt_mem_calloc(1, sizeof(struct Hook));
  hook->type = MUTT_IDXFMTHOOK;
  hook->command = mutt_buffer_strdup(fmtstring);
  hook->pattern = pat;
  hook->regex.pattern = mutt_buffer_strdup(pattern);
  hook->regex.regex = NULL;
  hook->regex.pat_not = pat_not;

  if (!hl)
  {
    hl = mutt_mem_calloc(1, sizeof(*hl));
    TAILQ_INIT(hl);
    mutt_hash_insert(IdxFmtHooks, mutt_b2s(name), hl);
  }

  TAILQ_INSERT_TAIL(hl, hook, entries);
  rc = MUTT_CMD_SUCCESS;

out:
  mutt_buffer_pool_release(&name);
  mutt_buffer_pool_release(&pattern);
  mutt_buffer_pool_release(&fmtstring);

  return rc;
}

/**
 * mutt_parse_unhook - Parse the 'unhook' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_unhook(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
    {
      if (current_hook_type != MUTT_TOKEN_NO_FLAGS)
      {
        mutt_buffer_printf(err, "%s", _("unhook: Can't do unhook * from within a hook"));
        return MUTT_CMD_WARNING;
      }
      mutt_delete_hooks(MUTT_HOOK_NO_FLAGS);
      delete_idxfmt_hooks();
      mutt_ch_lookup_remove();
    }
    else
    {
      HookFlags type = mutt_get_hook_type(buf->data);

      if (type == MUTT_HOOK_NO_FLAGS)
      {
        mutt_buffer_printf(err, _("unhook: unknown hook type: %s"), buf->data);
        return MUTT_CMD_ERROR;
      }
      if (type & (MUTT_CHARSET_HOOK | MUTT_ICONV_HOOK))
      {
        mutt_ch_lookup_remove();
        return MUTT_CMD_SUCCESS;
      }
      if (current_hook_type == type)
      {
        mutt_buffer_printf(err, _("unhook: Can't delete a %s from within a %s"),
                           buf->data, buf->data);
        return MUTT_CMD_WARNING;
      }
      if (type == MUTT_IDXFMTHOOK)
        delete_idxfmt_hooks();
      else
        mutt_delete_hooks(type);
    }
  }
  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_folder_hook - Perform a folder hook
 * @param path Path to potentially match
 * @param desc Description to potentially match
 */
void mutt_folder_hook(const char *path, const char *desc)
{
  if (!path && !desc)
    return;

  struct Hook *hook = NULL;
  struct Buffer *err = mutt_buffer_pool_get();

  current_hook_type = MUTT_FOLDER_HOOK;

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (!(hook->type & MUTT_FOLDER_HOOK))
      continue;

    const char *match = NULL;
    if (mutt_regex_match(&hook->regex, path))
      match = path;
    else if (mutt_regex_match(&hook->regex, desc))
      match = desc;

    if (match)
    {
      mutt_debug(LL_DEBUG1, "folder-hook '%s' matches '%s'\n", hook->regex.pattern, match);
      mutt_debug(LL_DEBUG5, "    %s\n", hook->command);
      if (mutt_parse_rc_line(hook->command, err) == MUTT_CMD_ERROR)
      {
        mutt_error("%s", mutt_b2s(err));
        break;
      }
    }
  }
  mutt_buffer_pool_release(&err);

  current_hook_type = MUTT_HOOK_NO_FLAGS;
}

/**
 * mutt_find_hook - Find a matching hook
 * @param type Hook type, see #HookFlags
 * @param pat  Pattern to match
 * @retval ptr Command string
 *
 * @note The returned string must not be freed.
 */
char *mutt_find_hook(HookFlags type, const char *pat)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if (tmp->type & type)
    {
      if (mutt_regex_match(&tmp->regex, pat))
        return tmp->command;
    }
  }
  return NULL;
}

/**
 * mutt_message_hook - Perform a message hook
 * @param m   Mailbox Context
 * @param e   Email
 * @param type Hook type, see #HookFlags
 */
void mutt_message_hook(struct Mailbox *m, struct Email *e, HookFlags type)
{
  struct Hook *hook = NULL;
  struct PatternCache cache = { 0 };
  struct Buffer *err = mutt_buffer_pool_get();

  current_hook_type = type;

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->type & type)
    {
      if ((mutt_pattern_exec(SLIST_FIRST(hook->pattern), 0, m, e, &cache) > 0) ^
          hook->regex.pat_not)
      {
        if (mutt_parse_rc_line(hook->command, err) == MUTT_CMD_ERROR)
        {
          mutt_error("%s", mutt_b2s(err));
          current_hook_type = MUTT_HOOK_NO_FLAGS;
          mutt_buffer_pool_release(&err);

          return;
        }
        /* Executing arbitrary commands could affect the pattern results,
         * so the cache has to be wiped */
        memset(&cache, 0, sizeof(cache));
      }
    }
  }
  mutt_buffer_pool_release(&err);

  current_hook_type = MUTT_HOOK_NO_FLAGS;
}

/**
 * addr_hook - Perform an address hook (get a path)
 * @param path    Buffer for path
 * @param pathlen Length of buffer
 * @param type    Hook type, see #HookFlags
 * @param ctx     Mailbox Context
 * @param e       Email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int addr_hook(char *path, size_t pathlen, HookFlags type,
                     struct Context *ctx, struct Email *e)
{
  struct Hook *hook = NULL;
  struct PatternCache cache = { 0 };

  /* determine if a matching hook exists */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->type & type)
    {
      struct Mailbox *m = ctx ? ctx->mailbox : NULL;
      if ((mutt_pattern_exec(SLIST_FIRST(hook->pattern), 0, m, e, &cache) > 0) ^
          hook->regex.pat_not)
      {
        mutt_make_string_flags(path, pathlen, 0, hook->command, ctx, m, e, MUTT_FORMAT_PLAIN);
        return 0;
      }
    }
  }

  return -1;
}

/**
 * mutt_default_save - Find the default save path for an email
 * @param path    Buffer for the path
 * @param pathlen Length of buffer
 * @param e       Email
 */
void mutt_default_save(char *path, size_t pathlen, struct Email *e)
{
  *path = '\0';
  if (addr_hook(path, pathlen, MUTT_SAVE_HOOK, Context, e) == 0)
    return;

  struct Envelope *env = e->env;
  const struct Address *from = TAILQ_FIRST(&env->from);
  const struct Address *reply_to = TAILQ_FIRST(&env->reply_to);
  const struct Address *to = TAILQ_FIRST(&env->to);
  const struct Address *cc = TAILQ_FIRST(&env->cc);
  const struct Address *addr = NULL;
  bool from_me = mutt_addr_is_user(from);

  if (!from_me && reply_to && reply_to->mailbox)
    addr = reply_to;
  else if (!from_me && from && from->mailbox)
    addr = from;
  else if (to && to->mailbox)
    addr = to;
  else if (cc && cc->mailbox)
    addr = cc;
  else
    addr = NULL;
  if (addr)
  {
    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_safe_path(tmp, addr);
    snprintf(path, pathlen, "=%s", mutt_b2s(tmp));
    mutt_buffer_pool_release(&tmp);
  }
}

/**
 * mutt_select_fcc - Select the FCC path for an email
 * @param path    Buffer for the path
 * @param e       Email
 */
void mutt_select_fcc(struct Buffer *path, struct Email *e)
{
  mutt_buffer_alloc(path, PATH_MAX);

  if (addr_hook(path->data, path->dsize, MUTT_FCC_HOOK, NULL, e) != 0)
  {
    const struct Address *to = TAILQ_FIRST(&e->env->to);
    const struct Address *cc = TAILQ_FIRST(&e->env->cc);
    const struct Address *bcc = TAILQ_FIRST(&e->env->bcc);
    if ((C_SaveName || C_ForceName) && (to || cc || bcc))
    {
      const struct Address *addr = to ? to : (cc ? cc : bcc);
      struct Buffer *buf = mutt_buffer_pool_get();
      mutt_safe_path(buf, addr);
      mutt_buffer_concat_path(path, NONULL(C_Folder), mutt_b2s(buf));
      mutt_buffer_pool_release(&buf);
      if (!C_ForceName && (mx_access(mutt_b2s(path), W_OK) != 0))
        mutt_buffer_strcpy(path, C_Record);
    }
    else
      mutt_buffer_strcpy(path, C_Record);
  }
  else
    mutt_buffer_fix_dptr(path);

  mutt_buffer_pretty_mailbox(path);
}

/**
 * list_hook - Find hook strings matching
 * @param[out] matches List of hook strings
 * @param[in]  match   String to match
 * @param[in]  hook    Hook type, see #HookFlags
 */
static void list_hook(struct ListHead *matches, const char *match, HookFlags hook)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if ((tmp->type & hook) && mutt_regex_match(&tmp->regex, match))
    {
      mutt_list_insert_tail(matches, mutt_str_dup(tmp->command));
    }
  }
}

/**
 * mutt_crypt_hook - Find crypto hooks for an Address
 * @param[out] list List of keys
 * @param[in]  addr Address to match
 *
 * The crypt-hook associates keys with addresses.
 */
void mutt_crypt_hook(struct ListHead *list, struct Address *addr)
{
  list_hook(list, addr->mailbox, MUTT_CRYPT_HOOK);
}

/**
 * mutt_account_hook - Perform an account hook
 * @param url Account URL to match
 */
void mutt_account_hook(const char *url)
{
  /* parsing commands with URLs in an account hook can cause a recursive
   * call. We just skip processing if this occurs. Typically such commands
   * belong in a folder-hook -- perhaps we should warn the user. */
  static bool inhook = false;
  if (inhook)
    return;

  struct Hook *hook = NULL;
  struct Buffer *err = mutt_buffer_pool_get();

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & MUTT_ACCOUNT_HOOK)))
      continue;

    if (mutt_regex_match(&hook->regex, url))
    {
      inhook = true;
      mutt_debug(LL_DEBUG1, "account-hook '%s' matches '%s'\n", hook->regex.pattern, url);
      mutt_debug(LL_DEBUG5, "    %s\n", hook->command);

      if (mutt_parse_rc_line(hook->command, err) == MUTT_CMD_ERROR)
      {
        mutt_error("%s", mutt_b2s(err));
        mutt_buffer_pool_release(&err);

        inhook = false;
        goto done;
      }

      inhook = false;
    }
  }
done:
  mutt_buffer_pool_release(&err);
}

/**
 * mutt_timeout_hook - Execute any timeout hooks
 *
 * The user can configure hooks to be run on timeout.
 * This function finds all the matching hooks and executes them.
 */
void mutt_timeout_hook(void)
{
  struct Hook *hook = NULL;
  struct Buffer err;
  char buf[256];

  mutt_buffer_init(&err);
  err.data = buf;
  err.dsize = sizeof(buf);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & MUTT_TIMEOUT_HOOK)))
      continue;

    if (mutt_parse_rc_line(hook->command, &err) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", err.data);
      mutt_buffer_reset(&err);

      /* The hooks should be independent of each other, so even though this on
       * failed, we'll carry on with the others. */
    }
  }

  /* Delete temporary attachment files */
  mutt_unlink_temp_attachments();
}

/**
 * mutt_startup_shutdown_hook - Execute any startup/shutdown hooks
 * @param type Hook type: #MUTT_STARTUP_HOOK or #MUTT_SHUTDOWN_HOOK
 *
 * The user can configure hooks to be run on startup/shutdown.
 * This function finds all the matching hooks and executes them.
 */
void mutt_startup_shutdown_hook(HookFlags type)
{
  struct Hook *hook = NULL;
  struct Buffer err = mutt_buffer_make(0);
  char buf[256];

  err.data = buf;
  err.dsize = sizeof(buf);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & type)))
      continue;

    if (mutt_parse_rc_line(hook->command, &err) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", err.data);
      mutt_buffer_reset(&err);
    }
  }
}

/**
 * mutt_idxfmt_hook - Get index-format-hook format string
 * @param name Hook name
 * @param m    Mailbox
 * @param e    Email
 * @retval ptr  printf(3)-like format string
 * @retval NULL No matching hook
 */
const char *mutt_idxfmt_hook(const char *name, struct Mailbox *m, struct Email *e)
{
  if (!IdxFmtHooks)
    return NULL;

  struct HookList *hl = mutt_hash_find(IdxFmtHooks, name);
  if (!hl)
    return NULL;

  current_hook_type = MUTT_IDXFMTHOOK;

  struct PatternCache cache = { 0 };
  const char *fmtstring = NULL;
  struct Hook *hook = NULL;

  TAILQ_FOREACH(hook, hl, entries)
  {
    struct Pattern *pat = SLIST_FIRST(hook->pattern);
    if ((mutt_pattern_exec(pat, 0, m, e, &cache) > 0) ^ hook->regex.pat_not)
    {
      fmtstring = hook->command;
      break;
    }
  }

  current_hook_type = MUTT_HOOK_NO_FLAGS;

  return fmtstring;
}
