/**
 * @file
 * Prepare and send an email
 *
 * @authors
 * Copyright (C) 1996-2002,2004,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page send_send Prepare and send an email
 *
 * Prepare and send an email
 */

#include "config.h"
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "send.h"
#include "lib.h"
#include "compose.h"
#include "context.h"
#include "copy.h"
#include "handler.h"
#include "hdrline.h"
#include "hook.h"
#include "maillist.h"
#include "mutt_attach.h"
#include "mutt_body.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "recvattach.h"
#include "rfc3676.h"
#include "sort.h"
#include "ncrypt/lib.h"
#ifdef USE_NNTP
#include "mx.h"
#include "nntp/lib.h"
#endif
#ifdef MIXMASTER
#include "remailer.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/**
 * append_signature - Append a signature to an email
 * @param fp File to write to
 */
static void append_signature(FILE *fp)
{
  FILE *fp_tmp = NULL;
  pid_t pid;

  if (C_Signature && (fp_tmp = mutt_open_read(C_Signature, &pid)))
  {
    if (C_SigDashes)
      fputs("\n-- \n", fp);
    mutt_file_copy_stream(fp_tmp, fp);
    mutt_file_fclose(&fp_tmp);
    if (pid != -1)
      filter_wait(pid);
  }
}

/**
 * remove_user - Remove any address which matches the current user
 * @param al         List of addresses
 * @param leave_only If set, don't remove the user's address if it it the only
 *                   one in the list
 */
static void remove_user(struct AddressList *al, bool leave_only)
{
  struct Address *a = NULL, *tmp = NULL;
  TAILQ_FOREACH_SAFE(a, al, entries, tmp)
  {
    if (mutt_addr_is_user(a) && (!leave_only || TAILQ_NEXT(a, entries)))
    {
      TAILQ_REMOVE(al, a, entries);
      mutt_addr_free(&a);
    }
  }
}

/**
 * add_mailing_lists - Search Address lists for mailing lists
 * @param out Address list where to append matching mailing lists
 * @param t   'To' Address list
 * @param c   'Cc' Address list
 */
static void add_mailing_lists(struct AddressList *out, const struct AddressList *t,
                              const struct AddressList *c)
{
  const struct AddressList *const als[] = { t, c };

  for (size_t i = 0; i < mutt_array_size(als); ++i)
  {
    const struct AddressList *al = als[i];
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      if (!a->group && mutt_is_mail_list(a))
      {
        mutt_addrlist_append(out, mutt_addr_copy(a));
      }
    }
  }
}

/**
 * mutt_edit_address - Edit an email address
 * @param[in,out] al          AddressList to edit
 * @param[in]  field          Prompt for user
 * @param[in]  expand_aliases If true, expand Address aliases
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_edit_address(struct AddressList *al, const char *field, bool expand_aliases)
{
  char buf[8192];
  char *err = NULL;
  int idna_ok = 0;

  do
  {
    buf[0] = '\0';
    mutt_addrlist_to_local(al);
    mutt_addrlist_write(al, buf, sizeof(buf), false);
    if (mutt_get_field(field, buf, sizeof(buf), MUTT_ALIAS) != 0)
      return -1;
    mutt_addrlist_clear(al);
    mutt_addrlist_parse2(al, buf);
    if (expand_aliases)
      mutt_expand_aliases(al);
    idna_ok = mutt_addrlist_to_intl(al, &err);
    if (idna_ok != 0)
    {
      mutt_error(_("Bad IDN: '%s'"), err);
      FREE(&err);
    }
  } while (idna_ok != 0);
  return 0;
}

/**
 * edit_envelope - Edit Envelope fields
 * @param en    Envelope to edit
 * @param flags Flags, see #SendFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int edit_envelope(struct Envelope *en, SendFlags flags)
{
  char buf[8192];

#ifdef USE_NNTP
  if (OptNewsSend)
  {
    if (en->newsgroups)
      mutt_str_copy(buf, en->newsgroups, sizeof(buf));
    else
      buf[0] = '\0';
    if (mutt_get_field("Newsgroups: ", buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0)
      return -1;
    FREE(&en->newsgroups);
    en->newsgroups = mutt_str_dup(buf);

    if (en->followup_to)
      mutt_str_copy(buf, en->followup_to, sizeof(buf));
    else
      buf[0] = '\0';
    if (C_AskFollowUp &&
        (mutt_get_field("Followup-To: ", buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0))
    {
      return -1;
    }
    FREE(&en->followup_to);
    en->followup_to = mutt_str_dup(buf);

    if (en->x_comment_to)
      mutt_str_copy(buf, en->x_comment_to, sizeof(buf));
    else
      buf[0] = '\0';
    if (C_XCommentTo && C_AskXCommentTo &&
        (mutt_get_field("X-Comment-To: ", buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0))
    {
      return -1;
    }
    FREE(&en->x_comment_to);
    en->x_comment_to = mutt_str_dup(buf);
  }
  else
#endif
  {
    if ((mutt_edit_address(&en->to, _("To: "), true) == -1) || TAILQ_EMPTY(&en->to))
      return -1;
    if (C_Askcc && (mutt_edit_address(&en->cc, _("Cc: "), true) == -1))
      return -1;
    if (C_Askbcc && (mutt_edit_address(&en->bcc, _("Bcc: "), true) == -1))
      return -1;
    if (C_ReplyWithXorig && (flags & (SEND_REPLY | SEND_LIST_REPLY | SEND_GROUP_REPLY)) &&
        (mutt_edit_address(&en->from, "From: ", true) == -1))
    {
      return -1;
    }
  }

  if (en->subject)
  {
    if (C_FastReply)
      return 0;
    mutt_str_copy(buf, en->subject, sizeof(buf));
  }
  else
  {
    const char *p = NULL;

    buf[0] = '\0';
    struct ListNode *uh = NULL;
    STAILQ_FOREACH(uh, &UserHeader, entries)
    {
      size_t plen = mutt_istr_startswith(uh->data, "subject:");
      if (plen)
      {
        p = mutt_str_skip_email_wsp(uh->data + plen);
        mutt_str_copy(buf, p, sizeof(buf));
      }
    }
  }

  if ((mutt_get_field(_("Subject: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0) ||
      ((buf[0] == '\0') &&
       (query_quadoption(C_AbortNosubject, _("No subject, abort?")) != MUTT_NO)))
  {
    mutt_message(_("No subject, aborting"));
    return -1;
  }
  mutt_str_replace(&en->subject, buf);

  return 0;
}

#ifdef USE_NNTP
/**
 * nntp_get_header - Get the trimmed header
 * @param s Header line with leading whitespace
 * @retval ptr Copy of string
 *
 * @note The caller should free the returned string.
 */
static char *nntp_get_header(const char *s)
{
  SKIPWS(s);
  return mutt_str_dup(s);
}
#endif

/**
 * process_user_recips - Process the user headers
 * @param env Envelope to populate
 */
static void process_user_recips(struct Envelope *env)
{
  struct ListNode *uh = NULL;
  STAILQ_FOREACH(uh, &UserHeader, entries)
  {
    size_t plen;
    if ((plen = mutt_istr_startswith(uh->data, "to:")))
      mutt_addrlist_parse(&env->to, uh->data + plen);
    else if ((plen = mutt_istr_startswith(uh->data, "cc:")))
      mutt_addrlist_parse(&env->cc, uh->data + plen);
    else if ((plen = mutt_istr_startswith(uh->data, "bcc:")))
      mutt_addrlist_parse(&env->bcc, uh->data + plen);
#ifdef USE_NNTP
    else if ((plen = mutt_istr_startswith(uh->data, "newsgroups:")))
      env->newsgroups = nntp_get_header(uh->data + plen);
    else if ((plen = mutt_istr_startswith(uh->data, "followup-to:")))
      env->followup_to = nntp_get_header(uh->data + plen);
    else if ((plen = mutt_istr_startswith(uh->data, "x-comment-to:")))
      env->x_comment_to = nntp_get_header(uh->data + plen);
#endif
  }
}

/**
 * process_user_header - Process the user headers
 * @param env Envelope to populate
 */
static void process_user_header(struct Envelope *env)
{
  struct ListNode *uh = NULL;
  STAILQ_FOREACH(uh, &UserHeader, entries)
  {
    size_t plen;
    if ((plen = mutt_istr_startswith(uh->data, "from:")))
    {
      /* User has specified a default From: address.  Remove default address */
      mutt_addrlist_clear(&env->from);
      mutt_addrlist_parse(&env->from, uh->data + plen);
    }
    else if ((plen = mutt_istr_startswith(uh->data, "reply-to:")))
    {
      mutt_addrlist_clear(&env->reply_to);
      mutt_addrlist_parse(&env->reply_to, uh->data + plen);
    }
    else if ((plen = mutt_istr_startswith(uh->data, "message-id:")))
    {
      char *tmp = mutt_extract_message_id(uh->data + plen, NULL);
      if (mutt_addr_valid_msgid(tmp))
      {
        FREE(&env->message_id);
        env->message_id = tmp;
      }
      else
        FREE(&tmp);
    }
    else if (!mutt_istr_startswith(uh->data, "to:") &&
             !mutt_istr_startswith(uh->data, "cc:") &&
             !mutt_istr_startswith(uh->data, "bcc:") &&
#ifdef USE_NNTP
             !mutt_istr_startswith(uh->data, "newsgroups:") &&
             !mutt_istr_startswith(uh->data, "followup-to:") &&
             !mutt_istr_startswith(uh->data, "x-comment-to:") &&
#endif
             !mutt_istr_startswith(uh->data, "supersedes:") &&
             !mutt_istr_startswith(uh->data, "subject:") &&
             !mutt_istr_startswith(uh->data, "return-path:"))
    {
      mutt_list_insert_tail(&env->userhdrs, mutt_str_dup(uh->data));
    }
  }
}

/**
 * mutt_forward_intro - Add the "start of forwarded message" text
 * @param m   Mailbox
 * @param e Email
 * @param fp  File to write to
 */
