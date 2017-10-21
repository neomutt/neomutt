/**
 * @file
 * Manipulate an email's header
 *
 * @authors
 * Copyright (C) 1996-2009,2012 Michael R. Elkins <me@mutt.org>
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include "lib/lib.h"
#include "mutt.h"
#include "alias.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mutt_idna.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"

void mutt_edit_headers(const char *editor, const char *body, struct Header *msg,
                       char *fcc, size_t fcclen)
{
  char path[_POSIX_PATH_MAX]; /* tempfile used to edit headers + body */
  char buffer[LONG_STRING];
  const char *p = NULL;
  FILE *ifp = NULL, *ofp = NULL;
  int i;
  bool keep;
  struct Envelope *n = NULL;
  time_t mtime;
  struct stat st;

  mutt_mktemp(path, sizeof(path));
  ofp = safe_fopen(path, "w");
  if (!ofp)
  {
    mutt_perror(path);
    return;
  }

  mutt_env_to_local(msg->env);
  mutt_write_rfc822_header(ofp, msg->env, NULL, 1, 0);
  fputc('\n', ofp); /* tie off the header. */

  /* now copy the body of the message. */
  ifp = fopen(body, "r");
  if (!ifp)
  {
    mutt_perror(body);
    safe_fclose(&ofp);
    return;
  }

  mutt_copy_stream(ifp, ofp);

  safe_fclose(&ifp);
  safe_fclose(&ofp);

  if (stat(path, &st) == -1)
  {
    mutt_perror(path);
    return;
  }

  mtime = mutt_decrease_mtime(path, &st);

  mutt_edit_file(editor, path);
  stat(path, &st);
  if (mtime == st.st_mtime)
  {
    mutt_debug(1, "ci_edit_headers(): temp file was not modified.\n");
    /* the file has not changed! */
    mutt_unlink(path);
    return;
  }

  mutt_unlink(body);
  mutt_list_free(&msg->env->userhdrs);

  /* Read the temp file back in */
  ifp = fopen(path, "r");
  if (!ifp)
  {
    mutt_perror(path);
    return;
  }

  ofp = safe_fopen(body, "w");
  if (!ofp)
  {
    /* intentionally leak a possible temporary file here */
    safe_fclose(&ifp);
    mutt_perror(body);
    return;
  }

  n = mutt_read_rfc822_header(ifp, NULL, 1, 0);
  while ((i = fread(buffer, 1, sizeof(buffer), ifp)) > 0)
    fwrite(buffer, 1, i, ofp);
  safe_fclose(&ofp);
  safe_fclose(&ifp);
  mutt_unlink(path);

/* in case the user modifies/removes the In-Reply-To header with
     $edit_headers set, we remove References: as they're likely invalid;
     we can simply compare strings as we don't generate References for
     multiple Message-Ids in IRT anyways */
#ifdef USE_NNTP
  if (!option(OPT_NEWS_SEND))
#endif
    if (!STAILQ_EMPTY(&msg->env->in_reply_to) &&
        (STAILQ_EMPTY(&n->in_reply_to) ||
         (mutt_strcmp(STAILQ_FIRST(&n->in_reply_to)->data,
                      STAILQ_FIRST(&msg->env->in_reply_to)->data) != 0)))
      mutt_list_free(&msg->env->references);

  /* restore old info. */
  mutt_list_free(&n->references);
  STAILQ_SWAP(&n->references, &msg->env->references, ListNode);

  mutt_free_envelope(&msg->env);
  msg->env = n;
  n = NULL;

  mutt_expand_aliases_env(msg->env);

  /* search through the user defined headers added to see if
   * fcc: or attach: or pgp: was specified
   */

  struct ListNode *np, *tmp;
  STAILQ_FOREACH_SAFE(np, &msg->env->userhdrs, entries, tmp)
  {
    keep = true;

    if (fcc && (mutt_strncasecmp("fcc:", np->data, 4) == 0))
    {
      p = skip_email_wsp(np->data + 4);
      if (*p)
      {
        strfcpy(fcc, p, fcclen);
        mutt_pretty_mailbox(fcc, fcclen);
      }
      keep = false;
    }
    else if (mutt_strncasecmp("attach:", np->data, 7) == 0)
    {
      struct Body *body2 = NULL;
      struct Body *parts = NULL;
      size_t l = 0;

      p = skip_email_wsp(np->data + 7);
      if (*p)
      {
        for (; *p && *p != ' ' && *p != '\t'; p++)
        {
          if (*p == '\\')
          {
            if (!*(p + 1))
              break;
            p++;
          }
          if (l < sizeof(path) - 1)
            path[l++] = *p;
        }
        p = skip_email_wsp(p);
        path[l] = '\0';

        mutt_expand_path(path, sizeof(path));
        if ((body2 = mutt_make_file_attach(path)))
        {
          body2->description = safe_strdup(p);
          for (parts = msg->content; parts->next; parts = parts->next)
            ;
          parts->next = body2;
        }
        else
        {
          mutt_pretty_mailbox(path, sizeof(path));
          mutt_error(_("%s: unable to attach file"), path);
        }
      }
      keep = false;
    }
    else if ((WithCrypto & APPLICATION_PGP) && (mutt_strncasecmp("pgp:", np->data, 4) == 0))
    {
      msg->security = mutt_parse_crypt_hdr(np->data + 4, 0, APPLICATION_PGP);
      if (msg->security)
        msg->security |= APPLICATION_PGP;
      keep = false;
    }

    if (!keep)
    {
      STAILQ_REMOVE(&msg->env->userhdrs, np, ListNode, entries);
      FREE(&np->data);
      FREE(&np);
    }
  }
}

