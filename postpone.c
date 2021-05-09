/**
 * @file
 * Save/restore and GUI list postponed emails
 *
 * @authors
 * Copyright (C) 1996-2002,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002,2004 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page neo_postpone Save/restore and GUI list postponed emails
 *
 * Save/restore and GUI list postponed emails
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "pattern/lib.h"
#include "send/lib.h"
#include "format_flags.h"
#include "handler.h"
#include "hdrline.h"
#include "keymap.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "rfc3676.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/// Help Bar for the Postponed email selection dialog
static const struct Mapping PostponeHelp[] = {
  // clang-format off
  { N_("Exit"),  OP_EXIT },
  { N_("Del"),   OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Help"),  OP_HELP },
  { NULL, 0 },
  // clang-format on
};

static short PostCount = 0;
static bool UpdateNumPostponed = false;

/**
 * mutt_num_postponed - Return the number of postponed messages
 * @param m    currently selected mailbox
 * @param force
 * * false Use a cached value if costly to get a fresh count (IMAP)
 * * true Force check
 * @retval num Postponed messages
 */
int mutt_num_postponed(struct Mailbox *m, bool force)
{
  struct stat st;

  static time_t LastModify = 0;
  static char *OldPostponed = NULL;

  if (UpdateNumPostponed)
  {
    UpdateNumPostponed = false;
    force = true;
  }

  const char *const c_postponed = cs_subset_string(NeoMutt->sub, "postponed");
  if (!mutt_str_equal(c_postponed, OldPostponed))
  {
    FREE(&OldPostponed);
    OldPostponed = mutt_str_dup(c_postponed);
    LastModify = 0;
    force = true;
  }

  if (!c_postponed)
    return 0;

  // We currently are in the `$postponed` mailbox so just pick the current status
  if (m && mutt_str_equal(c_postponed, m->realpath))
  {
    PostCount = m->msg_count - m->msg_deleted;
    return PostCount;
  }

#ifdef USE_IMAP
  /* LastModify is useless for IMAP */
  if (imap_path_probe(c_postponed, NULL) == MUTT_IMAP)
  {
    if (force)
    {
      short newpc;

      newpc = imap_path_status(c_postponed, false);
      if (newpc >= 0)
      {
        PostCount = newpc;
        mutt_debug(LL_DEBUG3, "%d postponed IMAP messages found\n", PostCount);
      }
      else
        mutt_debug(LL_DEBUG3, "using old IMAP postponed count\n");
    }
    return PostCount;
  }
#endif

  if (stat(c_postponed, &st) == -1)
  {
    PostCount = 0;
    LastModify = 0;
    return 0;
  }

  if (S_ISDIR(st.st_mode))
  {
    /* if we have a maildir mailbox, we need to stat the "new" dir */
    struct Buffer *buf = mutt_buffer_pool_get();

    mutt_buffer_printf(buf, "%s/new", c_postponed);
    if ((access(mutt_buffer_string(buf), F_OK) == 0) &&
        (stat(mutt_buffer_string(buf), &st) == -1))
    {
      PostCount = 0;
      LastModify = 0;
      mutt_buffer_pool_release(&buf);
      return 0;
    }
    mutt_buffer_pool_release(&buf);
  }

  if (LastModify < st.st_mtime)
  {
#ifdef USE_NNTP
    int optnews = OptNews;
#endif
    LastModify = st.st_mtime;

    if (access(c_postponed, R_OK | F_OK) != 0)
      return PostCount = 0;
#ifdef USE_NNTP
    if (optnews)
      OptNews = false;
#endif
    struct Mailbox *m_post = mx_path_resolve(c_postponed);
    if (mx_mbox_open(m_post, MUTT_NOSORT | MUTT_QUIET))
    {
      PostCount = m_post->msg_count;
    }
    else
    {
      mailbox_free(&m_post);
      PostCount = 0;
    }
    mx_fastclose_mailbox(m_post);
#ifdef USE_NNTP
    if (optnews)
      OptNews = true;
#endif
  }

  return PostCount;
}

/**
 * mutt_update_num_postponed - Force the update of the number of postponed messages
 */
void mutt_update_num_postponed(void)
{
  UpdateNumPostponed = true;
}

/**
 * post_make_entry - Format a menu item for the email list - Implements Menu::make_entry()
 */