void mutt_forward_intro(struct Mailbox *m, struct Email *e, FILE *fp)
{
  if (!C_ForwardAttributionIntro || !fp)
    return;

  char buf[1024];
  setlocale(LC_TIME, NONULL(C_AttributionLocale));
  mutt_make_string(buf, sizeof(buf), 0, C_ForwardAttributionIntro, NULL, m, e);
  setlocale(LC_TIME, "");
  fputs(buf, fp);
  fputs("\n\n", fp);
}

/**
 * mutt_forward_trailer - Add a "end of forwarded message" text
 * @param m   Mailbox
 * @param e Email
 * @param fp  File to write to
 */
void mutt_forward_trailer(struct Mailbox *m, struct Email *e, FILE *fp)
{
  if (!C_ForwardAttributionTrailer || !fp)
    return;

  char buf[1024];
  setlocale(LC_TIME, NONULL(C_AttributionLocale));
  mutt_make_string(buf, sizeof(buf), 0, C_ForwardAttributionTrailer, NULL, m, e);
  setlocale(LC_TIME, "");
  fputc('\n', fp);
  fputs(buf, fp);
  fputc('\n', fp);
}

/**
 * include_forward - Write out a forwarded message
 * @param m      Mailbox
 * @param e      Email
 * @param fp_out File to write to
 * @retval  0 Success
 * @retval -1 Failure
 */
static int include_forward(struct Mailbox *m, struct Email *e, FILE *fp_out)
{
  CopyHeaderFlags chflags = CH_DECODE;
  CopyMessageFlags cmflags = MUTT_CM_NO_FLAGS;

  mutt_parse_mime_message(m, e);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT) && C_ForwardDecode)
  {
    /* make sure we have the user's passphrase before proceeding... */
    if (!crypt_valid_passphrase(e->security))
      return -1;
  }

  mutt_forward_intro(m, e, fp_out);

  if (C_ForwardDecode)
  {
    cmflags |= MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    if (C_Weed)
    {
      chflags |= CH_WEED | CH_REORDER;
      cmflags |= MUTT_CM_WEED;
    }
  }
  if (C_ForwardQuote)
    cmflags |= MUTT_CM_PREFIX;

  mutt_copy_message(fp_out, m, e, cmflags, chflags, 0);
  mutt_forward_trailer(m, e, fp_out);
  return 0;
}

/**
 * inline_forward_attachments - Add attachments to an email, inline
 * @param[in]  m        Mailbox
 * @param[in]  e        Current Email
 * @param[out] plast    Pointer to the last Attachment
 * @param[out] forwardq Result of asking the user to forward the attachments, e.g. #MUTT_YES
 * @retval  0 Success
 * @retval -1 Error
 */
static int inline_forward_attachments(struct Mailbox *m, struct Email *e,
                                      struct Body ***plast, enum QuadOption *forwardq)
{
  struct Body **last = *plast;
  struct Body *body = NULL;
  struct Message *msg = NULL;
  struct AttachCtx *actx = NULL;
  int rc = 0, i;

  mutt_parse_mime_message(m, e);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  msg = mx_msg_open(m, e->msgno);
  if (!msg)
    return -1;

  actx = mutt_mem_calloc(1, sizeof(*actx));
  actx->email = e;
  actx->fp_root = msg->fp;

  mutt_generate_recvattach_list(actx, actx->email, actx->email->content,
                                actx->fp_root, -1, 0, 0);

  for (i = 0; i < actx->idxlen; i++)
  {
    body = actx->idx[i]->content;
    if ((body->type != TYPE_MULTIPART) && !mutt_can_decode(body) &&
        !((body->type == TYPE_APPLICATION) &&
          (mutt_istr_equal(body->subtype, "pgp-signature") ||
           mutt_istr_equal(body->subtype, "x-pkcs7-signature") ||
           mutt_istr_equal(body->subtype, "pkcs7-signature"))))
    {
      /* Ask the quadoption only once */
      if (*forwardq == MUTT_ABORT)
      {
        *forwardq = query_quadoption(C_ForwardAttachments,
                                     /* L10N: This is the prompt for $forward_attachments.
                                        When inline forwarding ($mime_forward answered "no"), this prompts
                                        whether to add non-decodable attachments from the original email.
                                        Text/plain parts and the like will already be included in the
                                        message contents, but other attachment, such as PDF files, will also
                                        be added as attachments to the new mail, if this is answered yes.  */
                                     _("Forward attachments?"));
        if (*forwardq != MUTT_YES)
        {
          if (*forwardq == -1)
            rc = -1;
          goto cleanup;
        }
      }
      if (mutt_body_copy(actx->idx[i]->fp, last, body) == -1)
      {
        rc = -1;
        goto cleanup;
      }
      last = &((*last)->next);
    }
  }

cleanup:
  *plast = last;
  mx_msg_close(m, &msg);
  mutt_actx_free(&actx);
  return rc;
}

/**
 * mutt_make_attribution - Add "on DATE, PERSON wrote" header
 * @param m      Mailbox
 * @param e      Email
 * @param fp_out File to write to
 */
void mutt_make_attribution(struct Mailbox *m, struct Email *e, FILE *fp_out)
{
  if (!C_Attribution || !fp_out)
    return;

  char buf[1024];
  setlocale(LC_TIME, NONULL(C_AttributionLocale));
  mutt_make_string(buf, sizeof(buf), 0, C_Attribution, NULL, m, e);
  setlocale(LC_TIME, "");
  fputs(buf, fp_out);
  fputc('\n', fp_out);
}

/**
 * mutt_make_post_indent - Add suffix to replied email text
 * @param m      Mailbox
 * @param e      Email
 * @param fp_out File to write to
 */
void mutt_make_post_indent(struct Mailbox *m, struct Email *e, FILE *fp_out)
{
  if (!C_PostIndentString || !fp_out)
    return;

  char buf[256];
  mutt_make_string(buf, sizeof(buf), 0, C_PostIndentString, NULL, m, e);
  fputs(buf, fp_out);
  fputc('\n', fp_out);
}

/**
 * include_reply - Generate the reply text for an email
 * @param m      Mailbox
 * @param e      Email
 * @param fp_out File to write to
 * @retval  0 Success
 * @retval -1 Failure
 */
static int include_reply(struct Mailbox *m, struct Email *e, FILE *fp_out)
{
  CopyMessageFlags cmflags =
      MUTT_CM_PREFIX | MUTT_CM_DECODE | MUTT_CM_CHARCONV | MUTT_CM_REPLYING;
  CopyHeaderFlags chflags = CH_DECODE;

  if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT))
  {
    /* make sure we have the user's passphrase before proceeding... */
    if (!crypt_valid_passphrase(e->security))
      return -1;
  }

  mutt_parse_mime_message(m, e);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  mutt_make_attribution(m, e, fp_out);

  if (!C_Header)
    cmflags |= MUTT_CM_NOHEADER;
  if (C_Weed)
  {
    chflags |= CH_WEED | CH_REORDER;
    cmflags |= MUTT_CM_WEED;
  }

  mutt_copy_message(fp_out, m, e, cmflags, chflags, 0);

  mutt_make_post_indent(m, e, fp_out);

  return 0;
}

/**
 * choose_default_to - Pick the best 'to:' value
 * @param from From Address
 * @param env  Envelope
 * @retval ptr Addresses to use
 */
static const struct AddressList *choose_default_to(const struct Address *from,
                                                   const struct Envelope *env)
{
  if (!C_ReplySelf && mutt_addr_is_user(from))
  {
    /* mail is from the user, assume replying to recipients */
    return &env->to;
  }
  else
  {
    return &env->from;
  }
}

/**
 * default_to - Generate default email addresses
 * @param[in,out] to      'To' address
 * @param[in]     env     Envelope to populate
 * @param[in]     flags   Flags, see #SendFlags
 * @param[in]     hmfupto If true, add 'followup-to' address to 'to' address
 * @retval  0 Success
 * @retval -1 Aborted
 */
static int default_to(struct AddressList *to, struct Envelope *env, SendFlags flags, int hmfupto)
{
  const struct Address *from = TAILQ_FIRST(&env->from);
  const struct Address *reply_to = TAILQ_FIRST(&env->reply_to);

  if (flags && !TAILQ_EMPTY(&env->mail_followup_to) && (hmfupto == MUTT_YES))
  {
    mutt_addrlist_copy(to, &env->mail_followup_to, true);
    return 0;
  }

  /* Exit now if we're setting up the default Cc list for list-reply
   * (only set if Mail-Followup-To is present and honoured).  */
  if (flags & SEND_LIST_REPLY)
    return 0;

  const struct AddressList *default_to = choose_default_to(from, env);

  if (reply_to)
  {
    const bool from_is_reply_to = mutt_addr_cmp(from, reply_to);
    const bool multiple_reply_to =
        reply_to && TAILQ_NEXT(TAILQ_FIRST(&env->reply_to), entries);
    if ((from_is_reply_to && !multiple_reply_to && !reply_to->personal) ||
        (C_IgnoreListReplyTo && mutt_is_mail_list(reply_to) &&
         (mutt_addrlist_search(&env->to, reply_to) || mutt_addrlist_search(&env->cc, reply_to))))
    {
      /* If the Reply-To: address is a mailing list, assume that it was
       * put there by the mailing list, and use the From: address
       *
       * We also take the from header if our correspondent has a reply-to
       * header which is identical to the electronic mail address given
       * in his From header, and the reply-to has no display-name.  */
      mutt_addrlist_copy(to, &env->from, false);
    }
    else if (!(from_is_reply_to && !multiple_reply_to) && (C_ReplyTo != MUTT_YES))
    {
      char prompt[256];
      /* There are quite a few mailing lists which set the Reply-To:
       * header field to the list address, which makes it quite impossible
       * to send a message to only the sender of the message.  This
       * provides a way to do that.  */
      /* L10N: Asks whether the user respects the reply-to header.
         If she says no, neomutt will reply to the from header's address instead. */
      snprintf(prompt, sizeof(prompt), _("Reply to %s%s?"), reply_to->mailbox,
               multiple_reply_to ? ",..." : "");
      switch (query_quadoption(C_ReplyTo, prompt))
      {
        case MUTT_YES:
          mutt_addrlist_copy(to, &env->reply_to, false);
          break;

        case MUTT_NO:
          mutt_addrlist_copy(to, default_to, false);
          break;

        default:
          return -1; /* abort */
      }
    }
    else
    {
      mutt_addrlist_copy(to, &env->reply_to, false);
    }
  }
  else
  {
    mutt_addrlist_copy(to, default_to, false);
  }

  return 0;
}

