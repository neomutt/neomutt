/**
 * @file
 * Manipulate an email's header
 *
 * @authors
 * Copyright (C) 2017-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Rayford Shireman
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
 * @page neo_mutt_header Manipulate an email's header
 *
 * Manipulate an email's header
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
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
#include "complete/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "index/lib.h"
#include "ncrypt/lib.h"
#include "postpone/lib.h"
#include "send/lib.h"
#include "globals.h"
#include "muttlib.h"
#include "mview.h"

/**
 * label_ref_dec - Decrease the refcount of a label
 * @param m     Mailbox
 * @param label Label
 */
static void label_ref_dec(struct Mailbox *m, char *label)
{
  struct HashElem *he = mutt_hash_find_elem(m->label_hash, label);
  if (!he)
    return;

  uintptr_t count = (uintptr_t) he->data;
  if (count <= 1)
  {
    mutt_hash_delete(m->label_hash, label, NULL);
    return;
  }

  count--;
  he->data = (void *) count;
}

/**
 * label_ref_inc - Increase the refcount of a label
 * @param m     Mailbox
 * @param label Label
 */
static void label_ref_inc(struct Mailbox *m, char *label)
{
  uintptr_t count;

  struct HashElem *he = mutt_hash_find_elem(m->label_hash, label);
  if (!he)
  {
    count = 1;
    mutt_hash_insert(m->label_hash, label, (void *) count);
    return;
  }

  count = (uintptr_t) he->data;
  count++;
  he->data = (void *) count;
}

/**
 * label_message - Add an X-Label: field
 * @param[in]  m         Mailbox
 * @param[in]  e         Email
 * @param[out] new_label Set to true if this is a new label
 * @retval true The label was added
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
 * @param mv Mailbox
 * @param ea Array of Emails to label
 * @retval num Number of messages changed
 */