static void post_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct Mailbox *m = menu->mdata;

  const char *const c_index_format =
      cs_subset_string(NeoMutt->sub, "index_format");
  mutt_make_string(buf, buflen, menu->win_index->state.cols, NONULL(c_index_format),
                   m, -1, m->emails[line], MUTT_FORMAT_ARROWCURSOR, NULL);
}

/**
 * dlg_select_postponed_email - Create a Menu to select a postponed message
 * @param m Mailbox
 * @retval ptr Email
 */
static struct Email *dlg_select_postponed_email(struct Mailbox *m)
{
  int r = -1;
  bool done = false;

  struct MuttWindow *dlg =
      dialog_create_simple_index(MENU_POSTPONE, WT_DLG_POSTPONE, PostponeHelp);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = post_make_entry;
  menu->max = m->msg_count;
  menu->mdata = m;
  menu->custom_search = true;

  struct MuttWindow *sbar = mutt_window_find(dlg, WT_INDEX_BAR);
  sbar_set_title(sbar, _("Postponed Messages"));

  /* The postponed mailbox is setup to have sorting disabled, but the global
   * `$sort` variable may indicate something different.   Sorting has to be
   * disabled while the postpone menu is being displayed. */
  const short c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);

  while (!done)
  {
    const int op = menu_loop(menu);
    switch (op)
    {
      case OP_DELETE:
      case OP_UNDELETE:
      {
        const int index = menu_get_index(menu);
        /* should deleted draft messages be saved in the trash folder? */
        mutt_set_flag(m, m->emails[index], MUTT_DELETE, (op == OP_DELETE));
        PostCount = m->msg_count - m->msg_deleted;
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve && (index < (menu->max - 1)))
        {
          menu_set_index(menu, index + 1);
          if (index >= (menu->top + menu->pagelen))
          {
            menu->top = index;
            menu_queue_redraw(menu, REDRAW_INDEX | REDRAW_STATUS);
          }
        }
        else
          menu_queue_redraw(menu, REDRAW_CURRENT);
        break;
      }

      // All search operations must exist to show the menu
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
      case OP_SEARCH:
      {
        int index = menu_get_index(menu);
        index = mutt_search_command(m, menu, index, op);
        if (index != -1)
          menu_set_index(menu, index);
        break;
      }

      case OP_GENERIC_SELECT_ENTRY:
        r = menu_get_index(menu);
        done = true;
        break;

      case OP_EXIT:
        done = true;
        break;
    }
  }

  cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
  dialog_destroy_simple_index(&dlg);

  return (r > -1) ? m->emails[r] : NULL;
}

/**
 * hardclose - try hard to close a mailbox
 * @param m Mailbox to close
 */
static void hardclose(struct Mailbox *m)
{
  /* messages might have been marked for deletion.
   * try once more on reopen before giving up. */
  enum MxStatus rc = mx_mbox_close(m);
  if (rc != MX_STATUS_ERROR && rc != MX_STATUS_OK)
    rc = mx_mbox_close(m);
  if (rc != MX_STATUS_OK)
    mx_fastclose_mailbox(m);
}

/**
 * mutt_get_postponed - Recall a postponed message
 * @param[in]  m_cur   Current mailbox
 * @param[in]  hdr     envelope/attachment info for recalled message
 * @param[out] cur     if message was a reply, 'cur' is set to the message which 'hdr' is in reply to
 * @param[in]  fcc     fcc for the recalled message
 * @retval -1         Error/no messages
 * @retval 0          Normal exit
 * @retval #SEND_REPLY Recalled message is a reply
 */
int mutt_get_postponed(struct Mailbox *m_cur, struct Email *hdr,
                       struct Email **cur, struct Buffer *fcc)
{
  const char *const c_postponed = cs_subset_string(NeoMutt->sub, "postponed");
  if (!c_postponed)
    return -1;

  struct Email *e = NULL;
  int rc = SEND_POSTPONED;
  const char *p = NULL;

  struct Mailbox *m = mx_path_resolve(c_postponed);
  if (m_cur != m)
  {
    if (!mx_mbox_open(m, MUTT_NOSORT))
    {
      PostCount = 0;
      mutt_error(_("No postponed messages"));
      mailbox_free(&m);
      return -1;
    }
  }