/**
 * mutt_fetch_recips - Generate recpients for a reply email
 * @param out   Envelope to populate
 * @param in    Envelope of source email
 * @param flags Flags, see #SendFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_fetch_recips(struct Envelope *out, struct Envelope *in, SendFlags flags)
{
  enum QuadOption hmfupto = MUTT_ABORT;
  const struct Address *followup_to = TAILQ_FIRST(&in->mail_followup_to);

  if ((flags & (SEND_LIST_REPLY | SEND_GROUP_REPLY | SEND_GROUP_CHAT_REPLY)) && followup_to)
  {
    char prompt[256];
    snprintf(prompt, sizeof(prompt), _("Follow-up to %s%s?"), followup_to->mailbox,
             TAILQ_NEXT(TAILQ_FIRST(&in->mail_followup_to), entries) ? ",..." : "");

    hmfupto = query_quadoption(C_HonorFollowupTo, prompt);
    if (hmfupto == MUTT_ABORT)
      return -1;
  }

  if (flags & SEND_LIST_REPLY)
  {
    add_mailing_lists(&out->to, &in->to, &in->cc);

    if (followup_to && (hmfupto == MUTT_YES) &&
        (default_to(&out->cc, in, flags & SEND_LIST_REPLY, (hmfupto == MUTT_YES)) == MUTT_ABORT))
    {
      return -1; /* abort */
    }
  }
  else if (flags & SEND_TO_SENDER)
  {
    mutt_addrlist_copy(&out->to, &in->from, false);
  }
  else
  {
    if (default_to(&out->to, in, flags & (SEND_GROUP_REPLY | SEND_GROUP_CHAT_REPLY),
                   (hmfupto == MUTT_YES)) == -1)
      return -1; /* abort */

    if ((flags & (SEND_GROUP_REPLY | SEND_GROUP_CHAT_REPLY)) &&
        (!followup_to || (hmfupto != MUTT_YES)))
    {
      /* if(!mutt_addr_is_user(in->to)) */
      if (flags & SEND_GROUP_REPLY)
        mutt_addrlist_copy(&out->cc, &in->to, true);
      else
        mutt_addrlist_copy(&out->to, &in->cc, true);
      mutt_addrlist_copy(&out->cc, &in->cc, true);
    }
  }
  return 0;
}

/**
 * add_references - Add the email's references to a list
 * @param head List of references
 * @param env    Envelope of message
 */
static void add_references(struct ListHead *head, struct Envelope *env)
{
  struct ListNode *np = NULL;

  struct ListHead *src = STAILQ_EMPTY(&env->references) ? &env->in_reply_to : &env->references;
  STAILQ_FOREACH(np, src, entries)
  {
    mutt_list_insert_tail(head, mutt_str_dup(np->data));
  }
}

/**
 * add_message_id - Add the email's message ID to a list
 * @param head List of message IDs
 * @param env  Envelope of message
 */
static void add_message_id(struct ListHead *head, struct Envelope *env)
{
  if (env->message_id)
  {
    mutt_list_insert_head(head, mutt_str_dup(env->message_id));
  }
}

/**
 * mutt_fix_reply_recipients - Remove duplicate recipients
 * @param env Envelope to fix
 */
void mutt_fix_reply_recipients(struct Envelope *env)
{
  if (!C_Metoo)
  {
    /* the order is important here.  do the CC: first so that if the
     * the user is the only recipient, it ends up on the TO: field */
    remove_user(&env->cc, TAILQ_EMPTY(&env->to));
    remove_user(&env->to, TAILQ_EMPTY(&env->cc) || C_ReplySelf);
  }

  /* the CC field can get cluttered, especially with lists */
  mutt_addrlist_dedupe(&env->to);
  mutt_addrlist_dedupe(&env->cc);
  mutt_addrlist_remove_xrefs(&env->to, &env->cc);

  if (!TAILQ_EMPTY(&env->cc) && TAILQ_EMPTY(&env->to))
  {
    TAILQ_SWAP(&env->to, &env->cc, Address, entries);
  }
}

/**
 * mutt_make_forward_subject - Create a subject for a forwarded email
 * @param env Envelope for result
 * @param m   Mailbox
 * @param e Email
 */
void mutt_make_forward_subject(struct Envelope *env, struct Mailbox *m, struct Email *e)
{
  if (!env)
    return;

  char buf[256];

  /* set the default subject for the message. */
  mutt_make_string(buf, sizeof(buf), 0, NONULL(C_ForwardFormat), NULL, m, e);
  mutt_str_replace(&env->subject, buf);
}

/**
 * mutt_make_misc_reply_headers - Set subject for a reply
 * @param env    Envelope for result
 * @param curenv Envelope of source email
 */
void mutt_make_misc_reply_headers(struct Envelope *env, struct Envelope *curenv)
{
  if (!env || !curenv)
    return;

  /* This takes precedence over a subject that might have
   * been taken from a List-Post header.  Is that correct?  */
  if (curenv->real_subj)
  {
    FREE(&env->subject);
    env->subject = mutt_mem_malloc(mutt_str_len(curenv->real_subj) + 5);
    sprintf(env->subject, "Re: %s", curenv->real_subj);
  }
  else if (!env->subject)
    env->subject = mutt_str_dup(C_EmptySubject);
}

/**
 * mutt_add_to_reference_headers - Generate references for a reply email
 * @param env    Envelope for result
 * @param curenv Envelope of source email
 */
void mutt_add_to_reference_headers(struct Envelope *env, struct Envelope *curenv)
{
  add_references(&env->references, curenv);
  add_message_id(&env->references, curenv);
  add_message_id(&env->in_reply_to, curenv);

#ifdef USE_NNTP
  if (OptNewsSend && C_XCommentTo && !TAILQ_EMPTY(&curenv->from))
    env->x_comment_to = mutt_str_dup(mutt_get_name(TAILQ_FIRST(&curenv->from)));
#endif
}

/**
 * make_reference_headers - Generate reference headers for an email
 * @param el  List of source Emails
 * @param env Envelope for result
 */
static void make_reference_headers(struct EmailList *el, struct Envelope *env)
{
  if (!el || !env || STAILQ_EMPTY(el))
    return;

  struct EmailNode *en = STAILQ_FIRST(el);
  bool single = !STAILQ_NEXT(en, entries);

  if (!single)
  {
    STAILQ_FOREACH(en, el, entries)
    {
      mutt_add_to_reference_headers(env, en->email->env);
    }
  }
  else
    mutt_add_to_reference_headers(env, en->email->env);

  /* if there's more than entry in In-Reply-To (i.e. message has multiple
   * parents), don't generate a References: header as it's discouraged by
   * RFC2822, sect. 3.6.4 */
  if (!single && !STAILQ_EMPTY(&env->in_reply_to) &&
      STAILQ_NEXT(STAILQ_FIRST(&env->in_reply_to), entries))
  {
    mutt_list_free(&env->references);
  }
}

/**
 * envelope_defaults - Fill in some defaults for a new email
 * @param env   Envelope for result
 * @param m     Mailbox
 * @param el    List of Emails to use
 * @param flags Flags, see #SendFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int envelope_defaults(struct Envelope *env, struct Mailbox *m,
                             struct EmailList *el, SendFlags flags)
{
  if (!el || STAILQ_EMPTY(el))
    return -1;

  struct EmailNode *en = STAILQ_FIRST(el);
  bool single = !STAILQ_NEXT(en, entries);

  struct Envelope *curenv = en->email->env;
  if (!curenv)
    return -1;

  if (flags & (SEND_REPLY | SEND_TO_SENDER))
  {
#ifdef USE_NNTP
    if ((flags & SEND_NEWS))
    {
      /* in case followup set Newsgroups: with Followup-To: if it present */
      if (!env->newsgroups && !mutt_istr_equal(curenv->followup_to, "poster"))
      {
        env->newsgroups = mutt_str_dup(curenv->followup_to);
      }
    }
    else
#endif
        if (!single)
    {
      STAILQ_FOREACH(en, el, entries)
      {
        if (mutt_fetch_recips(env, en->email->env, flags) == -1)
          return -1;
      }
    }
    else if (mutt_fetch_recips(env, curenv, flags) == -1)
      return -1;

    if ((flags & SEND_LIST_REPLY) && TAILQ_EMPTY(&env->to))
    {
      mutt_error(_("No mailing lists found"));
      return -1;
    }

    if (flags & SEND_REPLY)
    {
      mutt_make_misc_reply_headers(env, curenv);
      make_reference_headers(el, env);
    }
  }
  else if (flags & SEND_FORWARD)
  {
    mutt_make_forward_subject(env, m, en->email);
    if (C_ForwardReferences)
      make_reference_headers(el, env);
  }

  return 0;
}

/**
 * generate_body - Create a new email body
 * @param fp_tmp Stream for outgoing message
 * @param e      Email for outgoing message
 * @param flags  Compose mode, see #SendFlags
 * @param m      Mailbox
 * @param el     List of Emails to use
 * @retval  0 Success
 * @retval -1 Error
 */
static int generate_body(FILE *fp_tmp, struct Email *e, SendFlags flags,
                         struct Mailbox *m, struct EmailList *el)
{
  /* An EmailList is required for replying and forwarding */
  if (!el && (flags & (SEND_REPLY | SEND_FORWARD)))
    return -1;

