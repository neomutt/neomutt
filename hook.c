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

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

typedef struct hook
{
  int type;		/* hook type */
  REGEXP rx;		/* regular expression */
  char *command;	/* filename, command or pattern to execute */
  pattern_t *pattern;	/* used for fcc,save,send-hook */
  struct hook *next;
} HOOK;

static HOOK *Hooks = NULL;

int mutt_parse_hook (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  HOOK *ptr;
  BUFFER command, pattern;
  int rc, not = 0;
  regex_t *rx = NULL;
  pattern_t *pat = NULL;
  char path[_POSIX_PATH_MAX];

  memset (&pattern, 0, sizeof (pattern));
  memset (&command, 0, sizeof (command));

  if (*s->dptr == '!')
  {
    s->dptr++;
    SKIPWS (s->dptr);
    not = 1;
  }

  mutt_extract_token (&pattern, s, 0);

  if (!MoreArgs (s))
  {
    strfcpy (err->data, _("too few arguments"), err->dsize);
    goto error;
  }

  mutt_extract_token (&command, s, (data & (M_FOLDERHOOK | M_SENDHOOK)) ?  M_TOKEN_SPACE : 0);

  if (!command.data)
  {
    strfcpy (err->data, _("too few arguments"), err->dsize);
    goto error;
  }

  if (MoreArgs (s))
  {
    strfcpy (err->data, _("too many arguments"), err->dsize);
    goto error;
  }

  if (data & (M_FOLDERHOOK | M_MBOXHOOK))
  {
    strfcpy (path, pattern.data, sizeof (path));
    mutt_expand_path (path, sizeof (path));
    FREE (&pattern.data);
    memset (&pattern, 0, sizeof (pattern));
    pattern.data = safe_strdup (path);
  }
  else if (DefaultHook
#ifdef _PGPPATH
      && !(data & M_PGPHOOK)
#endif /* _PGPPATH */
      )
  {
    char tmp[HUGE_STRING];

    strfcpy (tmp, pattern.data, sizeof (tmp));
    mutt_check_simple (tmp, sizeof (tmp), DefaultHook);
    FREE (&pattern.data);
    memset (&pattern, 0, sizeof (pattern));
    pattern.data = safe_strdup (tmp);
  }

  if (data & (M_MBOXHOOK | M_SAVEHOOK | M_FCCHOOK))
  {
    strfcpy (path, command.data, sizeof (path));
    mutt_expand_path (path, sizeof (path));
    FREE (&command.data);
    memset (&command, 0, sizeof (command));
    command.data = safe_strdup (path);
  }

  /* check to make sure that a matching hook doesn't already exist */
  for (ptr = Hooks; ptr; ptr = ptr->next)
  {
    if (ptr->type == data &&
	ptr->rx.not == not &&
	!mutt_strcmp (pattern.data, ptr->rx.pattern))
    {
      if (data & (M_FOLDERHOOK | M_SENDHOOK))
      {
	/* folder-hook and send-hook allow multiple commands with the same
	   pattern, so if we've already seen this pattern/command pair, just
	   ignore it instead of creating a duplicate */
	if (!mutt_strcmp (ptr->command, command.data))
	{
	  FREE (&command.data);
	  FREE (&pattern.data);
	  return 0;
	}
      }
      else
      {
	/* other hooks only allow one command per pattern, so update the
	   entry with the new command.  this currently does not change the
	   order of execution of the hooks, which i think is desirable since
	   a common action to perform is to change the default (.) entry
	   based upon some other information. */
	FREE (&ptr->command);
	ptr->command = command.data;
	FREE (&pattern.data);
	return 0;
      }
    }
    if (!ptr->next)
      break;
  }

  if (data & (M_SENDHOOK | M_SAVEHOOK | M_FCCHOOK))
  {
    if ((pat = mutt_pattern_comp (pattern.data, (data & M_SAVEHOOK) ? M_FULL_MSG : 0, err)) == NULL)
      goto error;
  }
  else
  {
    rx = safe_malloc (sizeof (regex_t));
#ifdef M_PGPHOOK
    if ((rc = REGCOMP (rx, pattern.data, ((data & M_PGPHOOK) ? REG_ICASE : 0))) != 0)
#else
    if ((rc = REGCOMP (rx, pattern.data, 0)) != 0)
#endif /* _PGPPATH */
    {
      regerror (rc, rx, err->data, err->dsize);
      regfree (rx);
      safe_free ((void **) &rx);
      goto error;
    }
  }

  if (ptr)
  {
    ptr->next = safe_calloc (1, sizeof (HOOK));
    ptr = ptr->next;
  }
  else
    Hooks = ptr = safe_calloc (1, sizeof (HOOK));
  ptr->type = data;
  ptr->command = command.data;
  ptr->pattern = pat;
  ptr->rx.pattern = pattern.data;
  ptr->rx.rx = rx;
  ptr->rx.not = not;
  return 0;

error:
  FREE (&pattern.data);
  FREE (&command.data);
  return (-1);
}

