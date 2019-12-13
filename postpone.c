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
 * @page postpone Save/restore and GUI list postponed emails
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
#include "context.h"
#include "format_flags.h"
#include "handler.h"
#include "hdrline.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "rfc3676.h"
#include "sort.h"
#include "state.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

static const struct Mapping PostponeHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
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

  if (!mutt_str_equal(C_Postponed, OldPostponed))
  {
    FREE(&OldPostponed);
    OldPostponed = mutt_str_dup(C_Postponed);
    LastModify = 0;
    force = true;
  }

  if (!C_Postponed)
    return 0;

  // We currently are in the C_Postponed mailbox so just pick the current status
  if (m && mutt_str_equal(C_Postponed, m->realpath))
  {
    PostCount = m->msg_count - m->msg_deleted;
    return PostCount;
  }

#ifdef USE_IMAP
  /* LastModify is useless for IMAP */
  if (imap_path_probe(C_Postponed, NULL) == MUTT_IMAP)
  {
    if (force)
    {
      short newpc;

      newpc = imap_path_status(C_Postponed, false);
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

  if (stat(C_Postponed, &st) == -1)
  {
    PostCount = 0;
    LastModify = 0;
    return 0;
  }

  if (S_ISDIR(st.st_mode))
  {
    /* if we have a maildir mailbox, we need to stat the "new" dir */
    struct Buffer *buf = mutt_buffer_pool_get();

    mutt_buffer_printf(buf, "%s/new", C_Postponed);
    if ((access(mutt_b2s(buf), F_OK) == 0) && (stat(mutt_b2s(buf), &st) == -1))
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

    if (access(C_Postponed, R_OK | F_OK) != 0)
      return PostCount = 0;
#ifdef USE_NNTP
    if (optnews)
      OptNews = false;
#endif
    struct Mailbox *m_post = mx_path_resolve(C_Postponed);
    struct Context *ctx = mx_mbox_open(m_post, MUTT_NOSORT | MUTT_QUIET);
    if (ctx)
    {
      PostCount = ctx->mailbox->msg_count;
    }
    else
    {
      mailbox_free(&m_post);
      PostCount = 0;
    }
    mx_fastclose_mailbox(m_post);
    ctx_free(&ctx);
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
static void post_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct Context *ctx = menu->mdata;

  mutt_make_string_flags(buf, buflen, menu->win_index->state.cols,
                         NONULL(C_IndexFormat), ctx, ctx->mailbox,
                         ctx->mailbox->emails[line], MUTT_FORMAT_ARROWCURSOR);
}

/**
 * select_msg - Create a Menu to select a postponed message
 * @param ctx Context
 * @retval ptr Email
 */
static struct Email *select_msg(struct Context *ctx)
{
  int r = -1;
  bool done = false;
  char helpstr[1024];

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_POSTPONE, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  dialog_push(dlg);

  struct Menu *menu = mutt_menu_new(MENU_POSTPONE);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->make_entry = post_make_entry;
  menu->max = ctx->mailbox->msg_count;
  menu->title = _("Postponed Messages");
  menu->mdata = ctx;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_POSTPONE, PostponeHelp);
  mutt_menu_push_current(menu);

  /* The postponed mailbox is setup to have sorting disabled, but the global
   * C_Sort variable may indicate something different.   Sorting has to be
   * disabled while the postpone menu is being displayed. */
  const short orig_sort = C_Sort;
  C_Sort = SORT_ORDER;

  while (!done)
  {
    const int op = mutt_menu_loop(menu);
    switch (op)
    {
      case OP_DELETE:
      case OP_UNDELETE:
        /* should deleted draft messages be saved in the trash folder? */
        mutt_set_flag(ctx->mailbox, ctx->mailbox->emails[menu->current],
                      MUTT_DELETE, (op == OP_DELETE));
        PostCount = ctx->mailbox->msg_count - ctx->mailbox->msg_deleted;
        if (C_Resolve && (menu->current < menu->max - 1))
        {
          menu->oldcurrent = menu->current;
          menu->current++;
          if (menu->current >= menu->top + menu->pagelen)
          {
            menu->top = menu->current;
            menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
          }
          else
            menu->redraw |= REDRAW_MOTION_RESYNC;
        }
        else
          menu->redraw |= REDRAW_CURRENT;
        break;

      case OP_GENERIC_SELECT_ENTRY:
        r = menu->current;
        done = true;
        break;

      case OP_EXIT:
        done = true;
        break;
    }
  }

  C_Sort = orig_sort;
  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_pop();
  mutt_window_free(&dlg);

  return (r > -1) ? ctx->mailbox->emails[r] : NULL;
}

/**
 * mutt_get_postponed - Recall a postponed message
 * @param[in]  ctx     Context info, used when recalling a message to which we reply
 * @param[in]  hdr     envelope/attachment info for recalled message
 * @param[out] cur     if message was a reply, 'cur' is set to the message which 'hdr' is in reply to
 * @param[in]  fcc     fcc for the recalled message
 * @retval -1         Error/no messages
 * @retval 0          Normal exit
 * @retval #SEND_REPLY Recalled message is a reply
 */
int mutt_get_postponed(struct Context *ctx, struct Email *hdr,
                       struct Email **cur, struct Buffer *fcc)
{
  if (!C_Postponed)
    return -1;

  struct Email *e = NULL;
  int rc = SEND_POSTPONED;
  int rc_close;
  const char *p = NULL;
  struct Context *ctx_post = NULL;

  struct Mailbox *m = mx_path_resolve(C_Postponed);
  if (ctx && (ctx->mailbox == m))
    ctx_post = ctx;
  else
    ctx_post = mx_mbox_open(m, MUTT_NOSORT);

  if (!ctx_post)
  {
    PostCount = 0;
    mutt_error(_("No postponed messages"));
    mailbox_free(&m);
    return -1;
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
  mx_mbox_check(ctx_post->mailbox);

  if (ctx_post->mailbox->msg_count == 0)
  {
    PostCount = 0;
    if (ctx_post == ctx)
      ctx_post = NULL;
    else
      mx_fastclose_mailbox(ctx_post->mailbox);
    mutt_error(_("No postponed messages"));
    return -1;
  }

  if (ctx_post->mailbox->msg_count == 1)
  {
    /* only one message, so just use that one. */
    e = ctx_post->mailbox->emails[0];
  }
  else if (!(e = select_msg(ctx_post)))
  {
    if (ctx_post == ctx)
      ctx_post = NULL;
    else
    {
      /* messages might have been marked for deletion.
       * try once more on reopen before giving up. */
      rc_close = mx_mbox_close(&ctx_post);
      if (rc_close > 0)
        rc_close = mx_mbox_close(&ctx_post);
      if (rc_close != 0)
        mx_fastclose_mailbox(ctx_post->mailbox);
    }
    return -1;
  }

  if (mutt_prepare_template(NULL, ctx_post->mailbox, hdr, e, false) < 0)
  {
    if (ctx_post != ctx)
    {
      mx_fastclose_mailbox(ctx_post->mailbox);
      FREE(&ctx_post);
    }
    return -1;
  }

  /* finished with this message, so delete it. */
  mutt_set_flag(ctx_post->mailbox, e, MUTT_DELETE, true);
  mutt_set_flag(ctx_post->mailbox, e, MUTT_PURGE, true);

  /* update the count for the status display */
  PostCount = ctx_post->mailbox->msg_count - ctx_post->mailbox->msg_deleted;

  /* avoid the "purge deleted messages" prompt */
  int opt_delete = C_Delete;
  C_Delete = MUTT_YES;
  if (ctx_post == ctx)
    ctx_post = NULL;
  else
  {
    rc_close = mx_mbox_close(&ctx_post);
    if (rc_close > 0)
      rc_close = mx_mbox_close(&ctx_post);
    if (rc_close != 0)
      mx_fastclose_mailbox(ctx_post->mailbox);
  }
  C_Delete = opt_delete;

  struct ListNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &hdr->env->userhdrs, entries, tmp)
  {
    size_t plen = mutt_istr_startswith(np->data, "X-Mutt-References:");
    if (plen)
    {
      /* if a mailbox is currently open, look to see if the original message
       * the user attempted to reply to is in this mailbox */
      p = mutt_str_skip_email_wsp(np->data + plen);
      if (!ctx->mailbox->id_hash)
        ctx->mailbox->id_hash = mutt_make_id_hash(ctx->mailbox);
      *cur = mutt_hash_find(ctx->mailbox->id_hash, p);

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

  if (C_CryptOpportunisticEncrypt)
    crypt_opportunistic_encrypt(hdr);

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
      mutt_error("%s", mutt_b2s(&errmsg));

    mutt_buffer_dealloc(&errmsg);
  }

  /* Set {Smime,Pgp}SignAs, if desired. */

  if (((WithCrypto & APPLICATION_PGP) != 0) && (crypt_app == APPLICATION_PGP) &&
      (flags & SEC_SIGN) && (set_empty_signas || *sign_as))
  {
    mutt_str_replace(&C_PgpSignAs, sign_as);
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (crypt_app == APPLICATION_SMIME) &&
      (flags & SEC_SIGN) && (set_empty_signas || *sign_as))
  {
    mutt_str_replace(&C_SmimeSignAs, sign_as);
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
  e_new->content->length = e->content->length;
  mutt_parse_part(fp, e_new->content);

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
      (sec_type = mutt_is_multipart_encrypted(e_new->content)))
  {
    e_new->security |= sec_type;
    if (!crypt_valid_passphrase(sec_type))
      goto bail;

    mutt_message(_("Decrypting message..."));
    if ((crypt_pgp_decrypt_mime(fp, &fp_body, e_new->content, &b) == -1) || !b)
    {
      goto bail;
    }

    mutt_body_free(&e_new->content);
    e_new->content = b;

    if (b->mime_headers)
    {
      protected_headers = b->mime_headers;
      b->mime_headers = NULL;
    }

    mutt_clear_error();
  }

  /* remove a potential multipart/signed layer - useful when
   * resending messages */
  if ((WithCrypto != 0) && mutt_is_multipart_signed(e_new->content))
  {
    e_new->security |= SEC_SIGN;
    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        mutt_istr_equal(mutt_param_get(&e_new->content->parameter, "protocol"),
                        "application/pgp-signature"))
    {
      e_new->security |= APPLICATION_PGP;
    }
    else if (WithCrypto & APPLICATION_SMIME)
      e_new->security |= APPLICATION_SMIME;

    /* destroy the signature */
    mutt_body_free(&e_new->content->parts->next);
    e_new->content = mutt_remove_multipart(e_new->content);

    if (e_new->content->mime_headers)
    {
      mutt_env_free(&protected_headers);
      protected_headers = e_new->content->mime_headers;
      e_new->content->mime_headers = NULL;
    }
  }

  /* We don't need no primary multipart.
   * Note: We _do_ preserve messages!
   *
   * XXX - we don't handle multipart/alternative in any
   * smart way when sending messages.  However, one may
   * consider this a feature.  */
  if (e_new->content->type == TYPE_MULTIPART)
    e_new->content = mutt_remove_multipart(e_new->content);

  s.fp_in = fp_body;

  struct Buffer *file = mutt_buffer_pool_get();

  /* create temporary files for all attachments */
  for (b = e_new->content; b; b = b->next)
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
    s.fp_out = mutt_file_fopen(mutt_b2s(file), "w");
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

      if ((b == e_new->content) && !protected_headers)
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

    mutt_str_replace(&b->filename, mutt_b2s(file));
    b->unlink = true;

    mutt_stamp_attachment(b);

    mutt_body_free(&b->parts);
    if (b->email)
      b->email->content = NULL; /* avoid dangling pointer */
  }

  if (C_CryptProtectedHeadersRead && protected_headers && protected_headers->subject &&
      !mutt_str_equal(e_new->env->subject, protected_headers->subject))
  {
    mutt_str_replace(&e_new->env->subject, protected_headers->subject);
  }
  mutt_env_free(&protected_headers);

  /* Fix encryption flags. */

  /* No inline if multipart. */
  if ((WithCrypto != 0) && (e_new->security & SEC_INLINE) && e_new->content->next)
    e_new->security &= ~SEC_INLINE;

  /* Do we even support multiple mechanisms? */
  e_new->security &= WithCrypto | ~(APPLICATION_PGP | APPLICATION_SMIME);

  /* Theoretically, both could be set. Take the one the user wants to set by default. */
  if ((e_new->security & APPLICATION_PGP) && (e_new->security & APPLICATION_SMIME))
  {
    if (C_SmimeIsDefault)
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
    mutt_body_free(&e_new->content);
  }

  return rc;
}
