/* 
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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

#ifdef _PGPPATH
#include "pgp.h"
#endif

#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

void mutt_edit_headers (const char *editor,
			const char *body,
			HEADER *msg,
			char *fcc,
			size_t fcclen)
{
  char path[_POSIX_PATH_MAX];	/* tempfile used to edit headers + body */
  char buffer[LONG_STRING];
  char *p;
  FILE *ifp, *ofp;
  int i, keep;
  int in_reply_to = 0;	/* did we see the in-reply-to field ? */
  ENVELOPE *n;
  time_t mtime;
  struct stat st;
  LIST *cur, *last = NULL, *tmp;

  mutt_mktemp (path);
  if ((ofp = safe_fopen (path, "w")) == NULL)
  {
    mutt_perror (path);
    return;
  }

  mutt_write_rfc822_header (ofp, msg->env, NULL, 1, 0);
  fputc ('\n', ofp);	/* tie off the header. */

  /* now copy the body of the message. */
  if ((ifp = fopen (body, "r")) == NULL)
  {
    mutt_perror (body);
    return;
  }

  mutt_copy_stream (ifp, ofp);

  fclose (ifp);
  fclose (ofp);

  if (stat (path, &st) == -1)
  {
    mutt_perror (path);
    return;
  }

  mtime = st.st_mtime;
  mutt_edit_file (editor, path);
  stat (path, &st);
  if (mtime == st.st_mtime)
  {
    dprint (1, (debugfile, "ci_edit_headers(): temp file was not modified.\n"));
    /* the file has not changed! */
    mutt_unlink (path);
    return;
  }

  mutt_unlink (body);
  mutt_free_list (&msg->env->userhdrs);

  /* Read the temp file back in */
  ifp = fopen (path, "r");
  ofp = safe_fopen (body, "w");
  n = mutt_read_rfc822_header (ifp, NULL, 1, 0);
  while ((i = fread (buffer, 1, sizeof (buffer), ifp)) > 0)
    fwrite (buffer, 1, i, ofp);
  fclose (ofp);
  fclose (ifp);
  mutt_unlink (path);

  /* restore old info. */
  n->references = msg->env->references;
  msg->env->references = NULL;
  mutt_free_envelope (&msg->env);
  msg->env = n;

  msg->env->from = mutt_expand_aliases (msg->env->from);
  msg->env->to = mutt_expand_aliases (msg->env->to);
  msg->env->cc = mutt_expand_aliases (msg->env->cc);
  msg->env->bcc = mutt_expand_aliases (msg->env->bcc);
  msg->env->reply_to = mutt_expand_aliases (msg->env->reply_to);
  msg->env->mail_followup_to = mutt_expand_aliases (msg->env->mail_followup_to);

  /* search through the user defined headers added to see if either a 
   * fcc: or attach-file: field was specified.  
   */

  cur = msg->env->userhdrs;
  while (cur)
  {
    keep = 1;

    /* keep track of whether or not we see the in-reply-to field.  if we did
     * not, remove the references: field later so that we can generate a new
     * message based upon this one.
     */
    if (mutt_strncasecmp ("in-reply-to:", cur->data, 12) == 0)
      in_reply_to = 1;
    else if (fcc && mutt_strncasecmp ("fcc:", cur->data, 4) == 0)
    {
      p = cur->data + 4;
      SKIPWS (p);
      if (*p)
      {
	strfcpy (fcc, p, fcclen);
	mutt_pretty_mailbox (fcc);
      }
      keep = 0;
    }
    else if (mutt_strncasecmp ("attach:", cur->data, 7) == 0)
    {
      BODY *body;
      BODY *parts;
      char *q;

      p = cur->data + 7;
      SKIPWS (p);
      if (*p)
      {
	if ((q = strpbrk (p, " \t")))
	{
	  mutt_substrcpy (path, p, q, sizeof (path));
	  SKIPWS (q);
	}
	else
	  strfcpy (path, p, sizeof (path));
	mutt_expand_path (path, sizeof (path));
	if ((body = mutt_make_file_attach (path)))
	{
	  body->description = safe_strdup (q);
	  for (parts = msg->content; parts->next; parts = parts->next) ;
	  parts->next = body;
	}
	else
	{
	  mutt_pretty_mailbox (path);
	  mutt_error (_("%s: unable to attach file"), path);
	}
      }
      keep = 0;
    }



#ifdef _PGPPATH
    else if (mutt_strncasecmp ("pgp:", cur->data, 4) == 0)
    {
      msg->pgp = mutt_parse_pgp_hdr (cur->data + 4, 0);
      keep = 0;
    }
#endif


    if (keep)
    {
      last = cur;
      cur = cur->next;
    }
    else
    {
      if (last)
	last->next = cur->next;
      else
	msg->env->userhdrs = cur->next;
      tmp = cur;
      cur = cur->next;
      tmp->next = NULL;
      mutt_free_list (&tmp);
    }
  }

  if (!in_reply_to)
    mutt_free_list (&msg->env->references);
}
