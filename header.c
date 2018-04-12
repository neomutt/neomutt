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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "header.h"
#include "alias.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"

void mutt_edit_headers(const char *editor, const char *body, struct Header *msg,
                       char *fcc, size_t fcclen)
{
  char path[_POSIX_PATH_MAX]; /* tempfile used to edit headers + body */
  char buffer[LONG_STRING];
  const char *p = NULL;
  int i;
  struct Envelope *n = NULL;
  time_t mtime;
  struct stat st;

  mutt_mktemp(path, sizeof(path));
  FILE *ofp = mutt_file_fopen(path, "w");
  if (!ofp)
  {
    mutt_perror(path);
    return;
  }

  mutt_env_to_local(msg->env);
  mutt_rfc822_write_header(ofp, msg->env, NULL, 1, 0);
  fputc('\n', ofp); /* tie off the header. */

  /* now copy the body of the message. */
  FILE *ifp = fopen(body, "r");
  if (!ifp)
  {
    mutt_perror(body);
    mutt_file_fclose(&ofp);
    return;
  }

  mutt_file_copy_stream(ifp, ofp);

  mutt_file_fclose(&ifp);
  mutt_file_fclose(&ofp);

  if (stat(path, &st) == -1)
  {
    mutt_perror(path);
    return;
  }

  mtime = mutt_file_decrease_mtime(path, &st);

  mutt_edit_file(editor, path);
  stat(path, &st);
  if (mtime == st.st_mtime)
  {
    mutt_debug(1, "temp file was not modified.\n");
    /* the file has not changed! */
    mutt_file_unlink(path);
    return;
  }

  mutt_file_unlink(body);
  mutt_list_free(&msg->env->userhdrs);

  /* Read the temp file back in */
  ifp = fopen(path, "r");
  if (!ifp)
  {
    mutt_perror(path);
    return;
  }

  ofp = mutt_file_fopen(body, "w");
  if (!ofp)
  {
    /* intentionally leak a possible temporary file here */
    mutt_file_fclose(&ifp);
    mutt_perror(body);
    return;
  }

  n = mutt_rfc822_read_header(ifp, NULL, 1, 0);
  while ((i = fread(buffer, 1, sizeof(buffer), ifp)) > 0)
    fwrite(buffer, 1, i, ofp);
  mutt_file_fclose(&ofp);
  mutt_file_fclose(&ifp);
  mutt_file_unlink(path);

/* in case the user modifies/removes the In-Reply-To header with
     $edit_headers set, we remove References: as they're likely invalid;
     we can simply compare strings as we don't generate References for
     multiple Message-Ids in IRT anyways */
#ifdef USE_NNTP
  if (!OptNewsSend)
#endif
    if (!STAILQ_EMPTY(&msg->env->in_reply_to) &&
        (STAILQ_EMPTY(&n->in_reply_to) ||
         (mutt_str_strcmp(STAILQ_FIRST(&n->in_reply_to)->data,
                          STAILQ_FIRST(&msg->env->in_reply_to)->data) != 0)))
      mutt_list_free(&msg->env->references);

  /* restore old info. */
  mutt_list_free(&n->references);
  STAILQ_SWAP(&n->references, &msg->env->references, ListNode);

  mutt_env_free(&msg->env);
  msg->env = n;
  n = NULL;

  mutt_expand_aliases_env(msg->env);

  /* search through the user defined headers added to see if
   * fcc: or attach: or pgp: was specified
   */

  struct ListNode *np, *tmp;
  STAILQ_FOREACH_SAFE(np, &msg->env->userhdrs, entries, tmp)
  {
    bool keep = true;

    if (fcc && (mutt_str_strncasecmp("fcc:", np->data, 4) == 0))
    {
      p = mutt_str_skip_email_wsp(np->data + 4);
      if (*p)
      {
        mutt_str_strfcpy(fcc, p, fcclen);
        mutt_pretty_mailbox(fcc, fcclen);
      }
      keep = false;
    }
    else if (mutt_str_strncasecmp("attach:", np->data, 7) == 0)
    {
      struct Body *body2 = NULL;
      struct Body *parts = NULL;

      p = mutt_str_skip_email_wsp(np->data + 7);
      if (*p)
      {
        size_t l = 0;
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
        p = mutt_str_skip_email_wsp(p);
        path[l] = '\0';

        mutt_expand_path(path, sizeof(path));
        body2 = mutt_make_file_attach(path);
        if (body2)
        {
          body2->description = mutt_str_strdup(p);
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
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             (mutt_str_strncasecmp("pgp:", np->data, 4) == 0))
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
  struct HashElem *elem = mutt_hash_find_elem(ctx->label_hash, label);
  if (!elem)
    return;

  uintptr_t count = (uintptr_t) elem->data;
  if (count <= 1)
  {
    mutt_hash_delete(ctx->label_hash, label, NULL);
    return;
  }

  count--;
  elem->data = (void *) count;
}

static void label_ref_inc(struct Context *ctx, char *label)
{
  uintptr_t count;

  struct HashElem *elem = mutt_hash_find_elem(ctx->label_hash, label);
  if (!elem)
  {
    count = 1;
    mutt_hash_insert(ctx->label_hash, label, (void *) count);
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
  if (mutt_str_strcmp(hdr->env->x_label, new) == 0)
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
    mutt_str_strfcpy(buf, hdr->env->x_label, sizeof(buf));
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
    for (int i = 0; i < Context->msgcount; ++i)
    {
      if (!message_is_tagged(Context, i))
        continue;

      struct Header *h = Context->hdrs[i];
      if (label_message(Context, h, new))
      {
        changed++;
        mutt_set_flag(Context, h, MUTT_TAG, 0);
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
  ctx->label_hash = mutt_hash_create(131, MUTT_HASH_STRDUP_KEYS);
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

void mutt_header_free(struct Header **h)
{
  if (!h || !*h)
    return;
  mutt_env_free(&(*h)->env);
  mutt_body_free(&(*h)->content);
  FREE(&(*h)->maildir_flags);
  FREE(&(*h)->tree);
  FREE(&(*h)->path);
#ifdef MIXMASTER
  mutt_list_free(&(*h)->chain);
#endif
  driver_tags_free(&(*h)->tags);
#if defined(USE_POP) || defined(USE_IMAP) || defined(USE_NNTP) || defined(USE_NOTMUCH)
  if ((*h)->free_cb)
    (*h)->free_cb(*h);
  FREE(&(*h)->data);
#endif
  FREE(h);
}

struct Header *mutt_header_new(void)
{
  struct Header *h = mutt_mem_calloc(1, sizeof(struct Header));
#ifdef MIXMASTER
  STAILQ_INIT(&h->chain);
#endif
  STAILQ_INIT(&h->tags);
  return h;
}
