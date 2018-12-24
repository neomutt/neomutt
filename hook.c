/**
 * @file
 * Parse and execute user-defined hooks
 *
 * @authors
 * Copyright (C) 1996-2002,2004,2007 Michael R. Elkins <me@mutt.org>, and others
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
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "mutt.h"
#include "hook.h"
#include "alias.h"
#include "context.h"
#include "globals.h"
#include "hdrline.h"
#include "mutt_attach.h"
#include "mutt_commands.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "pattern.h"
#ifdef USE_COMPRESSED
#include "compress.h"
#endif

struct Context;

/* These Config Variables are only used in hook.c */
char *DefaultHook; ///< Config: Pattern to use for hooks that only have a simple regex
bool ForceName; ///< Config: Save outgoing mail in a folder of their name
bool SaveName; ///< Config: Save outgoing message to mailbox of recipient's name if it exists

/**
 * struct Hook - A list of user hooks
 */
struct Hook
{
  int type;                /**< hook type */
  struct Regex regex;      /**< regular expression */
  char *command;           /**< filename, command or pattern to execute */
  struct Pattern *pattern; /**< used for fcc,save,send-hook */
  TAILQ_ENTRY(Hook) entries;
};
static TAILQ_HEAD(, Hook) Hooks = TAILQ_HEAD_INITIALIZER(Hooks);

static int current_hook_type = 0;

/**
 * mutt_parse_hook - Parse the 'hook' family of commands - Implements ::command_t
 *
 * This is used by 'account-hook', 'append-hook' and many more.
 */
enum CommandResult mutt_parse_hook(struct Buffer *buf, struct Buffer *s,
                                   unsigned long data, struct Buffer *err)
{
  struct Hook *ptr = NULL;
  struct Buffer command, pattern;
  int rc;
  bool not = false;
  regex_t *rx = NULL;
  struct Pattern *pat = NULL;
  char path[PATH_MAX];

  mutt_buffer_init(&pattern);
  mutt_buffer_init(&command);

  if (~data & MUTT_GLOBAL_HOOK) /* NOT a global hook */
  {
    if (*s->dptr == '!')
    {
      s->dptr++;
      SKIPWS(s->dptr);
      not = true;
    }

    mutt_extract_token(&pattern, s, 0);

    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), buf->data);
      goto error;
    }
  }

  mutt_extract_token(&command, s,
                     (data & (MUTT_FOLDER_HOOK | MUTT_SEND_HOOK | MUTT_SEND2_HOOK |
                              MUTT_ACCOUNT_HOOK | MUTT_REPLY_HOOK)) ?
                         MUTT_TOKEN_SPACE :
                         0);

  if (!command.data)
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), buf->data);
    goto error;
  }

  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), buf->data);
    goto error;
  }

  if (data & (MUTT_FOLDER_HOOK | MUTT_MBOX_HOOK))
  {
    /* Accidentally using the ^ mailbox shortcut in the .neomuttrc is a
     * common mistake */
    if ((*pattern.data == '^') && (!CurrentFolder))
    {
      mutt_buffer_strcpy(err, _("current mailbox shortcut '^' is unset"));
      goto error;
    }

    mutt_str_strfcpy(path, pattern.data, sizeof(path));
    mutt_expand_path_regex(path, sizeof(path), true);

    /* Check for other mailbox shortcuts that expand to the empty string.
     * This is likely a mistake too */
    if (!*path && *pattern.data)
    {
      mutt_buffer_strcpy(err, _("mailbox shortcut expanded to empty regex"));
      goto error;
    }

    FREE(&pattern.data);
    mutt_buffer_init(&pattern);
    pattern.data = mutt_str_strdup(path);
  }
#ifdef USE_COMPRESSED
  else if (data & (MUTT_APPEND_HOOK | MUTT_OPEN_HOOK | MUTT_CLOSE_HOOK))
  {
    if (mutt_comp_valid_command(command.data) == 0)
    {
      mutt_buffer_strcpy(err, _("badly formatted command string"));
      return MUTT_CMD_ERROR;
    }
  }
