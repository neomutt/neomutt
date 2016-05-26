/*
 * Copyright (C) 1996-2009,2012 Michael R. Elkins <me@mutt.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"

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
  const char *p;
  FILE *ifp, *ofp;
  int i, keep;
  ENVELOPE *n;
  time_t mtime;
  struct stat st;
  LIST *cur, **last = NULL, *tmp;

  mutt_mktemp (path, sizeof (path));
  if ((ofp = safe_fopen (path, "w")) == NULL)
  {
    mutt_perror (path);
    return;
  }

  mutt_env_to_local (msg->env);
  mutt_write_rfc822_header (ofp, msg->env, NULL, 1, 0);
  fputc ('\n', ofp);	/* tie off the header. */

  /* now copy the body of the message. */
  if ((ifp = fopen (body, "r")) == NULL)
  {
    mutt_perror (body);
    return;
  }

  mutt_copy_stream (ifp, ofp);

  safe_fclose (&ifp);
  safe_fclose (&ofp);

  if (stat (path, &st) == -1)
  {
    mutt_perror (path);
    return;
  }

  mtime = mutt_decrease_mtime (path, &st);

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
  if ((ifp = fopen (path, "r")) == NULL)
  {
    mutt_perror (path);
    return;
  }

  if ((ofp = safe_fopen (body, "w")) == NULL)
  {
    /* intentionally leak a possible temporary file here */
    safe_fclose (&ifp);
    mutt_perror (body);
    return;
  }

  n = mutt_read_rfc822_header (ifp, NULL, 1, 0);
  while ((i = fread (buffer, 1, sizeof (buffer), ifp)) > 0)
    fwrite (buffer, 1, i, ofp);
  safe_fclose (&ofp);
  safe_fclose (&ifp);
  mutt_unlink (path);

  /* in case the user modifies/removes the In-Reply-To header with
     $edit_headers set, we remove References: as they're likely invalid;
     we can simply compare strings as we don't generate References for
     multiple Message-Ids in IRT anyways */
#ifdef USE_NNTP
  if (!option (OPTNEWSSEND))
#endif
  if (msg->env->in_reply_to &&
      (!n->in_reply_to || mutt_strcmp (n->in_reply_to->data,
				       msg->env->in_reply_to->data) != 0))
    mutt_free_list (&msg->env->references);

  /* restore old info. */
  mutt_free_list (&n->references);
  n->references = msg->env->references;
  msg->env->references = NULL;

  mutt_free_envelope (&msg->env);
  msg->env = n; n = NULL;

  mutt_expand_aliases_env (msg->env);

  /* search through the user defined headers added to see if
   * fcc: or attach: or pgp: was specified
   */

  cur = msg->env->userhdrs;
  last = &msg->env->userhdrs;
  while (cur)
  {
    keep = 1;

    if (fcc && ascii_strncasecmp ("fcc:", cur->data, 4) == 0)
    {
      p = skip_email_wsp(cur->data + 4);
      if (*p)
      {
	strfcpy (fcc, p, fcclen);
	mutt_pretty_mailbox (fcc, fcclen);
      }
      keep = 0;
    }
    else if (ascii_strncasecmp ("attach:", cur->data, 7) == 0)
    {
      BODY *body;
      BODY *parts;
      size_t l = 0;

      p = skip_email_wsp(cur->data + 7);
      if (*p)
      {
	for ( ; *p && *p != ' ' && *p != '\t'; p++)
	{
	  if (*p == '\\')
	  {
	    if (!*(p+1))
	      break;
	    p++;
	  }
	  if (l < sizeof (path) - 1)
	    path[l++] = *p;
	}
	p = skip_email_wsp(p);
	path[l] = 0;

	mutt_expand_path (path, sizeof (path));
	if ((body = mutt_make_file_attach (path)))
	{
	  body->description = safe_strdup (p);
	  for (parts = msg->content; parts->next; parts = parts->next) ;
	  parts->next = body;
	}
	else
	{
	  mutt_pretty_mailbox (path, sizeof (path));
	  mutt_error (_("%s: unable to attach file"), path);
	}
      }
      keep = 0;
    }
    else if ((WithCrypto & APPLICATION_PGP)
             && ascii_strncasecmp ("pgp:", cur->data, 4) == 0)
    {
      msg->security = mutt_parse_crypt_hdr (cur->data + 4, 0, APPLICATION_PGP);
      if (msg->security)
	msg->security |= APPLICATION_PGP;
      keep = 0;
    }

    if (keep)
    {
      last = &cur->next;
      cur  = cur->next;
    }
    else
    {
      tmp       = cur;
      *last     = cur->next;
      cur       = cur->next;
      tmp->next = NULL;
      mutt_free_list (&tmp);
    }
  }
}

