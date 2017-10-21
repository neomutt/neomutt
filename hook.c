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

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/lib.h"
#include "mutt.h"
#include "address.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mailbox.h"
#include "mutt_regex.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#ifdef USE_COMPRESSED
#include "compress.h"
#endif

/**
 * struct Hook - A list of user hooks
 */
static TAILQ_HEAD(HookHead, Hook) Hooks = TAILQ_HEAD_INITIALIZER(Hooks);
struct Hook
{
  int type;                /**< hook type */
  struct Regex regex;      /**< regular expression */
  char *command;           /**< filename, command or pattern to execute */
  struct Pattern *pattern; /**< used for fcc,save,send-hook */
  TAILQ_ENTRY(Hook) entries;
};

static int current_hook_type = 0;

int mutt_parse_hook(struct Buffer *buf, struct Buffer *s, unsigned long data,
                    struct Buffer *err)
{
  struct Hook *ptr = NULL;
  struct Buffer command, pattern;
  int rc;
  bool not = false;
  regex_t *rx = NULL;
  struct Pattern *pat = NULL;
  char path[_POSIX_PATH_MAX];

  mutt_buffer_init(&pattern);
  mutt_buffer_init(&command);

  if (~data & MUTT_GLOBALHOOK) /* NOT a global hook */
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
      strfcpy(err->data, _("too few arguments"), err->dsize);
      goto error;
    }
  }

  mutt_extract_token(&command, s,
                     (data & (MUTT_FOLDERHOOK | MUTT_SENDHOOK | MUTT_SEND2HOOK |
                              MUTT_ACCOUNTHOOK | MUTT_REPLYHOOK)) ?
                         MUTT_TOKEN_SPACE :
                         0);

  if (!command.data)
  {
    strfcpy(err->data, _("too few arguments"), err->dsize);
    goto error;
  }

  if (MoreArgs(s))
  {
    strfcpy(err->data, _("too many arguments"), err->dsize);
    goto error;
  }

  if (data & (MUTT_FOLDERHOOK | MUTT_MBOXHOOK))
  {
    /* Accidentally using the ^ mailbox shortcut in the .neomuttrc is a
     * common mistake */
    if ((*pattern.data == '^') && (!CurrentFolder))
    {
      strfcpy(err->data, _("current mailbox shortcut '^' is unset"), err->dsize);
      goto error;
    }

    strfcpy(path, pattern.data, sizeof(path));
    _mutt_expand_path(path, sizeof(path), 1);

    /* Check for other mailbox shortcuts that expand to the empty string.
     * This is likely a mistake too */
    if (!*path && *pattern.data)
    {
      strfcpy(err->data, _("mailbox shortcut expanded to empty regex"), err->dsize);
      goto error;
    }

    FREE(&pattern.data);
    mutt_buffer_init(&pattern);
    pattern.data = safe_strdup(path);
  }
#ifdef USE_COMPRESSED
  else if (data & (MUTT_APPENDHOOK | MUTT_OPENHOOK | MUTT_CLOSEHOOK))
  {
    if (mutt_comp_valid_command(command.data) == 0)
    {
      strfcpy(err->data, _("badly formatted command string"), err->dsize);
      return -1;
    }
  }
