/**
 * Copyright (C) 1996-2009,2012 Michael R. Elkins <me@mutt.org>
 *
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
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"

void mutt_edit_headers(const char *editor, const char *body, HEADER *msg, char *fcc, size_t fcclen)
{
  char path[_POSIX_PATH_MAX]; /* tempfile used to edit headers + body */
  char buffer[LONG_STRING];
  const char *p = NULL;
  FILE *ifp = NULL, *ofp = NULL;
  int i, keep;
  ENVELOPE *n = NULL;
  time_t mtime;
  struct stat st;
  LIST *cur = NULL, **last = NULL, *tmp = NULL;

  mutt_mktemp(path, sizeof(path));
  if ((ofp = safe_fopen(path, "w")) == NULL)
  {
    mutt_perror(path);
    return;
  }

  mutt_env_to_local(msg->env);
  mutt_write_rfc822_header(ofp, msg->env, NULL, 1, 0);
  fputc('\n', ofp); /* tie off the header. */

  /* now copy the body of the message. */
  if ((ifp = fopen(body, "r")) == NULL)
  {
    mutt_perror(body);
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
  mutt_free_list(&msg->env->userhdrs);

  /* Read the temp file back in */
  if ((ifp = fopen(path, "r")) == NULL)
  {
    mutt_perror(path);
    return;
  }

  if ((ofp = safe_fopen(body, "w")) == NULL)
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
  if (!option(OPTNEWSSEND))
#endif
    if (msg->env->in_reply_to &&
        (!n->in_reply_to ||
         (mutt_strcmp(n->in_reply_to->data, msg->env->in_reply_to->data) != 0)))
      mutt_free_list(&msg->env->references);

  /* restore old info. */
  mutt_free_list(&n->references);
  n->references = msg->env->references;
  msg->env->references = NULL;

  mutt_free_envelope(&msg->env);
  msg->env = n;
  n = NULL;

  mutt_expand_aliases_env(msg->env);

  /* search through the user defined headers added to see if
   * fcc: or attach: or pgp: was specified
   */

  cur = msg->env->userhdrs;
  last = &msg->env->userhdrs;
  while (cur)
  {
    keep = 1;

    if (fcc && (ascii_strncasecmp("fcc:", cur->data, 4) == 0))
    {
      p = skip_email_wsp(cur->data + 4);
      if (*p)
      {
        strfcpy(fcc, p, fcclen);
        mutt_pretty_mailbox(fcc, fcclen);
      }
      keep = 0;
    }
    else if (ascii_strncasecmp("attach:", cur->data, 7) == 0)
    {
      BODY *body2 = NULL;
      BODY *parts = NULL;
      size_t l = 0;

      p = skip_email_wsp(cur->data + 7);
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
        path[l] = 0;

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
      keep = 0;
    }
    else if ((WithCrypto & APPLICATION_PGP) &&
             (ascii_strncasecmp("pgp:", cur->data, 4) == 0))
    {
      msg->security = mutt_parse_crypt_hdr(cur->data + 4, 0, APPLICATION_PGP);
      if (msg->security)
        msg->security |= APPLICATION_PGP;
      keep = 0;
    }

    if (keep)
    {
      last = &cur->next;
      cur = cur->next;
    }
    else
    {
      tmp = cur;
      *last = cur->next;
      cur = cur->next;
      tmp->next = NULL;
      mutt_free_list(&tmp);
    }
  }
}

static void label_ref_dec(CONTEXT *ctx, char *label)
{
  struct hash_elem *elem = NULL;
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

static void label_ref_inc(CONTEXT *ctx, char *label)
{
  struct hash_elem *elem = NULL;
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

/*
 * add an X-Label: field.
 */
static int label_message(CONTEXT *ctx, HEADER *hdr, char *new)
{
  if (hdr == NULL)
    return 0;
  if (mutt_strcmp(hdr->env->x_label, new) == 0)
    return 0;

  if (hdr->env->x_label != NULL)
    label_ref_dec(ctx, hdr->env->x_label);
  mutt_str_replace(&hdr->env->x_label, new);
  if (hdr->env->x_label != NULL)
    label_ref_inc(ctx, hdr->env->x_label);

  return hdr->changed = hdr->xlabel_changed = true;
}

int mutt_label_message(HEADER *hdr)
{
  char buf[LONG_STRING], *new = NULL;
  int i;
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
  if (hdr != NULL)
  {
    if (label_message(Context, hdr, new))
    {
      ++changed;
      mutt_set_header_color(Context, hdr);
    }
  }
  else
  {
#define HDR_OF(index) Context->hdrs[Context->v2r[(index)]]
    for (i = 0; i < Context->vcount; ++i)
    {
      if (HDR_OF(i)->tagged)
        if (label_message(Context, HDR_OF(i), new))
        {
          ++changed;
          mutt_set_flag(Context, HDR_OF(i), MUTT_TAG, 0);
          /* mutt_set_flag re-evals the header color */
        }
    }
  }

  return changed;
}

void mutt_make_label_hash(CONTEXT *ctx)
{
  /* 131 is just a rough prime estimate of how many distinct
   * labels someone might have in a mailbox.
   */
  ctx->label_hash = hash_create(131, MUTT_HASH_STRDUP_KEYS);
}

void mutt_label_hash_add(CONTEXT *ctx, HEADER *hdr)
{
  if (!ctx || !ctx->label_hash)
    return;
  if (hdr->env->x_label)
    label_ref_inc(ctx, hdr->env->x_label);
}

void mutt_label_hash_remove(CONTEXT *ctx, HEADER *hdr)
{
  if (!ctx || !ctx->label_hash)
    return;
  if (hdr->env->x_label)
    label_ref_dec(ctx, hdr->env->x_label);
}