static void label_ref_dec(struct Context *ctx, char *label)
{
  struct HashElem *elem = NULL;
  uintptr_t count;

  elem = hash_find_elem(ctx->label_hash, label);
  if (!elem)
    return;

  count = (uintptr_t) elem->data;
  if (count <= 1)
  {
    hash_delete(ctx->label_hash, label, NULL, NULL);
    return;
  }

  count--;
  elem->data = (void *) count;
}

static void label_ref_inc(struct Context *ctx, char *label)
{
  struct HashElem *elem = NULL;
  uintptr_t count;

  elem = hash_find_elem(ctx->label_hash, label);
  if (!elem)
  {
    count = 1;
    hash_insert(ctx->label_hash, label, (void *) count);
    return;
  }

  count = (uintptr_t) elem->data;
  count++;
  elem->data = (void *) count;
}

/**
 * label_message - add an X-Label: field
 */
static int label_message(struct Context *ctx, struct Header *hdr, char *new)
{
  if (!hdr)
    return 0;
  if (mutt_strcmp(hdr->env->x_label, new) == 0)
    return 0;

  if (hdr->env->x_label)
    label_ref_dec(ctx, hdr->env->x_label);
  mutt_str_replace(&hdr->env->x_label, new);
  if (hdr->env->x_label)
    label_ref_inc(ctx, hdr->env->x_label);

  return hdr->changed = hdr->xlabel_changed = true;
}

int mutt_label_message(struct Header *hdr)
{
  char buf[LONG_STRING], *new = NULL;
  int changed;

  if (!Context || !Context->label_hash)
    return 0;

  *buf = '\0';
  if (hdr != NULL && hdr->env->x_label != NULL)
  {
    strfcpy(buf, hdr->env->x_label, sizeof(buf));
  }

  if (mutt_get_field("Label: ", buf, sizeof(buf), MUTT_LABEL /* | MUTT_CLEAR */) != 0)
    return 0;

  new = buf;
  SKIPWS(new);
  if (*new == '\0')
    new = NULL;

  changed = 0;
  if (hdr)
  {
    if (label_message(Context, hdr, new))
    {
      changed++;
      mutt_set_header_color(Context, hdr);
    }
  }
  else
  {
#define HDR_OF(index) Context->hdrs[Context->v2r[(index)]]
    for (int i = 0; i < Context->vcount; ++i)
    {
      if (HDR_OF(i)->tagged)
        if (label_message(Context, HDR_OF(i), new))
        {
          changed++;
          mutt_set_flag(Context, HDR_OF(i), MUTT_TAG, 0);
          /* mutt_set_flag re-evals the header color */
        }
    }
  }

  return changed;
}

void mutt_make_label_hash(struct Context *ctx)
{
  /* 131 is just a rough prime estimate of how many distinct
   * labels someone might have in a mailbox.
   */
  ctx->label_hash = hash_create(131, MUTT_HASH_STRDUP_KEYS);
}

void mutt_label_hash_add(struct Context *ctx, struct Header *hdr)
{
  if (!ctx || !ctx->label_hash)
    return;
  if (hdr->env->x_label)
    label_ref_inc(ctx, hdr->env->x_label);
}

void mutt_label_hash_remove(struct Context *ctx, struct Header *hdr)
{
  if (!ctx || !ctx->label_hash)
    return;
  if (hdr->env->x_label)
    label_ref_dec(ctx, hdr->env->x_label);
}