int mutt_label_message(struct MailboxView *mv, struct EmailArray *ea)
{
  if (!mv || !mv->mailbox || !ea)
    return 0;

  struct Mailbox *m = mv->mailbox;

  int changed = 0;
  struct Buffer *buf = buf_pool_get();

  struct Email **ep = ARRAY_GET(ea, 0);
  if (ARRAY_SIZE(ea) == 1)
  {
    // If there's only one email, use its label as a template
    struct Email *e = *ep;
    if (e->env->x_label)
      buf_strcpy(buf, e->env->x_label);
  }

  if (mw_get_field("Label: ", buf, MUTT_COMP_NO_FLAGS, HC_OTHER, &CompleteLabelOps, NULL) != 0)
  {
    goto done;
  }

  char *new_label = buf->data;
  SKIPWS(new_label);
  if (*new_label == '\0')
    new_label = NULL;

  ARRAY_FOREACH(ep, ea)
  {
    struct Email *e = *ep;
    if (label_message(m, e, new_label))
    {
      changed++;
      email_set_color(m, e);
    }
  }

done:
  buf_pool_release(&buf);
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
  struct Buffer *path = buf_pool_get();
  buf_mktemp(path);
  FILE *fp_out = mutt_file_fopen(buf_string(path), "w");
  if (!fp_out)
  {
    mutt_perror("%s", buf_string(path));
    goto cleanup;
  }

  mutt_env_to_local(e->env);
  mutt_rfc822_write_header(fp_out, e->env, NULL, MUTT_WRITE_HEADER_EDITHDRS,
                           false, false, NeoMutt->sub);
  fputc('\n', fp_out); /* tie off the header. */

  /* now copy the body of the message. */
  FILE *fp_in = mutt_file_fopen(body, "r");
  if (!fp_in)
  {
    mutt_perror("%s", body);
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(buf_string(path));
    goto cleanup;
  }

  mutt_file_copy_stream(fp_in, fp_out);

  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);

  struct stat st = { 0 };
  if (stat(buf_string(path), &st) == -1)
  {
    mutt_perror("%s", buf_string(path));
    goto cleanup;
  }

  time_t mtime = mutt_file_decrease_mtime(buf_string(path), &st);
  if (mtime == (time_t) -1)
  {
    mutt_perror("%s", buf_string(path));
    goto cleanup;
  }

  mutt_edit_file(editor, buf_string(path));
  if ((stat(buf_string(path), &st) != 0) || (mtime == st.st_mtime))
  {
    mutt_debug(LL_DEBUG1, "temp file was not modified\n");
    /* the file has not changed! */
    mutt_file_unlink(buf_string(path));
    goto cleanup;
  }

  mutt_file_unlink(body);
  mutt_list_free(&e->env->userhdrs);

  /* Read the temp file back in */
  fp_in = mutt_file_fopen(buf_string(path), "r");
  if (!fp_in)
  {
    mutt_perror("%s", buf_string(path));
    mutt_file_unlink(buf_string(path));
    goto cleanup;
  }

  fp_out = mutt_file_fopen(body, "w");
  if (!fp_out)
  {
    mutt_file_fclose(&fp_in);
    mutt_file_unlink(buf_string(path));
    mutt_perror("%s", body);
    goto cleanup;
  }

  struct Envelope *env_new = NULL;
  char buf[1024] = { 0 };
  env_new = mutt_rfc822_read_header(fp_in, NULL, true, false);
  int bytes_read;
  while ((bytes_read = fread(buf, 1, sizeof(buf), fp_in)) > 0)
    fwrite(buf, 1, bytes_read, fp_out);
  mutt_file_fclose(&fp_out);
  mutt_file_fclose(&fp_in);
  mutt_file_unlink(buf_string(path));

  /* in case the user modifies/removes the In-Reply-To header with
   * $edit_headers set, we remove References: as they're likely invalid;
   * we can simply compare strings as we don't generate References for
   * multiple Message-Ids in IRT anyways */
  if (!OptNewsSend)
  {
    if (!STAILQ_EMPTY(&e->env->in_reply_to) &&
        (STAILQ_EMPTY(&env_new->in_reply_to) ||
         !mutt_str_equal(STAILQ_FIRST(&env_new->in_reply_to)->data,
                         STAILQ_FIRST(&e->env->in_reply_to)->data)))
    {
      mutt_list_free(&e->env->references);
    }
  }

  /* restore old info. */
  mutt_list_free(&env_new->references);
  STAILQ_SWAP(&env_new->references, &e->env->references, ListNode);

  mutt_env_free(&e->env);
  e->env = env_new;
  env_new = NULL;

  mutt_expand_aliases_env(e->env);

  /* search through the user defined headers added to see if
   * fcc: or attach: or pgp: or smime: was specified */

  struct ListNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &e->env->userhdrs, entries, tmp)
  {
    bool keep = true;
    size_t plen = 0;

    // Check for header names: most specific first
    if (fcc && ((plen = mutt_istr_startswith(np->data, "X-Mutt-Fcc:")) ||
                (plen = mutt_istr_startswith(np->data, "Mutt-Fcc:")) ||
                (plen = mutt_istr_startswith(np->data, "fcc:"))))
    {
      const char *p = mutt_str_skip_email_wsp(np->data + plen);
      if (*p)
      {
        buf_strcpy(fcc, p);
        buf_pretty_mailbox(fcc);
      }
      keep = false;
    }
    // Check for header names: most specific first
    else if ((plen = mutt_istr_startswith(np->data, "X-Mutt-Attach:")) ||
             (plen = mutt_istr_startswith(np->data, "Mutt-Attach:")) ||
             (plen = mutt_istr_startswith(np->data, "attach:")))
    {
      struct Body *body2 = NULL;
      struct Body *parts = NULL;

      const char *p = mutt_str_skip_email_wsp(np->data + plen);
      if (*p)
      {
        buf_reset(path);
        for (; (p[0] != '\0') && (p[0] != ' ') && (p[0] != '\t'); p++)
        {
          if (p[0] == '\\')
          {
            if (p[1] == '\0')
              break;
            p++;
          }
          buf_addch(path, *p);
        }
        p = mutt_str_skip_email_wsp(p);

        buf_expand_path(path);
        body2 = mutt_make_file_attach(buf_string(path), NeoMutt->sub);
        if (body2)
        {
          body2->description = mutt_str_dup(p);
          for (parts = e->body; parts->next; parts = parts->next)
            ; // do nothing

          parts->next = body2;
        }
        else
        {
          buf_pretty_mailbox(path);
          mutt_error(_("%s: unable to attach file"), buf_string(path));
        }
      }
      keep = false;
    }
    // Check for header names: most specific first
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             ((plen = mutt_istr_startswith(np->data, "X-Mutt-PGP:")) ||
              (plen = mutt_istr_startswith(np->data, "Mutt-PGP:")) ||
              (plen = mutt_istr_startswith(np->data, "pgp:"))))
    {
      SecurityFlags sec = mutt_parse_crypt_hdr(np->data + plen, false, APPLICATION_PGP);
      if (sec != SEC_NO_FLAGS)
        sec |= APPLICATION_PGP;
      if (sec != e->security)
      {
        e->security = sec;
        notify_send(e->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
      }
      keep = false;
    }
    // Check for header names: most specific first
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             ((plen = mutt_istr_startswith(np->data, "X-Mutt-SMIME:")) ||
              (plen = mutt_istr_startswith(np->data, "Mutt-SMIME:")) ||
              (plen = mutt_istr_startswith(np->data, "smime:"))))
    {
      SecurityFlags sec = mutt_parse_crypt_hdr(np->data + plen, false, APPLICATION_SMIME);
      if (sec != SEC_NO_FLAGS)
        sec |= APPLICATION_SMIME;
      if (sec != e->security)
      {
        e->security = sec;
        notify_send(e->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
      }
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
  buf_pool_release(&path);
}

/**
 * mutt_make_label_hash - Create a Hash Table to store the labels
 * @param m Mailbox
 */
void mutt_make_label_hash(struct Mailbox *m)
{
  /* 131 is just a rough prime estimate of how many distinct
   * labels someone might have in a mailbox.  */
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