  if (flags & SEND_REPLY)
  {
    enum QuadOption ans =
        query_quadoption(C_Include, _("Include message in reply?"));
    if (ans == MUTT_ABORT)
      return -1;

    if (ans == MUTT_YES)
    {
      mutt_message(_("Including quoted message..."));
      struct EmailNode *en = NULL;
      STAILQ_FOREACH(en, el, entries)
      {
        if (include_reply(m, en->email, fp_tmp) == -1)
        {
          mutt_error(_("Could not include all requested messages"));
          return -1;
        }
        if (STAILQ_NEXT(en, entries) != NULL)
        {
          fputc('\n', fp_tmp);
        }
      }
    }
  }
  else if (flags & SEND_FORWARD)
  {
    enum QuadOption ans =
        query_quadoption(C_MimeForward, _("Forward as attachment?"));
    if (ans == MUTT_YES)
    {
      struct Body *last = e->content;

      mutt_message(_("Preparing forwarded message..."));

      while (last && last->next)
        last = last->next;

      struct EmailNode *en = NULL;
      STAILQ_FOREACH(en, el, entries)
      {
        struct Body *tmp = mutt_make_message_attach(m, en->email, false);
        if (last)
        {
          last->next = tmp;
          last = tmp;
        }
        else
        {
          last = tmp;
          e->content = tmp;
        }
      }
    }
    else if (ans != MUTT_ABORT)
    {
      enum QuadOption forwardq = MUTT_ABORT;
      struct Body **last = NULL;
      struct EmailNode *en = NULL;

      if (C_ForwardDecode && (C_ForwardAttachments != MUTT_NO))
      {
        last = &e->content;
        while (*last)
          last = &((*last)->next);
      }

      STAILQ_FOREACH(en, el, entries)
      {
        struct Email *e_cur = en->email;
        include_forward(m, e_cur, fp_tmp);
        if (C_ForwardDecode && (C_ForwardAttachments != MUTT_NO))
        {
          if (inline_forward_attachments(m, e_cur, &last, &forwardq) != 0)
            return -1;
        }
      }
    }
    else
      return -1;
  }
  /* if (WithCrypto && (flags & SEND_KEY)) */
  else if (((WithCrypto & APPLICATION_PGP) != 0) && (flags & SEND_KEY))
  {
    struct Body *b = NULL;

    if (((WithCrypto & APPLICATION_PGP) != 0) && !(b = crypt_pgp_make_key_attachment()))
    {
      return -1;
    }

    b->next = e->content;
    e->content = b;
  }

  mutt_clear_error();

  return 0;
}

/**
 * mutt_set_followup_to - Set followup-to field
 * @param env Envelope to modify
 */
void mutt_set_followup_to(struct Envelope *env)
{
  /* Only generate the Mail-Followup-To if the user has requested it, and
   * it hasn't already been set */

  if (!C_FollowupTo)
    return;
#ifdef USE_NNTP
  if (OptNewsSend)
  {
    if (!env->followup_to && env->newsgroups && (strrchr(env->newsgroups, ',')))
      env->followup_to = mutt_str_dup(env->newsgroups);
    return;
  }
#endif

  if (TAILQ_EMPTY(&env->mail_followup_to))
  {
    if (mutt_is_list_recipient(false, env))
    {
      /* this message goes to known mailing lists, so create a proper
       * mail-followup-to header */

      mutt_addrlist_copy(&env->mail_followup_to, &env->to, false);
      mutt_addrlist_copy(&env->mail_followup_to, &env->cc, true);
    }

    /* remove ourselves from the mail-followup-to header */
    remove_user(&env->mail_followup_to, false);

    /* If we are not subscribed to any of the lists in question, re-add
     * ourselves to the mail-followup-to header.  The mail-followup-to header
     * generated is a no-op with group-reply, but makes sure list-reply has the
     * desired effect.  */

    if (!TAILQ_EMPTY(&env->mail_followup_to) &&
        !mutt_is_subscribed_list_recipient(false, env))
    {
      struct AddressList *al = NULL;
      if (!TAILQ_EMPTY(&env->reply_to))
        al = &env->reply_to;
      else if (!TAILQ_EMPTY(&env->from))
        al = &env->from;

      if (al)
      {
        struct Address *a = NULL;
        TAILQ_FOREACH_REVERSE(a, al, AddressList, entries)
        {
          mutt_addrlist_prepend(&env->mail_followup_to, mutt_addr_copy(a));
        }
      }
      else
      {
        mutt_addrlist_prepend(&env->mail_followup_to, mutt_default_from());
      }
    }

    mutt_addrlist_dedupe(&env->mail_followup_to);
  }
}

/**
 * set_reverse_name - Try to set the 'from' field from the recipients
 * @param al  AddressList to prepend the found address
 * @param env Envelope to use
 *
 * Look through the recipients of the message we are replying to, and if we
 * find an address that matches $alternates, we use that as the default from
 * field
 */
static void set_reverse_name(struct AddressList *al, struct Envelope *env)
{
  struct Address *a = NULL;
  if (TAILQ_EMPTY(al))
  {
    TAILQ_FOREACH(a, &env->to, entries)
    {
      if (mutt_addr_is_user(a))
      {
        mutt_addrlist_append(al, mutt_addr_copy(a));
        break;
      }
    }
  }

  if (TAILQ_EMPTY(al))
  {
    TAILQ_FOREACH(a, &env->cc, entries)
    {
      if (mutt_addr_is_user(a))
      {
        mutt_addrlist_append(al, mutt_addr_copy(a));
        break;
      }
    }
  }

  if (TAILQ_EMPTY(al))
  {
    struct Address *from = TAILQ_FIRST(&env->from);
    if (from && mutt_addr_is_user(from))
    {
      mutt_addrlist_append(al, mutt_addr_copy(from));
    }
  }

  if (!TAILQ_EMPTY(al))
  {
    /* when $reverse_realname is not set, clear the personal name so that it
     * may be set via a reply- or send-hook.  */
    if (!C_ReverseRealname)
      FREE(&TAILQ_FIRST(al)->personal);
  }
}

/**
 * mutt_default_from - Get a default 'from' Address
 * @retval ptr Newly allocated Address
 */
struct Address *mutt_default_from(void)
{
  /* Note: We let $from override $realname here.
   *       Is this the right thing to do?
   */

  if (C_From)
  {
    return mutt_addr_copy(C_From);
  }
  else if (C_UseDomain)
  {
    struct Address *addr = mutt_addr_new();
    mutt_str_asprintf(&addr->mailbox, "%s@%s", NONULL(Username), NONULL(mutt_fqdn(true)));
    return addr;
  }
  else
  {
    return mutt_addr_create(NULL, Username);
  }
}

/**
 * invoke_mta - Send an email
 * @param e Email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int invoke_mta(struct Email *e)
{
  struct Buffer *tempfile = NULL;
  int rc = -1;
#ifdef USE_SMTP
  short old_write_bcc;
#endif

  /* Write out the message in MIME form. */
  tempfile = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfile);
  FILE *fp_tmp = mutt_file_fopen(mutt_b2s(tempfile), "w");
  if (!fp_tmp)
    goto cleanup;

#ifdef USE_SMTP
  old_write_bcc = C_WriteBcc;
  if (C_SmtpUrl)
    C_WriteBcc = false;
#endif
#ifdef MIXMASTER
  mutt_rfc822_write_header(fp_tmp, e->env, e->content, MUTT_WRITE_HEADER_NORMAL,
                           !STAILQ_EMPTY(&e->chain),
                           mutt_should_hide_protected_subject(e));
#endif
#ifndef MIXMASTER
  mutt_rfc822_write_header(fp_tmp, e->env, e->content, MUTT_WRITE_HEADER_NORMAL,
                           false, mutt_should_hide_protected_subject(e));
#endif
#ifdef USE_SMTP
  if (old_write_bcc)
    C_WriteBcc = true;
#endif

  fputc('\n', fp_tmp); /* tie off the header. */

  if ((mutt_write_mime_body(e->content, fp_tmp) == -1))
    goto cleanup;

  if (mutt_file_fclose(&fp_tmp) != 0)
  {
    mutt_perror(mutt_b2s(tempfile));
    unlink(mutt_b2s(tempfile));
    goto cleanup;
  }

#ifdef MIXMASTER
  if (!STAILQ_EMPTY(&e->chain))
  {
    rc = mix_send_message(&e->chain, mutt_b2s(tempfile));
    goto cleanup;
  }
#endif

#ifdef USE_NNTP
  if (OptNewsSend)
    goto sendmail;
#endif

#ifdef USE_SMTP
  if (C_SmtpUrl)
  {
    rc = mutt_smtp_send(&e->env->from, &e->env->to, &e->env->cc, &e->env->bcc,
                        mutt_b2s(tempfile), (e->content->encoding == ENC_8BIT));
    goto cleanup;
  }
#endif

sendmail:
  rc = mutt_invoke_sendmail(&e->env->from, &e->env->to, &e->env->cc, &e->env->bcc,
                            mutt_b2s(tempfile), (e->content->encoding == ENC_8BIT));
cleanup:
  if (fp_tmp)
  {
    mutt_file_fclose(&fp_tmp);
    unlink(mutt_b2s(tempfile));
  }
  mutt_buffer_pool_release(&tempfile);
  return rc;
}

/**
 * mutt_encode_descriptions - rfc2047 encode the content-descriptions
 * @param b       Body of email
 * @param recurse If true, encode children parts
 */
void mutt_encode_descriptions(struct Body *b, bool recurse)
{
  for (struct Body *t = b; t; t = t->next)
  {
    if (t->description)
    {
      rfc2047_encode(&t->description, NULL, sizeof("Content-Description:"), C_SendCharset);
    }
    if (recurse && t->parts)
      mutt_encode_descriptions(t->parts, recurse);
  }
}

/**
 * decode_descriptions - rfc2047 decode them in case of an error
 * @param b MIME parts to decode
 */
static void decode_descriptions(struct Body *b)
{
  for (struct Body *t = b; t; t = t->next)
  {
    if (t->description)
    {
      rfc2047_decode(&t->description);
    }
    if (t->parts)
      decode_descriptions(t->parts);
  }
}

/**
 * fix_end_of_file - Ensure a file ends with a linefeed
 * @param data Name of file to fix
 */
static void fix_end_of_file(const char *data)
{
  FILE *fp = mutt_file_fopen(data, "a+");
  if (!fp)
    return;
  if (fseek(fp, -1, SEEK_END) >= 0)
  {
    int c = fgetc(fp);
    if (c != '\n')
      fputc('\n', fp);
  }
  mutt_file_fclose(&fp);
}

/**
 * mutt_resend_message - Resend an email
 * @param fp    File containing email
 * @param ctx   Mailbox
 * @param e_cur Email to resend
 * @retval  0 Message was successfully sent
 * @retval -1 Message was aborted or an error occurred
 * @retval  1 Message was postponed
 */