void mutt_label_ref_dec(ENVELOPE *env)
{
  uintptr_t count;
  LIST *label;

  for (label = env->labels; label; label = label->next)
  {
    if (label->data == NULL)
      continue;
    count = (uintptr_t)hash_find(Labels, label->data);
    if (count)
    {
      hash_delete(Labels, label->data, NULL, NULL);
      count--;
      if (count > 0)
        hash_insert(Labels, label->data, (void *)count, 0);
    }
    dprint(1, (debugfile, "--label %s: %d\n", label->data, count));
  }
}

void mutt_label_ref_inc(ENVELOPE *env)
{
  uintptr_t count;
  LIST *label;

  for (label = env->labels; label; label = label->next)
  {
    if (label->data == NULL)
      continue;
    count = (uintptr_t)hash_find(Labels, label->data);
    if (count)
      hash_delete(Labels, label->data, NULL, NULL);
    count++;  /* was zero if not found */
    hash_insert(Labels, label->data, (void *)count, 0);
    dprint(1, (debugfile, "++label %s: %d\n", label->data, count));
  }
}

/*
 * set labels on a message
 */
static int label_message(HEADER *hdr, char *new)
{
  if (hdr == NULL)
    return 0;
  if (hdr->env->labels == NULL && new == NULL)
    return 0;
  if (hdr->env->labels != NULL && new != NULL)
  {
    char old[HUGE_STRING];
    mutt_labels(old, sizeof(old), hdr->env, NULL);
    if (!strcmp(old, new))
      return 0;
  }

  if (hdr->env->labels != NULL)
  {
    mutt_label_ref_dec(hdr->env);
    mutt_free_list(&hdr->env->labels);
  }

  if (new == NULL)
    hdr->env->labels = NULL;
  else
  {
    char *last, *label;

    for (label = strtok_r(new, ",", &last); label;
         label = strtok_r(NULL, ",", &last))
    {
      SKIPWS(label);
      if (mutt_find_list(hdr->env->labels, label))
        continue;
      if (hdr->env->labels == NULL)
      {
        hdr->env->labels = mutt_new_list();
        hdr->env->labels->data = safe_strdup(label);
      }
      else
        mutt_add_list(hdr->env->labels, label);
    }
    mutt_label_ref_inc(hdr->env);
  }
  return hdr->changed = hdr->label_changed = 1;
}

int mutt_label_message(HEADER *hdr)
{
  char buf[LONG_STRING], *new;
  int i;
  int changed;

  *buf = '\0';
  if (hdr != NULL && hdr->env->labels != NULL)
    mutt_labels(buf, sizeof(buf)-2, hdr->env, NULL);

  /* add a comma-space so that new typing is a new keyword */
  if (buf[0])
    strcat(buf, ", ");    /* __STRCAT_CHECKED__ */

  if (mutt_get_field("Label: ", buf, sizeof(buf), M_LABEL /* | M_CLEAR */) != 0)
    return 0;

  new = buf;
  SKIPWS(new);
  if (new && *new)
  {
    char *p;
    int len = strlen(new);
    p = &new[len]; /* '\0' */
    while (p > new)
    {
      if (!isspace((unsigned char)*(p-1)) && *(p-1) != ',')
        break;
      p--;
    }
    *p = '\0';
  }
  if (*new == '\0')
    new = NULL;

  changed = 0;
  if (hdr != NULL) {
    changed += label_message(hdr, new);
  } else {
#define HDR_OF(index) Context->hdrs[Context->v2r[(index)]]
    for (i = 0; i < Context->vcount; ++i) {
      if (HDR_OF(i)->tagged)
        if (label_message(HDR_OF(i), new)) {
          ++changed;
          mutt_set_flag(Context, HDR_OF(i),
            M_TAG, 0);
        }
    }
  }

  return changed;
}

/* scan a context (mailbox) and hash all labels we find */
void mutt_scan_labels(CONTEXT *ctx)
{
  int i;

  for (i = 0; i < ctx->msgcount; i++)
    if (ctx->hdrs[i]->env->labels)
      mutt_label_ref_inc(ctx->hdrs[i]->env);
}


char *mutt_labels(char *dst, int sz, ENVELOPE *env, char *sep)
{
  static char sbuf[HUGE_STRING];
  int off = 0;
  int len;
  LIST *label;

  if (sep == NULL)
    sep = ", ";

  if (dst == NULL)
  {
    dst = sbuf;
    sz = sizeof(sbuf);
  }

  *dst = '\0';

  for (label = env->labels; label; label = label->next)
  {
    if (label->data == NULL)
      continue;
    len = MIN(mutt_strlen(label->data), sz-off);
    strfcpy(&dst[off], label->data, len+1);
    off += len;
    if (label->next)
    {
      len = MIN(mutt_strlen(sep), sz-off);
      strfcpy(&dst[off], sep, len+1);
      off += len;
    }
  }

  return dst;
}
