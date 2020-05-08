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

/**
 * @page mutt_header Manipulate an email's header
 *
 * Manipulate an email's header
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "mutt_header.h"
#include "index.h"
#include "muttlib.h"
#include "options.h"
#include "protos.h"
#include "ncrypt/lib.h"
#include "send/lib.h"

/**
 * label_ref_dec - Decrease the refcount of a label
 * @param m     Mailbox
 * @param label Label
 */
static void label_ref_dec(struct Mailbox *m, char *label)
{
  struct HashElem *elem = mutt_hash_find_elem(m->label_hash, label);
  if (!elem)
    return;

  uintptr_t count = (uintptr_t) elem->data;
  if (count <= 1)
  {
    mutt_hash_delete(m->label_hash, label, NULL);
    return;
  }

  count--;
  elem->data = (void *) count;
}

/**
 * label_ref_inc - Increase the refcount of a label
 * @param m     Mailbox
 * @param label Label
 */
static void label_ref_inc(struct Mailbox *m, char *label)
{
  uintptr_t count;

  struct HashElem *elem = mutt_hash_find_elem(m->label_hash, label);
  if (!elem)
  {
    count = 1;
    mutt_hash_insert(m->label_hash, label, (void *) count);
    return;
  }

  count = (uintptr_t) elem->data;
  count++;
  elem->data = (void *) count;
}

/**
 * label_message - add an X-Label: field
 * @param[in]  m         Mailbox
 * @param[in]  e         Email
 * @param[out] new_label Set to true if this is a new label
 * @retval true If the label was added
 */
static bool label_message(struct Mailbox *m, struct Email *e, char *new_label)
{
  if (!e)
    return false;
  if (mutt_str_equal(e->env->x_label, new_label))
    return false;

  if (e->env->x_label)
    label_ref_dec(m, e->env->x_label);
  if (mutt_str_replace(&e->env->x_label, new_label))
    label_ref_inc(m, e->env->x_label);

  e->changed = true;
  e->env->changed |= MUTT_ENV_CHANGED_XLABEL;
  return true;
}

/**
 * mutt_label_message - Let the user label a message
 * @param m  Mailbox
 * @param el List of Emails to label
 * @retval num Number of messages changed
 */
int mutt_label_message(struct Mailbox *m, struct EmailList *el)
{
  if (!m || !el)
    return 0;

  char buf[1024] = { 0 };

  struct EmailNode *en = STAILQ_FIRST(el);
  if (!STAILQ_NEXT(en, entries))
  {
    // If there's only one email, use its label as a template
    if (en->email->env->x_label)
      mutt_str_copy(buf, en->email->env->x_label, sizeof(buf));
  }

  if (mutt_get_field("Label: ", buf, sizeof(buf), MUTT_LABEL /* | MUTT_CLEAR */) != 0)
    return 0;

  char *new_label = buf;
  SKIPWS(new_label);
  if (*new_label == '\0')
    new_label = NULL;

  int changed = 0;
  STAILQ_FOREACH(en, el, entries)
  {
    if (label_message(m, en->email, new_label))
    {
      changed++;
      mutt_set_header_color(m, en->email);
    }
  }

  return changed;
}

/**
 * mutt_edit_headers - Let the user edit the message header and body
 * @param editor Editor command
 * @param body   File containing message body
 * @param e      Email
 * @param fcc    Buffer for the fcc field
 */
void mutt_edit_headers(const char *editor, const char *body, struct Email *e,
                       struct Buffer *fcc)
{
  char buf[1024];
  const char *p = NULL;
  int i;
  struct Envelope *n = NULL;
  time_t mtime;
  struct stat st;

  struct Buffer *path = mutt_buffer_pool_get();
  mutt_buffer_mktemp(path);
  FILE *fp_out = mutt_file_fopen(mutt_b2s(path), "w");
  if (!fp_out)
  {
    mutt_perror(mutt_b2s(path));
    goto cleanup;
  }

  mutt_env_to_local(e->env);
  mutt_rfc822_write_header(fp_out, e->env, NULL, MUTT_WRITE_HEADER_EDITHDRS,
                           false, false, NeoMutt->sub);
  fputc('\n', fp_out); /* tie off the header. */

  /* now copy the body of the message. */
  FILE *fp_in = fopen(body, "r");
  if (!fp_in)
  {
    mutt_perror(body);
    mutt_file_fclose(&fp_out);
    goto cleanup;
  }