#endif
  else if (DefaultHook && (~data & MUTT_GLOBAL_HOOK) &&
           !(data & (MUTT_CHARSET_HOOK | MUTT_ICONV_HOOK | MUTT_ACCOUNT_HOOK)) &&
           (!WithCrypto || !(data & MUTT_CRYPT_HOOK)))
  {
    char tmp[HUGE_STRING];

    /* At this stage remain only message-hooks, reply-hooks, send-hooks,
     * send2-hooks, save-hooks, and fcc-hooks: All those allowing full
     * patterns. If given a simple regex, we expand $default_hook.
     */
    mutt_str_strfcpy(tmp, pattern.data, sizeof(tmp));
    mutt_check_simple(tmp, sizeof(tmp), DefaultHook);
    FREE(&pattern.data);
    mutt_buffer_init(&pattern);
    pattern.data = mutt_str_strdup(tmp);
  }

  if (data & (MUTT_MBOX_HOOK | MUTT_SAVE_HOOK | MUTT_FCC_HOOK))
  {
    mutt_str_strfcpy(path, command.data, sizeof(path));
    mutt_expand_path(path, sizeof(path));
    FREE(&command.data);
    mutt_buffer_init(&command);
    command.data = mutt_str_strdup(path);
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(ptr, &Hooks, entries)
  {
    if (data & MUTT_GLOBAL_HOOK)
    {
      /* Ignore duplicate global hooks */
      if (mutt_str_strcmp(ptr->command, command.data) == 0)
      {
        FREE(&command.data);
        return MUTT_CMD_SUCCESS;
      }
    }
    else if (ptr->type == data &&
             ptr->regex.not == not&&(mutt_str_strcmp(pattern.data, ptr->regex.pattern) == 0))
    {
      if (data & (MUTT_FOLDER_HOOK | MUTT_SEND_HOOK | MUTT_SEND2_HOOK | MUTT_MESSAGE_HOOK |
                  MUTT_ACCOUNT_HOOK | MUTT_REPLY_HOOK | MUTT_CRYPT_HOOK |
                  MUTT_TIMEOUT_HOOK | MUTT_STARTUP_HOOK | MUTT_SHUTDOWN_HOOK))
      {
        /* these hooks allow multiple commands with the same
         * pattern, so if we've already seen this pattern/command pair, just
         * ignore it instead of creating a duplicate */
        if (mutt_str_strcmp(ptr->command, command.data) == 0)
        {
          FREE(&command.data);
          FREE(&pattern.data);
          return MUTT_CMD_SUCCESS;
        }
      }
      else
      {
        /* other hooks only allow one command per pattern, so update the
         * entry with the new command.  this currently does not change the
         * order of execution of the hooks, which i think is desirable since
         * a common action to perform is to change the default (.) entry
         * based upon some other information. */
        FREE(&ptr->command);
        ptr->command = command.data;
        FREE(&pattern.data);
        return MUTT_CMD_SUCCESS;
      }
    }
  }

  if (data & (MUTT_CHARSET_HOOK | MUTT_ICONV_HOOK))
  {
    /* These are managed separately by the charset code */
    enum LookupType type = (data & MUTT_CHARSET_HOOK) ? MUTT_LOOKUP_CHARSET : MUTT_LOOKUP_ICONV;
    if (!mutt_ch_lookup_add(type, pattern.data, command.data, err))
      goto error;
    FREE(&pattern.data);
    FREE(&command.data);
    return MUTT_CMD_SUCCESS;
  }
  else if (data & (MUTT_SEND_HOOK | MUTT_SEND2_HOOK | MUTT_SAVE_HOOK |
                   MUTT_FCC_HOOK | MUTT_MESSAGE_HOOK | MUTT_REPLY_HOOK))
  {
    pat = mutt_pattern_comp(
        pattern.data,
        (data & (MUTT_SEND_HOOK | MUTT_SEND2_HOOK | MUTT_FCC_HOOK)) ? 0 : MUTT_FULL_MSG, err);
    if (!pat)
      goto error;
  }
  else if (~data & MUTT_GLOBAL_HOOK) /* NOT a global hook */
  {
    /* Hooks not allowing full patterns: Check syntax of regex */
    rx = mutt_mem_malloc(sizeof(regex_t));
    rc = REGCOMP(rx, NONULL(pattern.data), ((data & MUTT_CRYPT_HOOK) ? REG_ICASE : 0));
    if (rc != 0)
    {
      regerror(rc, rx, err->data, err->dsize);
      FREE(&rx);
      goto error;
    }
  }

  ptr = mutt_mem_calloc(1, sizeof(struct Hook));
  ptr->type = data;
  ptr->command = command.data;
  ptr->pattern = pat;
  ptr->regex.pattern = pattern.data;
  ptr->regex.regex = rx;
  ptr->regex.not = not;
  TAILQ_INSERT_TAIL(&Hooks, ptr, entries);
  return MUTT_CMD_SUCCESS;

error:
  if (~data & MUTT_GLOBAL_HOOK) /* NOT a global hook */
    FREE(&pattern.data);
  FREE(&command.data);
  return MUTT_CMD_ERROR;
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
 * @param type Hook type to delete, e.g. #MUTT_SEND_HOOK
 *
 * If 0 is passed, all the hooks will be deleted.
 */
void mutt_delete_hooks(int type)
{
  struct Hook *h = NULL;
  struct Hook *tmp = NULL;

  TAILQ_FOREACH_SAFE(h, &Hooks, entries, tmp)
  {
    if (type == 0 || type == h->type)
    {
      TAILQ_REMOVE(&Hooks, h, entries);
      delete_hook(h);
    }
  }
}

/**
 * mutt_parse_unhook - Parse the 'unhook' command - Implements ::command_t
 */
enum CommandResult mutt_parse_unhook(struct Buffer *buf, struct Buffer *s,
                                     unsigned long data, struct Buffer *err)
{
  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, 0);
    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      if (current_hook_type)
      {
        mutt_buffer_printf(err, "%s", _("unhook: Can't do unhook * from within a hook"));
        return MUTT_CMD_ERROR;
      }
      mutt_delete_hooks(0);
      mutt_ch_lookup_remove();
    }
    else
    {
      int type = mutt_get_hook_type(buf->data);

      if (!type)
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
        return MUTT_CMD_ERROR;
      }
      mutt_delete_hooks(type);
    }
  }
  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_folder_hook - Perform a folder hook
 * @param path Path to match
 */