#endif
  else if (DefaultHook && (~data & MUTT_GLOBALHOOK) &&
           !(data & (MUTT_CHARSETHOOK | MUTT_ICONVHOOK | MUTT_ACCOUNTHOOK)) &&
           (!WithCrypto || !(data & MUTT_CRYPTHOOK)))
  {
    char tmp[HUGE_STRING];

    /* At this stage remain only message-hooks, reply-hooks, send-hooks,
     * send2-hooks, save-hooks, and fcc-hooks: All those allowing full
     * patterns. If given a simple regex, we expand $default_hook.
     */
    strfcpy(tmp, pattern.data, sizeof(tmp));
    mutt_check_simple(tmp, sizeof(tmp), DefaultHook);
    FREE(&pattern.data);
    mutt_buffer_init(&pattern);
    pattern.data = safe_strdup(tmp);
  }

  if (data & (MUTT_MBOXHOOK | MUTT_SAVEHOOK | MUTT_FCCHOOK))
  {
    strfcpy(path, command.data, sizeof(path));
    mutt_expand_path(path, sizeof(path));
    FREE(&command.data);
    mutt_buffer_init(&command);
    command.data = safe_strdup(path);
  }

  /* check to make sure that a matching hook doesn't already exist */
  TAILQ_FOREACH(ptr, &Hooks, entries)
  {
    if (data & MUTT_GLOBALHOOK)
    {
      /* Ignore duplicate global hooks */
      if (mutt_strcmp(ptr->command, command.data) == 0)
      {
        FREE(&command.data);
        return 0;
      }
    }
    else if (ptr->type == data &&
             ptr->regex.not == not&&(mutt_strcmp(pattern.data, ptr->regex.pattern) == 0))
    {
      if (data & (MUTT_FOLDERHOOK | MUTT_SENDHOOK | MUTT_SEND2HOOK | MUTT_MESSAGEHOOK |
                  MUTT_ACCOUNTHOOK | MUTT_REPLYHOOK | MUTT_CRYPTHOOK |
                  MUTT_TIMEOUTHOOK | MUTT_STARTUPHOOK | MUTT_SHUTDOWNHOOK))
      {
        /* these hooks allow multiple commands with the same
         * pattern, so if we've already seen this pattern/command pair, just
         * ignore it instead of creating a duplicate */
        if (mutt_strcmp(ptr->command, command.data) == 0)
        {
          FREE(&command.data);
          FREE(&pattern.data);
          return 0;
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
        return 0;
      }
    }
  }

  if (data & (MUTT_SENDHOOK | MUTT_SEND2HOOK | MUTT_SAVEHOOK | MUTT_FCCHOOK |
              MUTT_MESSAGEHOOK | MUTT_REPLYHOOK))
  {
    if ((pat = mutt_pattern_comp(
             pattern.data, (data & (MUTT_SENDHOOK | MUTT_SEND2HOOK | MUTT_FCCHOOK)) ? 0 : MUTT_FULL_MSG,
             err)) == NULL)
      goto error;
  }
  else if (~data & MUTT_GLOBALHOOK) /* NOT a global hook */
  {
    /* Hooks not allowing full patterns: Check syntax of regex */
    rx = safe_malloc(sizeof(regex_t));
#ifdef MUTT_CRYPTHOOK
    if ((rc = REGCOMP(rx, NONULL(pattern.data),
                      ((data & (MUTT_CRYPTHOOK | MUTT_CHARSETHOOK | MUTT_ICONVHOOK)) ? REG_ICASE : 0))) != 0)
#else
    if ((rc = REGCOMP(rx, NONULL(pattern.data),
                      (data & (MUTT_CHARSETHOOK | MUTT_ICONVHOOK)) ? REG_ICASE : 0)) != 0)
#endif /* MUTT_CRYPTHOOK */
    {
      regerror(rc, rx, err->data, err->dsize);
      FREE(&rx);
      goto error;
    }
  }

  ptr = safe_calloc(1, sizeof(struct Hook));
  ptr->type = data;
  ptr->command = command.data;
  ptr->pattern = pat;
  ptr->regex.pattern = pattern.data;
  ptr->regex.regex = rx;
  ptr->regex.not = not;
  TAILQ_INSERT_TAIL(&Hooks, ptr, entries);
  return 0;

error:
  if (~data & MUTT_GLOBALHOOK) /* NOT a global hook */
    FREE(&pattern.data);
  FREE(&command.data);
  return -1;
}

static void delete_hook(struct Hook *h)
{
  FREE(&h->command);
  FREE(&h->regex.pattern);
  if (h->regex.regex)
  {
    regfree(h->regex.regex);
  }
  mutt_pattern_free(&h->pattern);
  FREE(&h);
}

/**
 * delete_hooks - Delete matching hooks
 * @param type
 * * Hook type to delete, e.g. #MUTT_SENDHOOK
 * * Or, 0 to delete all hooks
 */
static void delete_hooks(int type)
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