void mutt_folder_hook (char *path)
{
  HOOK *tmp = Hooks;
  BUFFER err, token;
  char buf[STRING];

  err.data = buf;
  err.dsize = sizeof (buf);
  memset (&token, 0, sizeof (token));
  for (; tmp; tmp = tmp->next)
  {
    if(!tmp->command)
      continue;

    if (tmp->type & M_FOLDERHOOK)
    {
      if ((regexec (tmp->rx.rx, path, 0, NULL, 0) == 0) ^ tmp->rx.not)
      {
	if (mutt_parse_rc_line (tmp->command, &token, &err) == -1)
	{
	  mutt_error ("%s", err.data);
	  FREE (&token.data);
	  sleep (1);	/* pause a moment to let the user see the error */
	  return;
	}
      }
    }
  }
  FREE (&token.data);
}

char *mutt_find_hook (int type, const char *pat)
{
  HOOK *tmp = Hooks;

  for (; tmp; tmp = tmp->next)
    if (tmp->type & type)
    {
      if (regexec (tmp->rx.rx, pat, 0, NULL, 0) == 0)
	return (tmp->command);
    }
  return (NULL);
}

void mutt_send_hook (HEADER *hdr)
{
  BUFFER err, token;
  HOOK *hook;
  char buf[STRING];

  err.data = buf;
  err.dsize = sizeof (buf);
  memset (&token, 0, sizeof (token));
  for (hook = Hooks; hook; hook = hook->next)
  {
    if(!hook->command)
      continue;

    if (hook->type & M_SENDHOOK)
      if ((mutt_pattern_exec (hook->pattern, 0, NULL, hdr) > 0) ^ hook->rx.not)
	if (mutt_parse_rc_line (hook->command, &token, &err) != 0)
	{
	  FREE (&token.data);
	  mutt_error ("%s", err.data);
	  sleep (1);
	  return;
	}
  }
  FREE (&token.data);
}

static int
mutt_addr_hook (char *path, size_t pathlen, int type, CONTEXT *ctx, HEADER *hdr)
{
  HOOK *hook;

  /* determine if a matching hook exists */
  for (hook = Hooks; hook; hook = hook->next)
  {
    if(!hook->command)
      continue;

    if (hook->type & type)
      if ((mutt_pattern_exec (hook->pattern, 0, ctx, hdr) > 0) ^ hook->rx.not)
      {
	mutt_make_string (path, pathlen, hook->command, ctx, hdr);
	return 0;
      }
  }

  return -1;
}

void mutt_default_save (char *path, size_t pathlen, HEADER *hdr)
{
  *path = 0;
  if (mutt_addr_hook (path, pathlen, M_SAVEHOOK, Context, hdr) != 0)
  {
    char tmp[_POSIX_PATH_MAX];
    ADDRESS *adr;
    ENVELOPE *env = hdr->env;
    int fromMe = mutt_addr_is_user (env->from);

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
      mutt_safe_path (tmp, sizeof (tmp), adr);
      snprintf (path, pathlen, "=%s", tmp);
    }
  }
}

void mutt_select_fcc (char *path, size_t pathlen, HEADER *hdr)
{
  ADDRESS *adr;
  char buf[_POSIX_PATH_MAX];
  ENVELOPE *env = hdr->env;

  if (mutt_addr_hook (path, pathlen, M_FCCHOOK, NULL, hdr) != 0)
  {
    if ((option (OPTSAVENAME) || option (OPTFORCENAME)) &&
	(env->to || env->cc || env->bcc))
    {
      adr = env->to ? env->to : (env->cc ? env->cc : env->bcc);
      mutt_safe_path (buf, sizeof (buf), adr);
      snprintf (path, pathlen, "%s/%s", NONULL (Maildir), buf);
      if (!option (OPTFORCENAME) && access (path, W_OK) != 0)
	strfcpy (path, NONULL (Outbox), pathlen);
    }
    else
      strfcpy (path, NONULL (Outbox), pathlen);
  }
  mutt_pretty_mailbox (path);
}

#ifdef _PGPPATH
char *mutt_pgp_hook (ADDRESS *adr)
{
  HOOK *tmp = Hooks;

  for (; tmp; tmp = tmp->next)
  {
    if ((tmp->type & M_PGPHOOK) && ((adr->mailbox && 
	 regexec (tmp->rx.rx, adr->mailbox, 0, NULL, 0) == 0) ^ tmp->rx.not))
      return (tmp->command);
  }
  return (NULL);
}
#endif /* _PGPPATH */