  /* TODO:
   * mx_mbox_open() for IMAP leaves IMAP_REOPEN_ALLOW set.  For the
   * index this is papered-over because it calls mx_check_mailbox()
   * every event loop(which resets that flag).
   *
   * For a stable-branch fix, I'm doing the same here, to prevent
   * context changes from occuring behind the scenes and causing
   * segvs, but probably the flag needs to be reset after downloading
   * headers in imap_open_mailbox().
   */
  mx_mbox_check(m);

  if (m->msg_count == 0)
  {
    PostCount = 0;
    if (m_cur != m)
      mx_fastclose_mailbox(m);
    mutt_error(_("No postponed messages"));
    return -1;
  }

  if (m->msg_count == 1)
  {
    /* only one message, so just use that one. */
    e = m->emails[0];
  }
  else if (!(e = dlg_select_postponed_email(m)))
  {
    if (m_cur != m)
    {
      hardclose(m);
    }
    return -1;
  }

  if (mutt_prepare_template(NULL, m, hdr, e, false) < 0)
  {
    if (m_cur != m)
    {
      hardclose(m);
    }
    return -1;
  }

  /* finished with this message, so delete it. */
  mutt_set_flag(m, e, MUTT_DELETE, true);
  mutt_set_flag(m, e, MUTT_PURGE, true);

  /* update the count for the status display */
  PostCount = m->msg_count - m->msg_deleted;

  /* avoid the "purge deleted messages" prompt */
  const enum QuadOption c_delete = cs_subset_quad(NeoMutt->sub, "delete");
  cs_subset_str_native_set(NeoMutt->sub, "delete", MUTT_YES, NULL);
  if (m_cur != m)
  {
    hardclose(m);
  }
  cs_subset_str_native_set(NeoMutt->sub, "delete", c_delete, NULL);

  struct ListNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &hdr->env->userhdrs, entries, tmp)
  {
    size_t plen = mutt_istr_startswith(np->data, "X-Mutt-References:");
    if (plen)
    {
      /* if a mailbox is currently open, look to see if the original message
       * the user attempted to reply to is in this mailbox */
      p = mutt_str_skip_email_wsp(np->data + plen);
      if (!m_cur->id_hash)
        m_cur->id_hash = mutt_make_id_hash(m_cur);
      *cur = mutt_hash_find(m_cur->id_hash, p);

      if (*cur)
        rc |= SEND_REPLY;
    }
    else if ((plen = mutt_istr_startswith(np->data, "X-Mutt-Fcc:")))
    {
      p = mutt_str_skip_email_wsp(np->data + plen);
      mutt_buffer_strcpy(fcc, p);
      mutt_buffer_pretty_mailbox(fcc);

      /* note that x-mutt-fcc was present.  we do this because we want to add a
       * default fcc if the header was missing, but preserve the request of the
       * user to not make a copy if the header field is present, but empty.
       * see http://dev.mutt.org/trac/ticket/3653 */
      rc |= SEND_POSTPONED_FCC;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             /* this is generated by old neomutt versions */
             (mutt_str_startswith(np->data, "Pgp:") ||
              /* this is the new way */
              mutt_str_startswith(np->data, "X-Mutt-PGP:")))
    {
      hdr->security = mutt_parse_crypt_hdr(strchr(np->data, ':') + 1, true, APPLICATION_PGP);
      hdr->security |= APPLICATION_PGP;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             mutt_str_startswith(np->data, "X-Mutt-SMIME:"))
    {
      hdr->security = mutt_parse_crypt_hdr(strchr(np->data, ':') + 1, true, APPLICATION_SMIME);
      hdr->security |= APPLICATION_SMIME;
    }
#ifdef MIXMASTER
    else if (mutt_str_startswith(np->data, "X-Mutt-Mix:"))
    {
      mutt_list_free(&hdr->chain);

      char *t = strtok(np->data + 11, " \t\n");
      while (t)
      {
        mutt_list_insert_tail(&hdr->chain, mutt_str_dup(t));
        t = strtok(NULL, " \t\n");
      }
    }
#endif
    else
    {
      // skip header removal
      continue;
    }

    // remove the header
    STAILQ_REMOVE(&hdr->env->userhdrs, np, ListNode, entries);
    FREE(&np->data);
    FREE(&np);
  }

  const bool c_crypt_opportunistic_encrypt =
      cs_subset_bool(NeoMutt->sub, "crypt_opportunistic_encrypt");
  if (c_crypt_opportunistic_encrypt)
    crypt_opportunistic_encrypt(m_cur, hdr);

  return rc;
}