void mutt_folder_hook(const char *path)
{
  struct Hook *tmp = NULL;
  struct Buffer err, token;

  current_hook_type = MUTT_FOLDER_HOOK;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  mutt_buffer_init(&token);
  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if (!tmp->command)
      continue;

    if (tmp->type & MUTT_FOLDER_HOOK)
    {
      if ((regexec(tmp->regex.regex, path, 0, NULL, 0) == 0) ^ tmp->regex.not)
      {
        if (mutt_parse_rc_line(tmp->command, &token, &err) == MUTT_CMD_ERROR)
        {
          mutt_error("%s", err.data);
          FREE(&token.data);
          current_hook_type = 0;
          FREE(&err.data);

          return;
        }
      }
    }
  }
  FREE(&token.data);
  FREE(&err.data);

  current_hook_type = 0;
}

/**
 * mutt_find_hook - Find a matching hook
 * @param type Type, e.g. #MUTT_FOLDER_HOOK
 * @param pat  Pattern to match
 * @retval ptr Command string
 *
 * @note The returned string must not be freed.
 */
char *mutt_find_hook(int type, const char *pat)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if (tmp->type & type)
    {
      if (regexec(tmp->regex.regex, pat, 0, NULL, 0) == 0)
        return tmp->command;
    }
  }
  return NULL;
}

/**
 * mutt_message_hook - Perform a message hook
 * @param m   Mailbox Context
 * @param e   Email
 * @param type Hook type, e.g. #MUTT_MESSAGE_HOOK
 */
void mutt_message_hook(struct Mailbox *m, struct Email *e, int type)
{
  struct Buffer err, token;
  struct Hook *hook = NULL;
  struct PatternCache cache = { 0 };

  current_hook_type = type;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  mutt_buffer_init(&token);
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->type & type)
    {
      if ((mutt_pattern_exec(hook->pattern, 0, m, e, &cache) > 0) ^ hook->regex.not)
      {
        if (mutt_parse_rc_line(hook->command, &token, &err) == MUTT_CMD_ERROR)
        {
          FREE(&token.data);
          mutt_error("%s", err.data);
          current_hook_type = 0;
          FREE(&err.data);

          return;
        }
        /* Executing arbitrary commands could affect the pattern results,
         * so the cache has to be wiped */
        memset(&cache, 0, sizeof(cache));
      }
    }
  }
  FREE(&token.data);
  FREE(&err.data);

  current_hook_type = 0;
}

/**
 * addr_hook - Perform an address hook (get a path)
 * @param path    Buffer for path
 * @param pathlen Length of buffer
 * @param type    Type e.g. #MUTT_FCC_HOOK
 * @param ctx     Mailbox Context
 * @param e       Email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int addr_hook(char *path, size_t pathlen, int type, struct Context *ctx,
                     struct Email *e)
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
      if ((mutt_pattern_exec(hook->pattern, 0, m, e, &cache) > 0) ^ hook->regex.not)
      {
        mutt_make_string_flags(path, pathlen, hook->command, ctx, e, MUTT_FORMAT_PLAIN);
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

  struct Address *addr = NULL;
  struct Envelope *env = e->env;
  bool from_me = mutt_addr_is_user(env->from);

  if (!from_me && env->reply_to && env->reply_to->mailbox)
    addr = env->reply_to;
  else if (!from_me && env->from && env->from->mailbox)
    addr = env->from;
  else if (env->to && env->to->mailbox)
    addr = env->to;
  else if (env->cc && env->cc->mailbox)
    addr = env->cc;
  else
    addr = NULL;
  if (addr)
  {
    char tmp[PATH_MAX];
    mutt_safe_path(tmp, sizeof(tmp), addr);
    snprintf(path, pathlen, "=%s", tmp);
  }
}

/**
 * mutt_select_fcc - Select the FCC path for an email
 * @param path    Buffer for the path
 * @param pathlen Length of the buffer
 * @param e       Email
 */