int mutt_resend_message(FILE *fp, struct Context *ctx, struct Email *e_cur)
{
  struct Email *e_new = email_new();

  if (mutt_prepare_template(fp, ctx->mailbox, e_new, e_cur, true) < 0)
  {
    email_free(&e_new);
    return -1;
  }

  if (WithCrypto)
  {
    /* mutt_prepare_template doesn't always flip on an application bit.
     * so fix that here */
    if (!(e_new->security & (APPLICATION_SMIME | APPLICATION_PGP)))
    {
      if (((WithCrypto & APPLICATION_SMIME) != 0) && C_SmimeIsDefault)
        e_new->security |= APPLICATION_SMIME;
      else if (WithCrypto & APPLICATION_PGP)
        e_new->security |= APPLICATION_PGP;
      else
        e_new->security |= APPLICATION_SMIME;
    }

    if (C_CryptOpportunisticEncrypt)
    {
      e_new->security |= SEC_OPPENCRYPT;
      crypt_opportunistic_encrypt(e_new);
    }
  }

  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, e_cur);
  int rc = mutt_send_message(SEND_RESEND, e_new, NULL, ctx, &el);
  emaillist_clear(&el);

  return rc;
}

/**
 * is_reply - Is one email a reply to another?
 * @param reply Email to test
 * @param orig  Original email
 * @retval true  It is a reply
 * @retval false It is not a reply
 */
static bool is_reply(struct Email *reply, struct Email *orig)
{
  if (!reply || !reply->env || !orig || !orig->env)
    return false;
  return mutt_list_find(&orig->env->references, reply->env->message_id) ||
         mutt_list_find(&orig->env->in_reply_to, reply->env->message_id);
}

/**
 * search_attach_keyword - Search an email for 'attachment' keywords
 * @param filename Filename
 * @retval true If the regex matches in the email
 *
 * Search an email for the regex in $abort_noattach_regex.
 * A match might indicate that the user should have attached something.
 *
 * @note Quoted lines (as defined by $quote_regex) are ignored
 */
static bool search_attach_keyword(char *filename)
{
  /* Search for the regex in C_AbortNoattachRegex within a file */
  if (!C_AbortNoattachRegex || !C_AbortNoattachRegex->regex || !C_QuoteRegex ||
      !C_QuoteRegex->regex)
  {
    return false;
  }

  FILE *fp_att = mutt_file_fopen(filename, "r");
  if (!fp_att)
    return false;

  char *inputline = mutt_mem_malloc(1024);
  bool found = false;
  while (!feof(fp_att))
  {
    fgets(inputline, 1024, fp_att);
    if (!mutt_is_quote_line(inputline, NULL) &&
        mutt_regex_match(C_AbortNoattachRegex, inputline))
    {
      found = true;
      break;
    }
  }
  FREE(&inputline);
  mutt_file_fclose(&fp_att);
  return found;
}

/**
 * save_fcc - Save an Email to a 'sent mail' folder
 * @param[in]  e             Email to save
 * @param[in]  fcc           Folder to save to (can be comma-separated list)
 * @param[in]  clear_content Cleartext content of Email
 * @param[in]  pgpkeylist    List of pgp keys
 * @param[in]  flags         Send mode, see #SendFlags
 * @param[out] finalpath     Path of final folder
 * @retval  0 Success
 * @retval -1 Error
 */
static int save_fcc(struct Email *e, struct Buffer *fcc, struct Body *clear_content,
                    char *pgpkeylist, SendFlags flags, char **finalpath)
{
  int rc = 0;
  struct Body *save_content = NULL;

  mutt_buffer_expand_path(fcc);

  /* Don't save a copy when we are in batch-mode, and the FCC
   * folder is on an IMAP server: This would involve possibly lots
   * of user interaction, which is not available in batch mode.
   *
   * Note: A patch to fix the problems with the use of IMAP servers
   * from non-curses mode is available from Brendan Cully.  However,
   * I'd like to think a bit more about this before including it.  */

#ifdef USE_IMAP
  if ((flags & SEND_BATCH) && !mutt_buffer_is_empty(fcc) &&
      (imap_path_probe(mutt_b2s(fcc), NULL) == MUTT_IMAP))
  {
    mutt_buffer_reset(fcc);
    mutt_error(_("Fcc to an IMAP mailbox is not supported in batch mode"));
    return rc;
  }
#endif

  if (mutt_buffer_is_empty(fcc) || mutt_str_equal("/dev/null", mutt_b2s(fcc)))
    return rc;

  struct Body *tmpbody = e->content;
  struct Body *save_sig = NULL;
  struct Body *save_parts = NULL;

  /* Before sending, we don't allow message manipulation because it
   * will break message signatures.  This is especially complicated by
   * Protected Headers. */
  if (!C_FccBeforeSend)
  {
    if ((WithCrypto != 0) && (e->security & (SEC_ENCRYPT | SEC_SIGN | SEC_AUTOCRYPT)) && C_FccClear)
    {
      e->content = clear_content;
      e->security &= ~(SEC_ENCRYPT | SEC_SIGN | SEC_AUTOCRYPT);
      mutt_env_free(&e->content->mime_headers);
    }

    /* check to see if the user wants copies of all attachments */
    bool save_atts = true;
    if (e->content->type == TYPE_MULTIPART)
    {
      /* In batch mode, save attachments if the quadoption is yes or ask-yes */
      if (flags & SEND_BATCH)
      {
        if ((C_FccAttach == MUTT_NO) || (C_FccAttach == MUTT_ASKNO))
          save_atts = false;
      }
      else if (query_quadoption(C_FccAttach, _("Save attachments in Fcc?")) != MUTT_YES)
        save_atts = false;
    }
    if (!save_atts)
    {
      if ((WithCrypto != 0) && (e->security & (SEC_ENCRYPT | SEC_SIGN | SEC_AUTOCRYPT)) &&
          (mutt_str_equal(e->content->subtype, "encrypted") ||
           mutt_str_equal(e->content->subtype, "signed")))
      {
        if ((clear_content->type == TYPE_MULTIPART) &&
            (query_quadoption(C_FccAttach, _("Save attachments in Fcc?")) == MUTT_NO))
        {
          if (!(e->security & SEC_ENCRYPT) && (e->security & SEC_SIGN))
          {
            /* save initial signature and attachments */
            save_sig = e->content->parts->next;
            save_parts = clear_content->parts->next;
          }

          /* this means writing only the main part */
          e->content = clear_content->parts;

          if (mutt_protect(e, pgpkeylist, false) == -1)
          {
            /* we can't do much about it at this point, so
           * fallback to saving the whole thing to fcc */
            e->content = tmpbody;
            save_sig = NULL;
            goto full_fcc;
          }

          save_content = e->content;
        }
      }
      else
      {
        if (query_quadoption(C_FccAttach, _("Save attachments in Fcc?")) == MUTT_NO)
          e->content = e->content->parts;
      }
    }
  }

full_fcc:
  if (e->content)
  {
    /* update received time so that when storing to a mbox-style folder
     * the From_ line contains the current time instead of when the
     * message was first postponed.  */
    e->received = mutt_date_epoch();
    rc = mutt_write_multiple_fcc(mutt_b2s(fcc), e, NULL, false, NULL, finalpath);
    while (rc && !(flags & SEND_BATCH))
    {
      mutt_clear_error();
      int choice = mutt_multi_choice(
          /* L10N: Called when saving to $record or Fcc failed after sending.
             (r)etry tries the same mailbox again.
             alternate (m)ailbox prompts for a different mailbox to try.
             (s)kip aborts saving.  */
          _("Fcc failed. (r)etry, alternate (m)ailbox, or (s)kip?"),
          /* L10N: These correspond to the "Fcc failed" multi-choice prompt
             (r)etry, alternate (m)ailbox, or (s)kip.
             Any similarity to famous leaders of the FSF is coincidental.  */
          _("rms"));
      switch (choice)
      {
        case 2: /* alternate (m)ailbox */
          /* L10N: This is the prompt to enter an "alternate (m)ailbox" when the
             initial Fcc fails.  */
          rc = mutt_buffer_enter_fname(_("Fcc mailbox"), fcc, true);
          if ((rc == -1) || mutt_buffer_is_empty(fcc))
          {
            rc = 0;
            break;
          }
          /* fall through */

        case 1: /* (r)etry */
          rc = mutt_write_multiple_fcc(mutt_b2s(fcc), e, NULL, false, NULL, finalpath);
          break;

        case -1: /* abort */
        case 3:  /* (s)kip */
          rc = 0;
          break;
      }
    }
  }

  if (!C_FccBeforeSend)
  {
    e->content = tmpbody;

    if ((WithCrypto != 0) && save_sig)
    {
      /* cleanup the second signature structures */
      if (save_content->parts)
      {
        mutt_body_free(&save_content->parts->next);
        save_content->parts = NULL;
      }
      mutt_body_free(&save_content);

      /* restore old signature and attachments */
      e->content->parts->next = save_sig;
      e->content->parts->parts->next = save_parts;
    }
    else if ((WithCrypto != 0) && save_content)
    {
      /* destroy the new encrypted body. */
      mutt_body_free(&save_content);
    }
  }

  return 0;
}

/**
 * postpone_message - Save an Email for another day
 * @param e_post Email to postpone
 * @param e_cur  Current Email in the index
 * @param fcc    Folder for 'sent mail'
 * @param flags  Send mode, see #SendFlags
 * @retval  0 Success
 * @retval -1 Error
 */
static int postpone_message(struct Email *e_post, struct Email *e_cur,
                            const char *fcc, SendFlags flags)
{
  char *pgpkeylist = NULL;
  char *encrypt_as = NULL;
  struct Body *clear_content = NULL;

  if (!C_Postponed)
  {
    mutt_error(_("Can't postpone.  $postponed is unset"));
    return -1;
  }

  if (e_post->content->next)
    e_post->content = mutt_make_multipart(e_post->content);

  mutt_encode_descriptions(e_post->content, true);

  if ((WithCrypto != 0) && C_PostponeEncrypt &&
      (e_post->security & (SEC_ENCRYPT | SEC_AUTOCRYPT)))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && (e_post->security & APPLICATION_PGP))
      encrypt_as = C_PgpDefaultKey;
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && (e_post->security & APPLICATION_SMIME))
      encrypt_as = C_SmimeDefaultKey;
    if (!encrypt_as)
      encrypt_as = C_PostponeEncryptAs;