int mutt_parse_unhook(struct Buffer *buf, struct Buffer *s, unsigned long data,
                      struct Buffer *err)
{
  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, 0);
    if (mutt_strcmp("*", buf->data) == 0)
    {
      if (current_hook_type)
      {
        snprintf(err->data, err->dsize, "%s",
                 _("unhook: Can't do unhook * from within a hook."));
        return -1;
      }
      delete_hooks(0);
    }
    else
    {
      int type = mutt_get_hook_type(buf->data);

      if (!type)
      {
        snprintf(err->data, err->dsize, _("unhook: unknown hook type: %s"), buf->data);
        return -1;
      }
      if (current_hook_type == type)
      {
        snprintf(err->data, err->dsize,
                 _("unhook: Can't delete a %s from within a %s."), buf->data, buf->data);
        return -1;
      }
      delete_hooks(type);
    }
  }
  return 0;
}

void mutt_folder_hook(char *path)
{
  struct Hook *tmp = NULL;
  struct Buffer err, token;

  current_hook_type = MUTT_FOLDERHOOK;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = safe_malloc(err.dsize);
  mutt_buffer_init(&token);
  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if (!tmp->command)
      continue;

    if (tmp->type & MUTT_FOLDERHOOK)
    {
      if ((regexec(tmp->regex.regex, path, 0, NULL, 0) == 0) ^ tmp->regex.not)
      {
        if (mutt_parse_rc_line(tmp->command, &token, &err) == -1)
        {
          mutt_error("%s", err.data);
          FREE(&token.data);
          mutt_sleep(1); /* pause a moment to let the user see the error */
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

void mutt_message_hook(struct Context *ctx, struct Header *hdr, int type)
{
  struct Buffer err, token;
  struct Hook *hook = NULL;
  struct PatternCache cache;

  current_hook_type = type;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = safe_malloc(err.dsize);
  mutt_buffer_init(&token);
  memset(&cache, 0, sizeof(cache));
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->type & type)
      if ((mutt_pattern_exec(hook->pattern, 0, ctx, hdr, &cache) > 0) ^
          hook->regex.not)
      {
        if (mutt_parse_rc_line(hook->command, &token, &err) == -1)
        {
          FREE(&token.data);
          mutt_error("%s", err.data);
          mutt_sleep(1);
          current_hook_type = 0;
          FREE(&err.data);

          return;
        }
        /* Executing arbitrary commands could affect the pattern results,
         * so the cache has to be wiped */
        memset(&cache, 0, sizeof(cache));
      }
  }
  FREE(&token.data);
  FREE(&err.data);

  current_hook_type = 0;
}

static int addr_hook(char *path, size_t pathlen, int type, struct Context *ctx,
                     struct Header *hdr)
{
  struct Hook *hook = NULL;
  struct PatternCache cache;

  memset(&cache, 0, sizeof(cache));
  /* determine if a matching hook exists */
  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!hook->command)
      continue;

    if (hook->type & type)
      if ((mutt_pattern_exec(hook->pattern, 0, ctx, hdr, &cache) > 0) ^
          hook->regex.not)
      {
        mutt_make_string(path, pathlen, hook->command, ctx, hdr);
        return 0;
      }
  }

  return -1;
}

void mutt_default_save(char *path, size_t pathlen, struct Header *hdr)
{
  *path = '\0';
  if (addr_hook(path, pathlen, MUTT_SAVEHOOK, Context, hdr) != 0)
  {
    char tmp[_POSIX_PATH_MAX];
    struct Address *adr = NULL;
    struct Envelope *env = hdr->env;
    bool fromMe = mutt_addr_is_user(env->from);

    if (!fromMe && env->reply_to && env->reply_to->mailbox)
      adr = env->reply_to;
    else if (!fromMe && env->from && env->from->mailbox)
      adr = env->from;
    else if (env->to && env->to->mailbox)
      adr = env->to;
    else if (env->cc && env->cc->mailbox)
      adr = env->cc;
    else
      adr = NULL;
    if (adr)
    {
      mutt_safe_path(tmp, sizeof(tmp), adr);
      snprintf(path, pathlen, "=%s", tmp);
    }
  }
}