/**
 * mutt_parse_crypt_hdr - Parse a crypto header string
 * @param p                Header string to parse
 * @param set_empty_signas Allow an empty "Sign as"
 * @param crypt_app App, e.g. #APPLICATION_PGP
 * @retval num SecurityFlags, see #SecurityFlags
 */
SecurityFlags mutt_parse_crypt_hdr(const char *p, bool set_empty_signas, SecurityFlags crypt_app)
{
  char smime_cryptalg[1024] = { 0 };
  char sign_as[1024] = { 0 };
  char *q = NULL;
  SecurityFlags flags = SEC_NO_FLAGS;

  if (!WithCrypto)
    return SEC_NO_FLAGS;

  p = mutt_str_skip_email_wsp(p);
  for (; p[0] != '\0'; p++)
  {
    switch (p[0])
    {
      case 'c':
      case 'C':
        q = smime_cryptalg;

        if (p[1] == '<')
        {
          for (p += 2; (p[0] != '\0') && (p[0] != '>') &&
                       (q < (smime_cryptalg + sizeof(smime_cryptalg) - 1));
               *q++ = *p++)
          {
          }

          if (p[0] != '>')
          {
            mutt_error(_("Illegal S/MIME header"));
            return SEC_NO_FLAGS;
          }
        }

        *q = '\0';
        break;

      case 'e':
      case 'E':
        flags |= SEC_ENCRYPT;
        break;

      case 'i':
      case 'I':
        flags |= SEC_INLINE;
        break;

      /* This used to be the micalg parameter.
       *
       * It's no longer needed, so we just skip the parameter in order
       * to be able to recall old messages.  */
      case 'm':
      case 'M':
        if (p[1] != '<')
          break;

        for (p += 2; (p[0] != '\0') && (p[0] != '>'); p++)
          ; // do nothing

        if (p[0] != '>')
        {
          mutt_error(_("Illegal crypto header"));
          return SEC_NO_FLAGS;
        }
        break;

      case 'o':
      case 'O':
        flags |= SEC_OPPENCRYPT;
        break;

      case 'a':
      case 'A':
#ifdef USE_AUTOCRYPT
        flags |= SEC_AUTOCRYPT;
#endif
        break;

      case 'z':
      case 'Z':
#ifdef USE_AUTOCRYPT
        flags |= SEC_AUTOCRYPT_OVERRIDE;
#endif
        break;

      case 's':
      case 'S':
        flags |= SEC_SIGN;
        q = sign_as;

        if (p[1] == '<')
        {
          for (p += 2;
               (p[0] != '\0') && (*p != '>') && (q < (sign_as + sizeof(sign_as) - 1));
               *q++ = *p++)
          {
          }

          if (p[0] != '>')
          {
            mutt_error(_("Illegal crypto header"));
            return SEC_NO_FLAGS;
          }
        }

        q[0] = '\0';
        break;

      default:
        mutt_error(_("Illegal crypto header"));
        return SEC_NO_FLAGS;
    }
  }

  /* the cryptalg field must not be empty */
  if (((WithCrypto & APPLICATION_SMIME) != 0) && *smime_cryptalg)
  {
    struct Buffer errmsg = mutt_buffer_make(0);
    int rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                      smime_cryptalg, &errmsg);

    if ((CSR_RESULT(rc) != CSR_SUCCESS) && !mutt_buffer_is_empty(&errmsg))
      mutt_error("%s", mutt_buffer_string(&errmsg));

    mutt_buffer_dealloc(&errmsg);
  }

  /* Set {Smime,Pgp}SignAs, if desired. */

  if (((WithCrypto & APPLICATION_PGP) != 0) && (crypt_app == APPLICATION_PGP) &&
      (flags & SEC_SIGN) && (set_empty_signas || *sign_as))
  {
    cs_subset_str_string_set(NeoMutt->sub, "pgp_sign_as", sign_as, NULL);
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (crypt_app == APPLICATION_SMIME) &&
      (flags & SEC_SIGN) && (set_empty_signas || *sign_as))
  {
    cs_subset_str_string_set(NeoMutt->sub, "smime_sign_as", sign_as, NULL);
  }

  return flags;
}