#ifdef USE_AUTOCRYPT
    if (e_post->security & SEC_AUTOCRYPT)
    {
      if (mutt_autocrypt_set_sign_as_default_key(e_post))
      {
        e_post->content = mutt_remove_multipart(e_post->content);
        decode_descriptions(e_post->content);
        mutt_error(_("Error encrypting message. Check your crypt settings."));
        return -1;
      }
      encrypt_as = AutocryptDefaultKey;
    }
#endif

    if (encrypt_as)
    {
      pgpkeylist = mutt_str_dup(encrypt_as);
      clear_content = e_post->content;
      if (mutt_protect(e_post, pgpkeylist, true) == -1)
      {
        FREE(&pgpkeylist);
        e_post->content = mutt_remove_multipart(e_post->content);
        decode_descriptions(e_post->content);
        mutt_error(_("Error encrypting message. Check your crypt settings."));
        return -1;
      }

      FREE(&pgpkeylist);

      mutt_encode_descriptions(e_post->content, false);
    }
  }

  /* make sure the message is written to the right part of a maildir
   * postponed folder.  */
  e_post->read = false;
  e_post->old = false;

  mutt_prepare_envelope(e_post->env, false);
  mutt_env_to_intl(e_post->env, NULL, NULL); /* Handle bad IDNAs the next time. */

  if (mutt_write_fcc(NONULL(C_Postponed), e_post,
                     (e_cur && (flags & SEND_REPLY)) ? e_cur->env->message_id : NULL,
                     true, fcc, NULL) < 0)
  {
    if (clear_content)
    {
      mutt_body_free(&e_post->content);
      e_post->content = clear_content;
    }
    mutt_env_free(&e_post->content->mime_headers); /* protected headers */
    e_post->content = mutt_remove_multipart(e_post->content);
    decode_descriptions(e_post->content);
    mutt_unprepare_envelope(e_post->env);
    return -1;
  }

  mutt_update_num_postponed();

  if (clear_content)
    mutt_body_free(&clear_content);

  return 0;
}

/**
 * mutt_send_message - Send an email
 * @param flags    Send mode, see #SendFlags
 * @param e_templ  Template to use for new message
 * @param tempfile File specified by -i or -H
 * @param ctx      Current mailbox
 * @param el       List of Emails to send
 * @retval  0 Message was successfully sent
 * @retval -1 Message was aborted or an error occurred
 * @retval  1 Message was postponed
 */
int mutt_send_message(SendFlags flags, struct Email *e_templ, const char *tempfile,
                      struct Context *ctx, struct EmailList *el)
{
  char buf[1024];
  struct Buffer fcc = mutt_buffer_make(0); /* where to copy this message */
  FILE *fp_tmp = NULL;
  struct Body *pbody = NULL;
  int i;
  bool free_clear_content = false;

  struct Body *clear_content = NULL;
  char *pgpkeylist = NULL;
  /* save current value of "pgp_sign_as"  and "smime_default_key" */
  char *pgp_signas = NULL;
  char *smime_signas = NULL;
  const char *tag = NULL;
  char *err = NULL;
  char *ctype = NULL;
  char *finalpath = NULL;
  struct EmailNode *en = NULL;
  struct Email *e_cur = NULL;

  if (el)
    en = STAILQ_FIRST(el);
  if (en)
    e_cur = STAILQ_NEXT(en, entries) ? NULL : en->email;

  int rc = -1;

#ifdef USE_NNTP
  if (flags & SEND_NEWS)
    OptNewsSend = true;
  else
    OptNewsSend = false;
#endif

  if (!flags && !e_templ && (C_Recall != MUTT_NO) &&
      mutt_num_postponed(ctx ? ctx->mailbox : NULL, true))
  {
    /* If the user is composing a new message, check to see if there
     * are any postponed messages first.  */
    enum QuadOption ans =
        query_quadoption(C_Recall, _("Recall postponed message?"));
    if (ans == MUTT_ABORT)
      return rc;

    if (ans == MUTT_YES)
      flags |= SEND_POSTPONED;
  }

  /* Allocate the buffer due to the long lifetime, but
   * pre-resize it to ensure there are no NULL data field issues */
  mutt_buffer_alloc(&fcc, 1024);

  if (flags & SEND_POSTPONED)
  {
    if (WithCrypto & APPLICATION_PGP)
      pgp_signas = mutt_str_dup(C_PgpSignAs);
    if (WithCrypto & APPLICATION_SMIME)
      smime_signas = mutt_str_dup(C_SmimeSignAs);
  }

  /* Delay expansion of aliases until absolutely necessary--shouldn't
   * be necessary unless we are prompting the user or about to execute a
   * send-hook.  */

  if (!e_templ)
  {
    e_templ = email_new();

    if (flags == SEND_POSTPONED)
    {
      rc = mutt_get_postponed(ctx, e_templ, &e_cur, &fcc);
      if (rc < 0)
      {
        flags = SEND_POSTPONED;
        goto cleanup;
      }
      flags = rc;
#ifdef USE_NNTP
      /* If postponed message is a news article, it have
       * a "Newsgroups:" header line, then set appropriate flag.  */
      if (e_templ->env->newsgroups)
      {
        flags |= SEND_NEWS;
        OptNewsSend = true;
      }
      else
      {
        flags &= ~SEND_NEWS;
        OptNewsSend = false;
      }
#endif
    }

    if (flags & (SEND_POSTPONED | SEND_RESEND))
    {
      fp_tmp = mutt_file_fopen(e_templ->content->filename, "a+");
      if (!fp_tmp)
      {
        mutt_perror(e_templ->content->filename);
        goto cleanup;
      }
    }

    if (!e_templ->env)
      e_templ->env = mutt_env_new();
  }

  /* Parse and use an eventual list-post header */
  if ((flags & SEND_LIST_REPLY) && e_cur && e_cur->env && e_cur->env->list_post)
  {
    /* Use any list-post header as a template */
    mutt_parse_mailto(e_templ->env, NULL, e_cur->env->list_post);
    /* We don't let them set the sender's address. */
    mutt_addrlist_clear(&e_templ->env->from);
  }

  if (!(flags & (SEND_KEY | SEND_POSTPONED | SEND_RESEND)))
  {
    /* When SEND_DRAFT_FILE is set, the caller has already
     * created the "parent" body structure.  */
    if (!(flags & SEND_DRAFT_FILE))
    {
      pbody = mutt_body_new();
      pbody->next = e_templ->content; /* don't kill command-line attachments */
      e_templ->content = pbody;

      ctype = mutt_str_dup(C_ContentType);
      if (!ctype)
        ctype = mutt_str_dup("text/plain");
      mutt_parse_content_type(ctype, e_templ->content);
      FREE(&ctype);
      e_templ->content->unlink = true;
      e_templ->content->use_disp = false;
      e_templ->content->disposition = DISP_INLINE;

      if (tempfile)
      {
        fp_tmp = mutt_file_fopen(tempfile, "a+");
        e_templ->content->filename = mutt_str_dup(tempfile);
      }
      else
      {
        mutt_mktemp(buf, sizeof(buf));
        fp_tmp = mutt_file_fopen(buf, "w+");
        e_templ->content->filename = mutt_str_dup(buf);
      }
    }
    else
      fp_tmp = mutt_file_fopen(e_templ->content->filename, "a+");

    if (!fp_tmp)
    {
      mutt_debug(LL_DEBUG1, "can't create tempfile %s (errno=%d)\n",
                 e_templ->content->filename, errno);
      mutt_perror(e_templ->content->filename);
      goto cleanup;
    }
  }

  /* this is handled here so that the user can match ~f in send-hook */
  if (e_cur && C_ReverseName && !(flags & (SEND_POSTPONED | SEND_RESEND)))
  {
    /* We shouldn't have to worry about alias expansion here since we are
     * either replying to a real or postponed message, therefore no aliases
     * should exist since the user has not had the opportunity to add
     * addresses to the list.  We just have to ensure the postponed messages
     * have their aliases expanded.  */

    if (!TAILQ_EMPTY(&e_templ->env->from))
    {
      mutt_debug(LL_DEBUG5, "e_templ->env->from before set_reverse_name: %s\n",
                 TAILQ_FIRST(&e_templ->env->from)->mailbox);
      mutt_addrlist_clear(&e_templ->env->from);
    }
    set_reverse_name(&e_templ->env->from, e_cur->env);
  }
  if (e_cur && C_ReplyWithXorig && !(flags & (SEND_POSTPONED | SEND_RESEND | SEND_FORWARD)))
  {
    /* We shouldn't have to worry about freeing 'e_templ->env->from' before
     * setting it here since this code will only execute when doing some
     * sort of reply. The pointer will only be set when using the -H command
     * line option.
     *
     * If there is already a from address recorded in 'e_templ->env->from',
     * then it theoretically comes from C_ReverseName handling, and we don't use
     * the 'X-Orig-To header'.  */
    if (!TAILQ_EMPTY(&e_cur->env->x_original_to) && TAILQ_EMPTY(&e_templ->env->from))
    {
      mutt_addrlist_copy(&e_templ->env->from, &e_cur->env->x_original_to, false);
      mutt_debug(LL_DEBUG5, "e_templ->env->from extracted from X-Original-To: header: %s\n",
                 TAILQ_FIRST(&e_templ->env->from)->mailbox);
    }
  }