  mutt_file_copy_stream(fp_in, fp_out);

  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);

  if (stat(mutt_b2s(path), &st) == -1)
  {
    mutt_perror(mutt_b2s(path));
    goto cleanup;
  }

  mtime = mutt_file_decrease_mtime(mutt_b2s(path), &st);
  if (mtime == (time_t) -1)
  {
    mutt_perror(mutt_b2s(path));
    goto cleanup;
  }

  mutt_edit_file(editor, mutt_b2s(path));
  stat(mutt_b2s(path), &st);
  if (mtime == st.st_mtime)
  {
    mutt_debug(LL_DEBUG1, "temp file was not modified\n");
    /* the file has not changed! */
    mutt_file_unlink(mutt_b2s(path));
    goto cleanup;
  }

  mutt_file_unlink(body);
  mutt_list_free(&e->env->userhdrs);

  /* Read the temp file back in */
  fp_in = fopen(mutt_b2s(path), "r");
  if (!fp_in)
  {
    mutt_perror(mutt_b2s(path));
    goto cleanup;
  }

  fp_out = mutt_file_fopen(body, "w");
  if (!fp_out)
  {
    /* intentionally leak a possible temporary file here */
    mutt_file_fclose(&fp_in);
    mutt_perror(body);
    goto cleanup;
  }

  n = mutt_rfc822_read_header(fp_in, NULL, true, false);
  while ((i = fread(buf, 1, sizeof(buf), fp_in)) > 0)
    fwrite(buf, 1, i, fp_out);
  mutt_file_fclose(&fp_out);
  mutt_file_fclose(&fp_in);
  mutt_file_unlink(mutt_b2s(path));

  /* in case the user modifies/removes the In-Reply-To header with
   * $edit_headers set, we remove References: as they're likely invalid;
   * we can simply compare strings as we don't generate References for
   * multiple Message-Ids in IRT anyways */
#ifdef USE_NNTP
  if (!OptNewsSend)
#endif
  {
    if (!STAILQ_EMPTY(&e->env->in_reply_to) &&
        (STAILQ_EMPTY(&n->in_reply_to) ||
         !mutt_str_equal(STAILQ_FIRST(&n->in_reply_to)->data,
                         STAILQ_FIRST(&e->env->in_reply_to)->data)))
    {
      mutt_list_free(&e->env->references);
    }
  }

  /* restore old info. */
  mutt_list_free(&n->references);
  STAILQ_SWAP(&n->references, &e->env->references, ListNode);

  mutt_env_free(&e->env);
  e->env = n;
  n = NULL;

  mutt_expand_aliases_env(e->env);

  /* search through the user defined headers added to see if
   * fcc: or attach: or pgp: was specified */

  struct ListNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &e->env->userhdrs, entries, tmp)
  {
    bool keep = true;
    size_t plen;

    if (fcc && (plen = mutt_istr_startswith(np->data, "fcc:")))
    {
      p = mutt_str_skip_email_wsp(np->data + plen);
      if (*p)
      {
        mutt_buffer_strcpy(fcc, p);
        mutt_buffer_pretty_mailbox(fcc);
      }
      keep = false;
    }
    else if ((plen = mutt_istr_startswith(np->data, "attach:")))
    {
      struct Body *body2 = NULL;
      struct Body *parts = NULL;

      p = mutt_str_skip_email_wsp(np->data + plen);
      if (*p)
      {
        mutt_buffer_reset(path);
        for (; (p[0] != '\0') && (p[0] != ' ') && (p[0] != '\t'); p++)
        {
          if (p[0] == '\\')
          {
            if (p[1] == '\0')
              break;
            p++;
          }
          mutt_buffer_addch(path, *p);
        }
        p = mutt_str_skip_email_wsp(p);

        mutt_buffer_expand_path(path);
        body2 = mutt_make_file_attach(mutt_b2s(path), NeoMutt->sub);
        if (body2)
        {
          body2->description = mutt_str_dup(p);
          for (parts = e->content; parts->next; parts = parts->next)
            ; // do nothing

          parts->next = body2;
        }
        else
        {
          mutt_buffer_pretty_mailbox(path);
          mutt_error(_("%s: unable to attach file"), mutt_b2s(path));
        }
      }
      keep = false;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             (plen = mutt_istr_startswith(np->data, "pgp:")))
    {
      e->security = mutt_parse_crypt_hdr(np->data + plen, false, APPLICATION_PGP);
      if (e->security)
        e->security |= APPLICATION_PGP;
      keep = false;
    }

    if (!keep)
    {
      STAILQ_REMOVE(&e->env->userhdrs, np, ListNode, entries);
      FREE(&np->data);
      FREE(&np);
    }
  }

cleanup:
  mutt_buffer_pool_release(&path);
}

/**
 * mutt_make_label_hash - Create a Hash Table to store the labels
 * @param m Mailbox
 */
void mutt_make_label_hash(struct Mailbox *m)
{
  /* 131 is just a rough prime estimate of how many distinct
   * labels someone might have in a m.  */
  m->label_hash = mutt_hash_new(131, MUTT_HASH_STRDUP_KEYS);
}

/**
 * mutt_label_hash_add - Add a message's labels to the Hash Table
 * @param m Mailbox
 * @param e Email
 */
void mutt_label_hash_add(struct Mailbox *m, struct Email *e)
{
  if (!m || !m->label_hash)
    return;
  if (e->env->x_label)
    label_ref_inc(m, e->env->x_label);
}

/**
 * mutt_label_hash_remove - Remove a message's labels from the Hash Table
 * @param m Mailbox
 * @param e Email
 */
void mutt_label_hash_remove(struct Mailbox *m, struct Email *e)
{
  if (!m || !m->label_hash)
    return;
  if (e->env->x_label)
    label_ref_dec(m, e->env->x_label);
}