/**
 * mutt_prepare_template - Prepare a message template
 * @param fp      If not NULL, file containing the template
 * @param m       If fp is NULL, the Mailbox containing the header with the template
 * @param e_new   The template is read into this Header
 * @param e       Email to recall/resend
 * @param resend  Set if resending (as opposed to recalling a postponed msg)
 *                Resent messages enable header weeding, and also
 *                discard any existing Message-ID and Mail-Followup-To
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_prepare_template(FILE *fp, struct Mailbox *m, struct Email *e_new,
                          struct Email *e, bool resend)
{
  struct Message *msg = NULL;
  struct Body *b = NULL;
  FILE *fp_body = NULL;
  int rc = -1;
  struct State s = { 0 };
  SecurityFlags sec_type;
  struct Envelope *protected_headers = NULL;
  struct Buffer *file = NULL;

  if (!fp && !(msg = mx_msg_open(m, e->msgno)))
    return -1;

  if (!fp)
    fp = msg->fp;

  fp_body = fp;

  /* parse the message header and MIME structure */

  fseeko(fp, e->offset, SEEK_SET);
  e_new->offset = e->offset;
  /* enable header weeding for resent messages */
  e_new->env = mutt_rfc822_read_header(fp, e_new, true, resend);
  e_new->body->length = e->body->length;
  mutt_parse_part(fp, e_new->body);

  /* If resending a message, don't keep message_id or mail_followup_to.
   * Otherwise, we are resuming a postponed message, and want to keep those
   * headers if they exist.  */
  if (resend)
  {
    FREE(&e_new->env->message_id);
    FREE(&e_new->env->mail_followup_to);
  }

  /* decrypt pgp/mime encoded messages */

  if (((WithCrypto & APPLICATION_PGP) != 0) &&
      (sec_type = mutt_is_multipart_encrypted(e_new->body)))
  {
    e_new->security |= sec_type;
    if (!crypt_valid_passphrase(sec_type))
      goto bail;

    mutt_message(_("Decrypting message..."));
    if ((crypt_pgp_decrypt_mime(fp, &fp_body, e_new->body, &b) == -1) || !b)
    {
      mutt_error(_("Could not decrypt PGP message"));
      goto bail;
    }

    mutt_body_free(&e_new->body);
    e_new->body = b;

    if (b->mime_headers)
    {
      protected_headers = b->mime_headers;
      b->mime_headers = NULL;
    }

    mutt_clear_error();
  }

  /* remove a potential multipart/signed layer - useful when
   * resending messages */
  if ((WithCrypto != 0) && mutt_is_multipart_signed(e_new->body))
  {
    e_new->security |= SEC_SIGN;
    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        mutt_istr_equal(mutt_param_get(&e_new->body->parameter, "protocol"),
                        "application/pgp-signature"))
    {
      e_new->security |= APPLICATION_PGP;
    }
    else if (WithCrypto & APPLICATION_SMIME)
      e_new->security |= APPLICATION_SMIME;

    /* destroy the signature */
    mutt_body_free(&e_new->body->parts->next);
    e_new->body = mutt_remove_multipart(e_new->body);

    if (e_new->body->mime_headers)
    {
      mutt_env_free(&protected_headers);
      protected_headers = e_new->body->mime_headers;
      e_new->body->mime_headers = NULL;
    }
  }

  /* We don't need no primary multipart.
   * Note: We _do_ preserve messages!
   *
   * XXX - we don't handle multipart/alternative in any
   * smart way when sending messages.  However, one may
   * consider this a feature.  */
  if (e_new->body->type == TYPE_MULTIPART)
    e_new->body = mutt_remove_multipart(e_new->body);

  s.fp_in = fp_body;

  file = mutt_buffer_pool_get();

  /* create temporary files for all attachments */
  for (b = e_new->body; b; b = b->next)
  {
    /* what follows is roughly a receive-mode variant of
     * mutt_get_tmp_attachment () from muttlib.c */

    mutt_buffer_reset(file);
    if (b->filename)
    {
      mutt_buffer_strcpy(file, b->filename);
      b->d_filename = mutt_str_dup(b->filename);
    }
    else
    {
      /* avoid Content-Disposition: header with temporary filename */
      b->use_disp = false;
    }

    /* set up state flags */

    s.flags = 0;

    if (b->type == TYPE_TEXT)
    {
      if (mutt_istr_equal("yes",
                          mutt_param_get(&b->parameter, "x-mutt-noconv")))
      {
        b->noconv = true;
      }
      else
      {
        s.flags |= MUTT_CHARCONV;
        b->noconv = false;
      }

      mutt_param_delete(&b->parameter, "x-mutt-noconv");
    }

    mutt_adv_mktemp(file);
    s.fp_out = mutt_file_fopen(mutt_buffer_string(file), "w");
    if (!s.fp_out)
      goto bail;

    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        ((sec_type = mutt_is_application_pgp(b)) & (SEC_ENCRYPT | SEC_SIGN)))
    {
      if (sec_type & SEC_ENCRYPT)
      {
        if (!crypt_valid_passphrase(APPLICATION_PGP))
          goto bail;
        mutt_message(_("Decrypting message..."));
      }

      if (mutt_body_handler(b, &s) < 0)
      {
        mutt_error(_("Decryption failed"));
        goto bail;
      }

      if ((b == e_new->body) && !protected_headers)
      {
        protected_headers = b->mime_headers;
        b->mime_headers = NULL;
      }

      e_new->security |= sec_type;
      b->type = TYPE_TEXT;
      mutt_str_replace(&b->subtype, "plain");
      mutt_param_delete(&b->parameter, "x-action");
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             ((sec_type = mutt_is_application_smime(b)) & (SEC_ENCRYPT | SEC_SIGN)))
    {
      if (sec_type & SEC_ENCRYPT)
      {
        if (!crypt_valid_passphrase(APPLICATION_SMIME))
          goto bail;
        crypt_smime_getkeys(e_new->env);
        mutt_message(_("Decrypting message..."));
      }

      if (mutt_body_handler(b, &s) < 0)
      {
        mutt_error(_("Decryption failed"));
        goto bail;
      }

      e_new->security |= sec_type;
      b->type = TYPE_TEXT;
      mutt_str_replace(&b->subtype, "plain");
    }
    else
      mutt_decode_attachment(b, &s);

    if (mutt_file_fclose(&s.fp_out) != 0)
      goto bail;

    mutt_str_replace(&b->filename, mutt_buffer_string(file));
    b->unlink = true;

    mutt_stamp_attachment(b);

    mutt_body_free(&b->parts);
    if (b->email)
      b->email->body = NULL; /* avoid dangling pointer */
  }

  const bool c_crypt_protected_headers_read =
      cs_subset_bool(NeoMutt->sub, "crypt_protected_headers_read");
  if (c_crypt_protected_headers_read && protected_headers && protected_headers->subject &&
      !mutt_str_equal(e_new->env->subject, protected_headers->subject))
  {
    mutt_str_replace(&e_new->env->subject, protected_headers->subject);
  }
  mutt_env_free(&protected_headers);

  /* Fix encryption flags. */

  /* No inline if multipart. */
  if ((WithCrypto != 0) && (e_new->security & SEC_INLINE) && e_new->body->next)
    e_new->security &= ~SEC_INLINE;

  /* Do we even support multiple mechanisms? */
  e_new->security &= WithCrypto | ~(APPLICATION_PGP | APPLICATION_SMIME);

  /* Theoretically, both could be set. Take the one the user wants to set by default. */
  if ((e_new->security & APPLICATION_PGP) && (e_new->security & APPLICATION_SMIME))
  {
    const bool c_smime_is_default =
        cs_subset_bool(NeoMutt->sub, "smime_is_default");
    if (c_smime_is_default)
      e_new->security &= ~APPLICATION_PGP;
    else
      e_new->security &= ~APPLICATION_SMIME;
  }

  mutt_rfc3676_space_unstuff(e_new);

  rc = 0;

bail:

  /* that's it. */
  mutt_buffer_pool_release(&file);
  if (fp_body != fp)
    mutt_file_fclose(&fp_body);
  if (msg)
    mx_msg_close(m, &msg);

  if (rc == -1)
  {
    mutt_env_free(&e_new->env);
    mutt_body_free(&e_new->body);
  }

  return rc;
}