  if (!(flags & (SEND_POSTPONED | SEND_RESEND)) &&
      !((flags & SEND_DRAFT_FILE) && C_ResumeDraftFiles))
  {
    if ((flags & (SEND_REPLY | SEND_FORWARD | SEND_TO_SENDER)) && ctx &&
        (envelope_defaults(e_templ->env, ctx->mailbox, el, flags) == -1))
    {
      goto cleanup;
    }

    if (C_Hdrs)
      process_user_recips(e_templ->env);

    /* Expand aliases and remove duplicates/crossrefs */
    mutt_expand_aliases_env(e_templ->env);

    if (flags & SEND_REPLY)
      mutt_fix_reply_recipients(e_templ->env);

#ifdef USE_NNTP
    if ((flags & SEND_NEWS) && ctx && (ctx->mailbox->type == MUTT_NNTP) &&
        !e_templ->env->newsgroups)
    {
      e_templ->env->newsgroups =
          mutt_str_dup(((struct NntpMboxData *) ctx->mailbox->mdata)->group);
    }
#endif

    if (!(flags & SEND_BATCH) && !(C_Autoedit && C_EditHeaders) &&
        !((flags & SEND_REPLY) && C_FastReply))
    {
      if (edit_envelope(e_templ->env, flags) == -1)
        goto cleanup;
    }

    /* the from address must be set here regardless of whether or not
     * $use_from is set so that the '~P' (from you) operator in send-hook
     * patterns will work.  if $use_from is unset, the from address is killed
     * after send-hooks are evaluated */

    const bool killfrom = TAILQ_EMPTY(&e_templ->env->from);
    if (killfrom)
    {
      mutt_addrlist_append(&e_templ->env->from, mutt_default_from());
    }

    if ((flags & SEND_REPLY) && e_cur)
    {
      /* change setting based upon message we are replying to */
      mutt_message_hook(ctx ? ctx->mailbox : NULL, e_cur, MUTT_REPLY_HOOK);

      /* set the replied flag for the message we are generating so that the
       * user can use ~Q in a send-hook to know when reply-hook's are also
       * being used.  */
      e_templ->replied = true;
    }

    /* change settings based upon recipients */

    mutt_message_hook(NULL, e_templ, MUTT_SEND_HOOK);

    /* Unset the replied flag from the message we are composing since it is
     * no longer required.  This is done here because the FCC'd copy of
     * this message was erroneously get the 'R'eplied flag when stored in
     * a maildir-style mailbox.  */
    e_templ->replied = false;

    /* $use_from and/or $from might have changed in a send-hook */
    if (killfrom)
    {
      mutt_addrlist_clear(&e_templ->env->from);
      if (C_UseFrom && !(flags & (SEND_POSTPONED | SEND_RESEND)))
        mutt_addrlist_append(&e_templ->env->from, mutt_default_from());
    }

    if (C_Hdrs)
      process_user_header(e_templ->env);

    if (flags & SEND_BATCH)
    {
      if (mutt_file_copy_stream(stdin, fp_tmp) < 1)
      {
        mutt_error(_("Refusing to send an empty email"));
        mutt_message(_("Try: echo | neomutt -s 'subject' user@example.com"));
        goto cleanup;
      }
    }

    if (C_SigOnTop && !(flags & (SEND_KEY | SEND_BATCH)) && C_Editor)
    {
      append_signature(fp_tmp);
    }

    /* include replies/forwarded messages, unless we are given a template */
    if (!tempfile && (ctx || !(flags & (SEND_REPLY | SEND_FORWARD))) &&
        (generate_body(fp_tmp, e_templ, flags, ctx ? ctx->mailbox : NULL, el) == -1))
    {
      goto cleanup;
    }

    if (!C_SigOnTop && !(flags & (SEND_KEY | SEND_BATCH)) && C_Editor)
    {
      append_signature(fp_tmp);
    }
  }

  /* Only set format=flowed for new messages.  postponed/resent/draftfiles
   * should respect the original email.
   *
   * This is set here so that send-hook can be used to turn the option on.  */
  if (!(flags & (SEND_KEY | SEND_POSTPONED | SEND_RESEND | SEND_DRAFT_FILE)))
  {
    if (C_TextFlowed && (e_templ->content->type == TYPE_TEXT) &&
        mutt_istr_equal(e_templ->content->subtype, "plain"))
    {
      mutt_param_set(&e_templ->content->parameter, "format", "flowed");
    }
  }

  /* This hook is even called for postponed messages, and can, e.g., be
   * used for setting the editor, the sendmail path, or the
   * envelope sender.  */
  mutt_message_hook(NULL, e_templ, MUTT_SEND2_HOOK);

  /* wait until now to set the real name portion of our return address so
   * that $realname can be set in a send-hook */
  {
    struct Address *from = TAILQ_FIRST(&e_templ->env->from);
    if (from && !from->personal && !(flags & (SEND_RESEND | SEND_POSTPONED)))
      from->personal = mutt_str_dup(C_Realname);
  }

  if (!(((WithCrypto & APPLICATION_PGP) != 0) && (flags & SEND_KEY)))
    mutt_file_fclose(&fp_tmp);

  if (!(flags & SEND_BATCH))
  {
    struct stat st;
    time_t mtime = mutt_file_decrease_mtime(e_templ->content->filename, NULL);

    mutt_update_encoding(e_templ->content);

    /* Select whether or not the user's editor should be called now.  We
     * don't want to do this when:
     * 1) we are sending a key/cert
     * 2) we are forwarding a message and the user doesn't want to edit it.
     *    This is controlled by the quadoption $forward_edit.  However, if
     *    both $edit_headers and $autoedit are set, we want to ignore the
     *    setting of $forward_edit because the user probably needs to add the
     *    recipients.  */
    if (!(flags & SEND_KEY) &&
        (((flags & SEND_FORWARD) == 0) || (C_EditHeaders && C_Autoedit) ||
         (query_quadoption(C_ForwardEdit, _("Edit forwarded message?")) == MUTT_YES)))
    {
      /* If the this isn't a text message, look for a mailcap edit command */
      if (mutt_needs_mailcap(e_templ->content))
      {
        if (!mutt_edit_attachment(e_templ->content))
          goto cleanup;
      }
      else if (C_EditHeaders)
      {
        mutt_env_to_local(e_templ->env);
        mutt_edit_headers(C_Editor, e_templ->content->filename, e_templ, &fcc);
        mutt_env_to_intl(e_templ->env, NULL, NULL);
      }
      else
      {
        mutt_edit_file(C_Editor, e_templ->content->filename);
        if (stat(e_templ->content->filename, &st) == 0)
        {
          if (mtime != st.st_mtime)
            fix_end_of_file(e_templ->content->filename);
        }
        else
          mutt_perror(e_templ->content->filename);
      }

      mutt_message_hook(NULL, e_templ, MUTT_SEND2_HOOK);
    }

    if (!(flags & (SEND_POSTPONED | SEND_FORWARD | SEND_KEY | SEND_RESEND | SEND_DRAFT_FILE)))
    {
      if (stat(e_templ->content->filename, &st) == 0)
      {
        /* if the file was not modified, bail out now */
        if ((mtime == st.st_mtime) && !e_templ->content->next &&
            (query_quadoption(C_AbortUnmodified,
                              _("Abort unmodified message?")) == MUTT_YES))
        {
          mutt_message(_("Aborted unmodified message"));
          goto cleanup;
        }
      }
      else
        mutt_perror(e_templ->content->filename);
    }
  }

  /* Set the message security unless:
   * 1) crypto support is not enabled (WithCrypto==0)
   * 2) pgp: header field was present during message editing with $edit_headers (e_templ->security != 0)
   * 3) we are resending a message
   * 4) we are recalling a postponed message (don't override the user's saved settings)
   * 5) we are in batch mode
   *
   * This is done after allowing the user to edit the message so that security
   * settings can be configured with send2-hook and $edit_headers.  */
  if ((WithCrypto != 0) && (e_templ->security == 0) &&
      !(flags & (SEND_BATCH | SEND_POSTPONED | SEND_RESEND)))
  {
    if (
#ifdef USE_AUTOCRYPT
        C_Autocrypt && C_AutocryptReply
#else
        0
#endif
        && e_cur && (e_cur->security & SEC_AUTOCRYPT))
    {
      e_templ->security |= (SEC_AUTOCRYPT | SEC_AUTOCRYPT_OVERRIDE | APPLICATION_PGP);
    }
    else
    {
      if (C_CryptAutosign)
        e_templ->security |= SEC_SIGN;
      if (C_CryptAutoencrypt)
        e_templ->security |= SEC_ENCRYPT;
      if (C_CryptReplyencrypt && e_cur && (e_cur->security & SEC_ENCRYPT))
        e_templ->security |= SEC_ENCRYPT;
      if (C_CryptReplysign && e_cur && (e_cur->security & SEC_SIGN))
        e_templ->security |= SEC_SIGN;
      if (C_CryptReplysignencrypted && e_cur && (e_cur->security & SEC_ENCRYPT))
        e_templ->security |= SEC_SIGN;
      if (((WithCrypto & APPLICATION_PGP) != 0) &&
          ((e_templ->security & (SEC_ENCRYPT | SEC_SIGN)) || C_CryptOpportunisticEncrypt))
      {
        if (C_PgpAutoinline)
          e_templ->security |= SEC_INLINE;
        if (C_PgpReplyinline && e_cur && (e_cur->security & SEC_INLINE))
          e_templ->security |= SEC_INLINE;
      }
    }

    if (e_templ->security || C_CryptOpportunisticEncrypt)
    {
      /* When replying / forwarding, use the original message's
       * crypto system.  According to the documentation,
       * smime_is_default should be disregarded here.
       *
       * Problem: At least with forwarding, this doesn't really
       * make much sense. Should we have an option to completely
       * disable individual mechanisms at run-time?  */
      if (e_cur)
      {
        if (((WithCrypto & APPLICATION_PGP) != 0) && C_CryptAutopgp &&
            (e_cur->security & APPLICATION_PGP))
        {
          e_templ->security |= APPLICATION_PGP;
        }
        else if (((WithCrypto & APPLICATION_SMIME) != 0) && C_CryptAutosmime &&
                 (e_cur->security & APPLICATION_SMIME))
        {
          e_templ->security |= APPLICATION_SMIME;
        }
      }

      /* No crypto mechanism selected? Use availability + smime_is_default
       * for the decision.  */
      if (!(e_templ->security & (APPLICATION_SMIME | APPLICATION_PGP)))
      {
        if (((WithCrypto & APPLICATION_SMIME) != 0) && C_CryptAutosmime && C_SmimeIsDefault)
        {
          e_templ->security |= APPLICATION_SMIME;
        }
        else if (((WithCrypto & APPLICATION_PGP) != 0) && C_CryptAutopgp)
        {
          e_templ->security |= APPLICATION_PGP;
        }
        else if (((WithCrypto & APPLICATION_SMIME) != 0) && C_CryptAutosmime)
        {
          e_templ->security |= APPLICATION_SMIME;
        }
      }
    }

    /* opportunistic encrypt relies on SMIME or PGP already being selected */
    if (C_CryptOpportunisticEncrypt)
    {
      /* If something has already enabled encryption, e.g. C_CryptAutoencrypt
       * or C_CryptReplyencrypt, then don't enable opportunistic encrypt for
       * the message.  */
      if (!(e_templ->security & (SEC_ENCRYPT | SEC_AUTOCRYPT)))
      {
        e_templ->security |= SEC_OPPENCRYPT;
        crypt_opportunistic_encrypt(e_templ);
      }
    }

    /* No permissible mechanisms found.  Don't sign or encrypt. */
    if (!(e_templ->security & (APPLICATION_SMIME | APPLICATION_PGP)))
      e_templ->security = SEC_NO_FLAGS;
  }