void mutt_select_fcc(char *path, size_t pathlen, struct Header *hdr)
{
  struct Address *adr = NULL;
  char buf[_POSIX_PATH_MAX];
  struct Envelope *env = hdr->env;

  if (addr_hook(path, pathlen, MUTT_FCCHOOK, NULL, hdr) != 0)
  {
    if ((option(OPT_SAVE_NAME) || option(OPT_FORCE_NAME)) && (env->to || env->cc || env->bcc))
    {
      adr = env->to ? env->to : (env->cc ? env->cc : env->bcc);
      mutt_safe_path(buf, sizeof(buf), adr);
      mutt_concat_path(path, NONULL(Folder), buf, pathlen);
      if (!option(OPT_FORCE_NAME) && mx_access(path, W_OK) != 0)
        strfcpy(path, NONULL(Record), pathlen);
    }
    else
      strfcpy(path, NONULL(Record), pathlen);
  }
  mutt_pretty_mailbox(path, pathlen);
}

static char *_mutt_string_hook(const char *match, int hook)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if ((tmp->type & hook) && ((match && regexec(tmp->regex.regex, match, 0, NULL, 0) == 0) ^
                               tmp->regex.not))
      return tmp->command;
  }
  return NULL;
}

static void _mutt_list_hook(struct ListHead *matches, const char *match, int hook)
{
  struct Hook *tmp = NULL;

  TAILQ_FOREACH(tmp, &Hooks, entries)
  {
    if ((tmp->type & hook) && ((match && regexec(tmp->regex.regex, match, 0, NULL, 0) == 0) ^
                               tmp->regex.not))
      mutt_list_insert_tail(matches, safe_strdup(tmp->command));
  }
}

char *mutt_charset_hook(const char *chs)
{
  return _mutt_string_hook(chs, MUTT_CHARSETHOOK);
}

char *mutt_iconv_hook(const char *chs)
{
  return _mutt_string_hook(chs, MUTT_ICONVHOOK);
}

void mutt_crypt_hook(struct ListHead *list, struct Address *adr)
{
  _mutt_list_hook(list, adr->mailbox, MUTT_CRYPTHOOK);
}

#ifdef USE_SOCKET
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
  err.data = safe_malloc(err.dsize);
  mutt_buffer_init(&token);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & MUTT_ACCOUNTHOOK)))
      continue;

    if ((regexec(hook->regex.regex, url, 0, NULL, 0) == 0) ^ hook->regex.not)
    {
      inhook = true;

      if (mutt_parse_rc_line(hook->command, &token, &err) == -1)
      {
        FREE(&token.data);
        mutt_error("%s", err.data);
        FREE(&err.data);
        mutt_sleep(1);

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

void mutt_timeout_hook(void)
{
  struct Hook *hook = NULL;
  struct Buffer token;
  struct Buffer err;
  char buf[STRING];

  err.data = buf;
  err.dsize = sizeof(buf);
  mutt_buffer_init(&token);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & MUTT_TIMEOUTHOOK)))
      continue;

    if (mutt_parse_rc_line(hook->command, &token, &err) == -1)
    {
      mutt_error("%s", err.data);
      mutt_sleep(1);

      /* The hooks should be independent of each other, so even though this on
       * failed, we'll carry on with the others. */
    }
  }
  FREE(&token.data);
}

/**
 * mutt_startup_shutdown_hook - Execute any startup/shutdown hooks
 * @param type Hook type: MUTT_STARTUPHOOK or MUTT_SHUTDOWNHOOK
 *
 * The user can configure hooks to be run on startup/shutdown.
 * This function finds all the matching hooks and executes them.
 */
void mutt_startup_shutdown_hook(int type)
{
  struct Hook *hook = NULL;
  struct Buffer token;
  struct Buffer err;
  char buf[STRING];

  err.data = buf;
  err.dsize = sizeof(buf);
  mutt_buffer_init(&token);

  TAILQ_FOREACH(hook, &Hooks, entries)
  {
    if (!(hook->command && (hook->type & type)))
      continue;

    if (mutt_parse_rc_line(hook->command, &token, &err) == -1)
    {
      mutt_error("%s", err.data);
      mutt_sleep(1);
    }
  }
  FREE(&token.data);
}