void mutt_select_fcc(char *path, size_t pathlen, struct Email *e)
{
  if (addr_hook(path, pathlen, MUTT_FCC_HOOK, NULL, e) != 0)
  {
    struct Envelope *env = e->env;
    if ((SaveName || ForceName) && (env->to || env->cc || env->bcc))
    {
      struct Address *addr = env->to ? env->to : (env->cc ? env->cc : env->bcc);
      char buf[PATH_MAX];
      mutt_safe_path(buf, sizeof(buf), addr);
      mutt_path_concat(path, NONULL(Folder), buf, pathlen);
      if (!ForceName && mx_access(path, W_OK) != 0)
        mutt_str_strfcpy(path, Record, pathlen);
    }
    else
      mutt_str_strfcpy(path, Record, pathlen);
  }
  mutt_pretty_mailbox(path, pathlen);
}

/**
 * list_hook - Find hook strings matching
 * @param[out] matches List of hook strings
 * @param[in]  match   String to match
 * @param[in]  hook    Hook type, e.g. #MUTT_CRYPT_HOOK
 */
static void list_hook(struct ListHead *matches, const char *match, int hook)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if ((tmp->type & hook) && ((match && regexec(tmp->regex.regex, match, 0, NULL, 0) == 0) ^
                               tmp->regex.not))
    {
      mutt_list_insert_tail(matches, mutt_str_strdup(tmp->command));
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

#ifdef USE_SOCKET
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

  struct Hook *hook = NULL;
  struct Buffer token;
  struct Buffer err;

  if (inhook)
    return;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  mutt_buffer_init(&token);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & MUTT_ACCOUNT_HOOK)))
      continue;

    if ((regexec(hook->regex.regex, url, 0, NULL, 0) == 0) ^ hook->regex.not)
    {
      inhook = true;

      if (mutt_parse_rc_line(hook->command, &token, &err) == MUTT_CMD_ERROR)
      {
        FREE(&token.data);
        mutt_error("%s", err.data);
        FREE(&err.data);

        inhook = false;
        return;
      }

      inhook = false;
    }
  }

  FREE(&token.data);
  FREE(&err.data);
}
#endif

/**
 * mutt_timeout_hook - Execute any timeout hooks
 *
 * The user can configure hooks to be run on timeout.
 * This function finds all the matching hooks and executes them.
 */
void mutt_timeout_hook(void)
{
  struct Hook *hook = NULL;
  struct Buffer token;
  struct Buffer err;
  char buf[STRING];

  mutt_buffer_init(&err);
  err.data = buf;
  err.dsize = sizeof(buf);
  mutt_buffer_init(&token);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & MUTT_TIMEOUT_HOOK)))
      continue;

    if (mutt_parse_rc_line(hook->command, &token, &err) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", err.data);
      mutt_buffer_reset(&err);

      /* The hooks should be independent of each other, so even though this on
       * failed, we'll carry on with the others. */
    }
  }
  FREE(&token.data);

  /* Delete temporary attachment files */
  mutt_unlink_temp_attachments();
}

/**
 * mutt_startup_shutdown_hook - Execute any startup/shutdown hooks
 * @param type Hook type: MUTT_STARTUP_HOOK or MUTT_SHUTDOWN_HOOK
 *
 * The user can configure hooks to be run on startup/shutdown.
 * This function finds all the matching hooks and executes them.
 */
void mutt_startup_shutdown_hook(int type)
{
  struct Hook *hook = NULL;
  struct Buffer token = { 0 };
  struct Buffer err = { 0 };
  char buf[STRING];

  err.data = buf;
  err.dsize = sizeof(buf);
  mutt_buffer_init(&token);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & type)))
      continue;

    if (mutt_parse_rc_line(hook->command, &token, &err) == MUTT_CMD_ERROR)
    {
      mutt_error("%s", err.data);
      mutt_buffer_reset(&err);
    }
  }
  FREE(&token.data);
}