  /* Deal with the corner case where the crypto module backend is not available.
   * This can happen if configured without PGP/SMIME and with GPGME, but
   * $crypt_use_gpgme is unset.  */
  if (e_templ->security && !crypt_has_module_backend(e_templ->security))
  {
    mutt_error(_(
        "No crypto backend configured.  Disabling message security setting."));
    e_templ->security = SEC_NO_FLAGS;
  }

  /* specify a default fcc.  if we are in batchmode, only save a copy of
   * the message if the value of $copy is yes or ask-yes */

  if (mutt_buffer_is_empty(&fcc) && !(flags & SEND_POSTPONED_FCC) &&
      (!(flags & SEND_BATCH) || (C_Copy & 0x1)))
  {
    /* set the default FCC */
    const bool killfrom = TAILQ_EMPTY(&e_templ->env->from);
    if (killfrom)
    {
      mutt_addrlist_append(&e_templ->env->from, mutt_default_from());
    }
    mutt_select_fcc(&fcc, e_templ);
    if (killfrom)
    {
      mutt_addrlist_clear(&e_templ->env->from);
    }
  }

  mutt_rfc3676_space_stuff(e_templ);

  mutt_update_encoding(e_templ->content);

  if (!(flags & SEND_BATCH))
  {
  main_loop:

    mutt_buffer_pretty_mailbox(&fcc);
    i = mutt_compose_menu(e_templ, &fcc, e_cur,
                          ((flags & SEND_NO_FREE_HEADER) ? MUTT_COMPOSE_NOFREEHEADER : 0));
    if (i == -1)
    {
/* abort */
#ifdef USE_NNTP
      if (flags & SEND_NEWS)
        mutt_message(_("Article not posted"));
      else
#endif
        mutt_message(_("Mail not sent"));
      goto cleanup;
    }
    else if (i == 1)
    {
      if (postpone_message(e_templ, e_cur, mutt_b2s(&fcc), flags) != 0)
        goto main_loop;
      mutt_message(_("Message postponed"));
      rc = 1;
      goto cleanup;
    }
  }

#ifdef USE_NNTP
  if (!(flags & SEND_NEWS))
#endif
    if ((mutt_addrlist_count_recips(&e_templ->env->to) == 0) &&
        (mutt_addrlist_count_recips(&e_templ->env->cc) == 0) &&
        (mutt_addrlist_count_recips(&e_templ->env->bcc) == 0))
    {
      if (flags & SEND_BATCH)
      {
        puts(_("No recipients specified"));
        goto cleanup;
      }

      mutt_error(_("No recipients specified"));
      goto main_loop;
    }

  if (mutt_env_to_intl(e_templ->env, &tag, &err))
  {
    mutt_error(_("Bad IDN in '%s': '%s'"), tag, err);
    FREE(&err);
    if (flags & SEND_BATCH)
      goto cleanup;
    goto main_loop;
  }

  if (!e_templ->env->subject && !(flags & SEND_BATCH) &&
      (query_quadoption(C_AbortNosubject, _("No subject, abort sending?")) != MUTT_NO))
  {
    /* if the abort is automatic, print an error message */
    if (C_AbortNosubject == MUTT_YES)
      mutt_error(_("No subject specified"));
    goto main_loop;
  }
#ifdef USE_NNTP
  if ((flags & SEND_NEWS) && !e_templ->env->subject)
  {
    mutt_error(_("No subject specified"));
    goto main_loop;
  }

  if ((flags & SEND_NEWS) && !e_templ->env->newsgroups)
  {
    mutt_error(_("No newsgroup specified"));
    goto main_loop;
  }
#endif

  if (!(flags & SEND_BATCH) && (C_AbortNoattach != MUTT_NO) &&
      !e_templ->content->next && (e_templ->content->type == TYPE_TEXT) &&
      mutt_istr_equal(e_templ->content->subtype, "plain") &&
      search_attach_keyword(e_templ->content->filename) &&
      (query_quadoption(C_AbortNoattach,
                        _("No attachments, cancel sending?")) != MUTT_NO))
  {
    /* if the abort is automatic, print an error message */
    if (C_AbortNoattach == MUTT_YES)
    {
      mutt_error(_("Message contains text matching "
                   "\"$abort_noattach_regex\". Not sending."));
    }
    goto main_loop;
  }

  if (e_templ->content->next)
    e_templ->content = mutt_make_multipart(e_templ->content);

  /* Ok, we need to do it this way instead of handling all fcc stuff in
   * one place in order to avoid going to main_loop with encoded "env"
   * in case of error.  Ugh.  */

  mutt_encode_descriptions(e_templ->content, true);

  /* Make sure that clear_content and free_clear_content are
   * properly initialized -- we may visit this particular place in
   * the code multiple times, including after a failed call to
   * mutt_protect().  */

  clear_content = NULL;
  free_clear_content = false;

  if (WithCrypto)
  {
    if (e_templ->security & (SEC_ENCRYPT | SEC_SIGN | SEC_AUTOCRYPT))
    {
      /* save the decrypted attachments */
      clear_content = e_templ->content;

      if ((crypt_get_keys(e_templ, &pgpkeylist, 0) == -1) ||
          (mutt_protect(e_templ, pgpkeylist, false) == -1))
      {
        e_templ->content = mutt_remove_multipart(e_templ->content);

        FREE(&pgpkeylist);

        decode_descriptions(e_templ->content);
        goto main_loop;
      }
      mutt_encode_descriptions(e_templ->content, false);
    }

    /* at this point, e_templ->content is one of the following three things:
     * - multipart/signed.     In this case, clear_content is a child
     * - multipart/encrypted.  In this case, clear_content exists independently
     * - application/pgp.      In this case, clear_content exists independently
     * - something else.       In this case, it's the same as clear_content */

    /* This is ugly -- lack of "reporting back" from mutt_protect(). */

    if (clear_content && (e_templ->content != clear_content) &&
        (e_templ->content->parts != clear_content))
      free_clear_content = true;
  }

  if (!OptNoCurses)
    mutt_message(_("Sending message..."));

  mutt_prepare_envelope(e_templ->env, true);

  if (C_FccBeforeSend)
    save_fcc(e_templ, &fcc, clear_content, pgpkeylist, flags, &finalpath);

  i = invoke_mta(e_templ);
  if (i < 0)
  {
    if (!(flags & SEND_BATCH))
    {
      if (!WithCrypto)
        ; // do nothing
      else if ((e_templ->security & (SEC_ENCRYPT | SEC_AUTOCRYPT)) ||
               ((e_templ->security & SEC_SIGN) && (e_templ->content->type == TYPE_APPLICATION)))
      {
        if (e_templ->content != clear_content)
        {
          mutt_body_free(&e_templ->content); /* destroy PGP data */
          e_templ->content = clear_content;  /* restore clear text. */
        }
      }
      else if ((e_templ->security & SEC_SIGN) && (e_templ->content->type == TYPE_MULTIPART))
      {
        mutt_body_free(&e_templ->content->parts->next); /* destroy sig */
        e_templ->content = mutt_remove_multipart(e_templ->content);
      }

      FREE(&pgpkeylist);
      mutt_env_free(&e_templ->content->mime_headers); /* protected headers */
      e_templ->content = mutt_remove_multipart(e_templ->content);
      decode_descriptions(e_templ->content);
      mutt_unprepare_envelope(e_templ->env);
      FREE(&finalpath);
      goto main_loop;
    }
    else
    {
      puts(_("Could not send the message"));
      goto cleanup;
    }
  }

  if (!C_FccBeforeSend)
    save_fcc(e_templ, &fcc, clear_content, pgpkeylist, flags, &finalpath);

  if (!OptNoCurses)
  {
    mutt_message((i != 0) ? _("Sending in background") :
                            (flags & SEND_NEWS) ? _("Article posted") : /* USE_NNTP */
                                _("Mail sent"));
#ifdef USE_NOTMUCH
    if (C_NmRecord)
      nm_record_message(ctx ? ctx->mailbox : NULL, finalpath, e_cur);
#endif
    mutt_sleep(0);
  }

  if (WithCrypto)
    FREE(&pgpkeylist);

  if ((WithCrypto != 0) && free_clear_content)
    mutt_body_free(&clear_content);

  /* set 'replied' flag only if the user didn't change/remove
   * In-Reply-To: and References: headers during edit */
  if (flags & SEND_REPLY)
  {
    if (!(flags & SEND_POSTPONED) && ctx && ctx->mailbox)
    {
      STAILQ_FOREACH(en, el, entries)
      {
        mutt_set_flag(ctx->mailbox, en->email, MUTT_REPLIED, is_reply(en->email, e_templ));
      }
    }
  }

  rc = 0;

cleanup:
  mutt_buffer_dealloc(&fcc);

  if (flags & SEND_POSTPONED)
  {
    if (WithCrypto & APPLICATION_PGP)
    {
      FREE(&C_PgpSignAs);
      C_PgpSignAs = pgp_signas;
    }
    if (WithCrypto & APPLICATION_SMIME)
    {
      FREE(&C_SmimeSignAs);
      C_SmimeSignAs = smime_signas;
    }
  }

  mutt_file_fclose(&fp_tmp);
  if (!(flags & SEND_NO_FREE_HEADER))
    email_free(&e_templ);

  FREE(&finalpath);
  return rc;
}
