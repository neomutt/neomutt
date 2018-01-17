/**
 * @file
 * Prepare and send an email
 *
 * @authors
 * Copyright (C) 1996-2002,2004,2010,2012-2013 Michael R. Elkins <me@mutt.org>
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
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/email.h"
#include "mutt.h"
#include "send.h"
#include "alias.h"
#include "compose.h"
#include "context.h"
#include "copy.h"
#include "curs_lib.h"
#include "edit.h"
#include "filter.h"
#include "globals.h"
#include "hdrline.h"
#include "hook.h"
#include "mailbox.h"
#include "mutt_attach.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "rfc3676.h"
#include "sendlib.h"
#include "smtp.h"
#include "sort.h"
#ifdef USE_NNTP
#include "mx.h"
#include "nntp/nntp.h"
#endif
#ifdef MIXMASTER
#include "remailer.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/mutt_notmuch.h"
#endif

/* These Config Variables are only used in send.c */
unsigned char AbortNoattach; /* forgotten attachment detector */
struct Regex *AbortNoattachRegex;
unsigned char AbortNosubject;
unsigned char AbortUnmodified;
bool AskFollowUp;
bool AskXCommentTo;
char *ContentType;
bool CryptAutoencrypt;
bool CryptAutopgp;
bool CryptAutosign;
bool CryptAutosmime;
bool CryptReplyencrypt;
bool CryptReplysign;
bool CryptReplysignencrypted;
char *EmptySubject;
bool FastReply;
unsigned char FccAttach;
bool FccClear;
bool FollowupTo;
char *ForwardAttributionIntro;
char *ForwardAttributionTrailer;
unsigned char ForwardEdit;
char *ForwardFormat;
bool ForwardReferences;
bool Hdrs;
unsigned char HonorFollowupTo;
bool IgnoreListReplyTo;
unsigned char Include;
bool Metoo;
bool NmRecord;
bool PgpReplyinline;
char *PostIndentString;
bool PostponeEncrypt;
char *PostponeEncryptAs;
unsigned char Recall;
bool ReplySelf;
unsigned char ReplyTo;
bool ReplyWithXorig;
bool ReverseName;
bool ReverseRealname;
bool SigDashes;
char *Signature;
bool SigOnTop;
bool UseFrom;

/**
 * append_signature - Append a signature to an email
 * @param f File to write to
 */
static void append_signature(FILE *f)
{
  FILE *tmpfp = NULL;
  pid_t thepid;

  if (Signature && (tmpfp = mutt_open_read(Signature, &thepid)))
  {
    if (SigDashes)
      fputs("\n-- \n", f);
    mutt_file_copy_stream(tmpfp, f);
    mutt_file_fclose(&tmpfp);
    if (thepid != -1)
      mutt_wait_filter(thepid);
  }
}

/**
 * mutt_remove_xrefs - Remove cross-references
 * @param a Reference list of Addresses
 * @param b Address list to trim
 * @retval ptr Updated Address list
 *
 * Remove addresses from "b" which are contained in "a"
 */
struct Address *mutt_remove_xrefs(struct Address *a, struct Address *b)
{
  struct Address *p = NULL, *prev = NULL;

  struct Address *top = b;
  while (b)
  {
    for (p = a; p; p = p->next)
    {
      if (mutt_addr_cmp(p, b))
        break;
    }
    if (p)
    {
      if (prev)
      {
        prev->next = b->next;
        b->next = NULL;
        mutt_addr_free(&b);
        b = prev;
      }
      else
      {
        top = top->next;
        b->next = NULL;
        mutt_addr_free(&b);
        b = top;
      }
    }
    else
    {
      prev = b;
      b = b->next;
    }
  }
  return top;
}

/**
 * remove_user - Remove any address which matches the current user
 * @param a          List of addresses
 * @param leave_only If set, don't remove the user's address if it it the only
 *                   one in the list
 * @retval ptr Head of the remaining Address List
 */
static struct Address *remove_user(struct Address *a, int leave_only)
{
  struct Address *top = NULL, *last = NULL;

  while (a)
  {
    if (!mutt_addr_is_user(a))
    {
      if (top)
      {
        last->next = a;
        last = last->next;
      }
      else
        last = top = a;
      a = a->next;
      last->next = NULL;
    }
    else
    {
      struct Address *tmp = a;

      a = a->next;
      if (!leave_only || a || last)
      {
        tmp->next = NULL;
        mutt_addr_free(&tmp);
      }
      else
        last = top = tmp;
    }
  }
  return top;
}

/**
 * find_mailing_lists - Search Address lists for mailing lists
 * @param t 'To' Address list
 * @param c 'Cc' Address list
 * @retval ptr Newly allocated Address list of all mailing list matches
 */
static struct Address *find_mailing_lists(struct Address *t, struct Address *c)
{
  struct Address *top = NULL, *ptr = NULL;

  for (; t || c; t = c, c = NULL)
  {
    for (; t; t = t->next)
    {
      if (mutt_is_mail_list(t) && !t->group)
      {
        if (top)
        {
          ptr->next = mutt_addr_copy(t);
          ptr = ptr->next;
        }
        else
          ptr = top = mutt_addr_copy(t);
      }
    }
  }
  return top;
}

/**
 * edit_address - Edit an email address
 * @param a     Address to edit
 * @param field Prompt for user
 * @retval  0 Success
 * @retval -1 Failure
 */
static int edit_address(struct Address **a, /* const */ char *field)
{
  char buf[HUGE_STRING];
  char *err = NULL;
  int idna_ok = 0;

  do
  {
    buf[0] = 0;
    mutt_addrlist_to_local(*a);
    mutt_addr_write(buf, sizeof(buf), *a, false);
    if (mutt_get_field(field, buf, sizeof(buf), MUTT_ALIAS) != 0)
      return -1;
    mutt_addr_free(a);
    *a = mutt_expand_aliases(mutt_addr_parse_list2(NULL, buf));
    idna_ok = mutt_addrlist_to_intl(*a, &err);
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
 * @param flags Flags, e.g. #SEND_LIST_REPLY
 * @retval  0 Success
 * @retval -1 Failure
 */
static int edit_envelope(struct Envelope *en, int flags)
{
  char buf[HUGE_STRING];

#ifdef USE_NNTP
  if (OptNewsSend)
  {
    if (en->newsgroups)
      mutt_str_strfcpy(buf, en->newsgroups, sizeof(buf));
    else
      buf[0] = 0;
    if (mutt_get_field("Newsgroups: ", buf, sizeof(buf), 0) != 0)
      return -1;
    FREE(&en->newsgroups);
    en->newsgroups = mutt_str_strdup(buf);

    if (en->followup_to)
      mutt_str_strfcpy(buf, en->followup_to, sizeof(buf));
    else
      buf[0] = 0;
    if (AskFollowUp && mutt_get_field("Followup-To: ", buf, sizeof(buf), 0) != 0)
    {
      return -1;
    }
    FREE(&en->followup_to);
    en->followup_to = mutt_str_strdup(buf);

    if (en->x_comment_to)
      mutt_str_strfcpy(buf, en->x_comment_to, sizeof(buf));
    else
      buf[0] = 0;
    if (XCommentTo && AskXCommentTo &&
        mutt_get_field("X-Comment-To: ", buf, sizeof(buf), 0) != 0)
    {
      return -1;
    }
    FREE(&en->x_comment_to);
    en->x_comment_to = mutt_str_strdup(buf);
  }
  else
#endif
  {
    if (edit_address(&en->to, _("To: ")) == -1 || en->to == NULL)
      return -1;
    if (Askcc && edit_address(&en->cc, _("Cc: ")) == -1)
      return -1;
    if (Askbcc && edit_address(&en->bcc, _("Bcc: ")) == -1)
      return -1;
    if (ReplyWithXorig && (flags & (SEND_REPLY | SEND_LIST_REPLY | SEND_GROUP_REPLY)) &&
        (edit_address(&en->from, "From: ") == -1))
    {
      return -1;
    }
  }

  if (en->subject)
  {
    if (FastReply)
      return 0;
    else
      mutt_str_strfcpy(buf, en->subject, sizeof(buf));
  }
  else
  {
    const char *p = NULL;

    buf[0] = 0;
    struct ListNode *uh = NULL;
    STAILQ_FOREACH(uh, &UserHeader, entries)
    {
      if (mutt_str_strncasecmp("subject:", uh->data, 8) == 0)
      {
        p = mutt_str_skip_email_wsp(uh->data + 8);
        mutt_str_strfcpy(buf, p, sizeof(buf));
      }
    }
  }

  if (mutt_get_field(_("Subject: "), buf, sizeof(buf), 0) != 0 ||
      (!buf[0] && query_quadoption(AbortNosubject, _("No subject, abort?")) != MUTT_NO))
  {
    mutt_message(_("No subject, aborting."));
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
  return mutt_str_strdup(s);
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
    if (mutt_str_strncasecmp("to:", uh->data, 3) == 0)
      env->to = mutt_addr_parse_list(env->to, uh->data + 3);
    else if (mutt_str_strncasecmp("cc:", uh->data, 3) == 0)
      env->cc = mutt_addr_parse_list(env->cc, uh->data + 3);
    else if (mutt_str_strncasecmp("bcc:", uh->data, 4) == 0)
      env->bcc = mutt_addr_parse_list(env->bcc, uh->data + 4);
#ifdef USE_NNTP
    else if (mutt_str_strncasecmp("newsgroups:", uh->data, 11) == 0)
      env->newsgroups = nntp_get_header(uh->data + 11);
    else if (mutt_str_strncasecmp("followup-to:", uh->data, 12) == 0)
      env->followup_to = nntp_get_header(uh->data + 12);
    else if (mutt_str_strncasecmp("x-comment-to:", uh->data, 13) == 0)
      env->x_comment_to = nntp_get_header(uh->data + 13);
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
    if (mutt_str_strncasecmp("from:", uh->data, 5) == 0)
    {
      /* User has specified a default From: address.  Remove default address */
      mutt_addr_free(&env->from);
      env->from = mutt_addr_parse_list(env->from, uh->data + 5);
    }
    else if (mutt_str_strncasecmp("reply-to:", uh->data, 9) == 0)
    {
      mutt_addr_free(&env->reply_to);
      env->reply_to = mutt_addr_parse_list(env->reply_to, uh->data + 9);
    }
    else if (mutt_str_strncasecmp("message-id:", uh->data, 11) == 0)
    {
      char *tmp = mutt_extract_message_id(uh->data + 11, NULL);
      if (mutt_addr_valid_msgid(tmp))
      {
        FREE(&env->message_id);
        env->message_id = tmp;
      }
      else
        FREE(&tmp);
    }
    else if ((mutt_str_strncasecmp("to:", uh->data, 3) != 0) &&
             (mutt_str_strncasecmp("cc:", uh->data, 3) != 0) &&
             (mutt_str_strncasecmp("bcc:", uh->data, 4) != 0) &&
#ifdef USE_NNTP
             (mutt_str_strncasecmp("newsgroups:", uh->data, 11) != 0) &&
             (mutt_str_strncasecmp("followup-to:", uh->data, 12) != 0) &&
             (mutt_str_strncasecmp("x-comment-to:", uh->data, 13) != 0) &&
#endif
             (mutt_str_strncasecmp("supersedes:", uh->data, 11) != 0) &&
             (mutt_str_strncasecmp("subject:", uh->data, 8) != 0) &&
             (mutt_str_strncasecmp("return-path:", uh->data, 12) != 0))
    {
      mutt_list_insert_tail(&env->userhdrs, mutt_str_strdup(uh->data));
    }
  }
}

/**
 * mutt_forward_intro - Add the "start of forwarded message" text
 * @param ctx Mailbox
 * @param cur Header of email
 * @param fp  File to write to
 */
void mutt_forward_intro(struct Context *ctx, struct Header *cur, FILE *fp)
{
  if (!ForwardAttributionIntro || !fp)
    return;

  char buffer[LONG_STRING];
  setlocale(LC_TIME, NONULL(AttributionLocale));
  mutt_make_string(buffer, sizeof(buffer), ForwardAttributionIntro, ctx, cur);
  setlocale(LC_TIME, "");
  fputs(buffer, fp);
  fputs("\n\n", fp);
}

/**
 * mutt_forward_trailer - Add a "end of forwarded message" text
 * @param ctx Mailbox
 * @param cur Header of email
 * @param fp  File to write to
 */
void mutt_forward_trailer(struct Context *ctx, struct Header *cur, FILE *fp)
{
  if (!ForwardAttributionTrailer || !fp)
    return;

  char buffer[LONG_STRING];
  setlocale(LC_TIME, NONULL(AttributionLocale));
  mutt_make_string(buffer, sizeof(buffer), ForwardAttributionTrailer, ctx, cur);
  setlocale(LC_TIME, "");
  fputc('\n', fp);
  fputs(buffer, fp);
  fputc('\n', fp);
}

/**
 * include_forward - Write out a forwarded message
 * @param ctx Mailbox
 * @param cur Header of email
 * @param out File to write to
 * @retval  0 Success
 * @retval -1 Failure
 */
static int include_forward(struct Context *ctx, struct Header *cur, FILE *out)
{
  int chflags = CH_DECODE, cmflags = 0;

  mutt_parse_mime_message(ctx, cur);
  mutt_message_hook(ctx, cur, MUTT_MESSAGE_HOOK);

  if ((WithCrypto != 0) && (cur->security & ENCRYPT) && ForwardDecode)
  {
    /* make sure we have the user's passphrase before proceeding... */
    if (!crypt_valid_passphrase(cur->security))
      return -1;
  }

  mutt_forward_intro(ctx, cur, out);

  if (ForwardDecode)
  {
    cmflags |= MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    if (Weed)
    {
      chflags |= CH_WEED | CH_REORDER;
      cmflags |= MUTT_CM_WEED;
    }
  }
  if (ForwardQuote)
    cmflags |= MUTT_CM_PREFIX;

  /* wrapping headers for forwarding is considered a display
   * rather than send action */
  chflags |= CH_DISPLAY;

  mutt_copy_message_ctx(out, ctx, cur, cmflags, chflags);
  mutt_forward_trailer(ctx, cur, out);
  return 0;
}

/**
 * mutt_make_attribution - Add "on DATE, PERSON wrote" header
 * @param ctx Mailbox
 * @param cur Header of email
 * @param out File to write to
 */
void mutt_make_attribution(struct Context *ctx, struct Header *cur, FILE *out)
{
  if (!Attribution || !out)
    return;

  char buffer[LONG_STRING];
  setlocale(LC_TIME, NONULL(AttributionLocale));
  mutt_make_string(buffer, sizeof(buffer), Attribution, ctx, cur);
  setlocale(LC_TIME, "");
  fputs(buffer, out);
  fputc('\n', out);
}

/**
 * mutt_make_post_indent - Add suffix to replied email text
 * @param ctx Mailbox
 * @param cur Header of email
 * @param out File to write to
 */
void mutt_make_post_indent(struct Context *ctx, struct Header *cur, FILE *out)
{
  if (!PostIndentString || !out)
    return;

  char buffer[STRING];
  mutt_make_string(buffer, sizeof(buffer), PostIndentString, ctx, cur);
  fputs(buffer, out);
  fputc('\n', out);
}

/**
 * include_reply - Generate the reply text for an email
 * @param ctx Mailbox
 * @param cur Header of email
 * @param out File to write to
 * @retval  0 Success
 * @retval -1 Failure
 */
static int include_reply(struct Context *ctx, struct Header *cur, FILE *out)
{
  int cmflags = MUTT_CM_PREFIX | MUTT_CM_DECODE | MUTT_CM_CHARCONV | MUTT_CM_REPLYING;
  int chflags = CH_DECODE;

  if ((WithCrypto != 0) && (cur->security & ENCRYPT))
  {
    /* make sure we have the user's passphrase before proceeding... */
    if (!crypt_valid_passphrase(cur->security))
      return -1;
  }

  mutt_parse_mime_message(ctx, cur);
  mutt_message_hook(ctx, cur, MUTT_MESSAGE_HOOK);

  mutt_make_attribution(ctx, cur, out);

  if (!Header)
    cmflags |= MUTT_CM_NOHEADER;
  if (Weed)
  {
    chflags |= CH_WEED | CH_REORDER;
    cmflags |= MUTT_CM_WEED;
  }

  mutt_copy_message_ctx(out, ctx, cur, cmflags, chflags);

  mutt_make_post_indent(ctx, cur, out);

  return 0;
}

/**
 * default_to - Generate default email addresses
 * @param to      'To' address
 * @param env     Envelope to populate
 * @param flags   Flags, e.g. #SEND_LIST_REPLY
 * @param hmfupto If true, add 'followup-to' address to 'to' address
 * @retval  0 Success
 * @retval -1 Aborted
 */
static int default_to(struct Address **to, struct Envelope *env, int flags, int hmfupto)
{
  char prompt[STRING];

  if (flags && env->mail_followup_to && hmfupto == MUTT_YES)
  {
    mutt_addr_append(to, env->mail_followup_to, true);
    return 0;
  }

  /* Exit now if we're setting up the default Cc list for list-reply
   * (only set if Mail-Followup-To is present and honoured).
   */
  if (flags & SEND_LIST_REPLY)
    return 0;

  if (!ReplySelf && mutt_addr_is_user(env->from))
  {
    /* mail is from the user, assume replying to recipients */
    mutt_addr_append(to, env->to, true);
  }
  else if (env->reply_to)
  {
    if ((mutt_addr_cmp(env->from, env->reply_to) && !env->reply_to->next &&
         !env->reply_to->personal) ||
        (IgnoreListReplyTo && mutt_is_mail_list(env->reply_to) &&
         (mutt_addr_search(env->reply_to, env->to) ||
          mutt_addr_search(env->reply_to, env->cc))))
    {
      /* If the Reply-To: address is a mailing list, assume that it was
       * put there by the mailing list, and use the From: address
       *
       * We also take the from header if our correspondent has a reply-to
       * header which is identical to the electronic mail address given
       * in his From header, and the reply-to has no display-name.
       *
       */
      mutt_addr_append(to, env->from, false);
    }
    else if (!(mutt_addr_cmp(env->from, env->reply_to) && !env->reply_to->next) &&
             ReplyTo != MUTT_YES)
    {
      /* There are quite a few mailing lists which set the Reply-To:
       * header field to the list address, which makes it quite impossible
       * to send a message to only the sender of the message.  This
       * provides a way to do that.
       */
      /* L10N:
         Asks whether the user respects the reply-to header.
         If she says no, neomutt will reply to the from header's address instead. */
      snprintf(prompt, sizeof(prompt), _("Reply to %s%s?"),
               env->reply_to->mailbox, env->reply_to->next ? ",..." : "");
      switch (query_quadoption(ReplyTo, prompt))
      {
        case MUTT_YES:
          mutt_addr_append(to, env->reply_to, false);
          break;

        case MUTT_NO:
          mutt_addr_append(to, env->from, false);
          break;

        default:
          return -1; /* abort */
      }
    }
    else
      mutt_addr_append(to, env->reply_to, false);
  }
  else
    mutt_addr_append(to, env->from, false);

  return 0;
}

/**
 * mutt_fetch_recips - Generate recpients for a reply email
 * @param out   Envelope to populate
 * @param in    Envelope of source email
 * @param flags Flags, e.g. SEND_LIST_REPLY
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_fetch_recips(struct Envelope *out, struct Envelope *in, int flags)
{
  struct Address *tmp = NULL;
  int hmfupto = -1;

  if ((flags & (SEND_LIST_REPLY | SEND_GROUP_REPLY)) && in->mail_followup_to)
  {
    char prompt[STRING];
    snprintf(prompt, sizeof(prompt), _("Follow-up to %s%s?"),
             in->mail_followup_to->mailbox, in->mail_followup_to->next ? ",..." : "");

    hmfupto = query_quadoption(HonorFollowupTo, prompt);
    if (hmfupto == MUTT_ABORT)
      return -1;
  }

  if (flags & SEND_LIST_REPLY)
  {
    tmp = find_mailing_lists(in->to, in->cc);
    mutt_addr_append(&out->to, tmp, false);
    mutt_addr_free(&tmp);

    if (in->mail_followup_to && hmfupto == MUTT_YES &&
        default_to(&out->cc, in, flags & SEND_LIST_REPLY, hmfupto) == MUTT_ABORT)
    {
      return -1; /* abort */
    }
  }
  else
  {
    if (default_to(&out->to, in, flags & SEND_GROUP_REPLY, hmfupto) == MUTT_ABORT)
      return -1; /* abort */

    if ((flags & SEND_GROUP_REPLY) && (!in->mail_followup_to || hmfupto != MUTT_YES))
    {
      /* if(!mutt_addr_is_user(in->to)) */
      mutt_addr_append(&out->cc, in->to, true);
      mutt_addr_append(&out->cc, in->cc, true);
    }
  }
  return 0;
}

/**
 * add_references - Add the email's references to a list
 * @param head List of references
 * @param e    Envelope of message
 */
static void add_references(struct ListHead *head, struct Envelope *e)
{
  struct ListHead *src = NULL;
  struct ListNode *np = NULL;

  src = !STAILQ_EMPTY(&e->references) ? &e->references : &e->in_reply_to;
  STAILQ_FOREACH(np, src, entries)
  {
    mutt_list_insert_tail(head, mutt_str_strdup(np->data));
  }
}

/**
 * add_message_id - Add the email's message ID to a list
 * @param head List of message IDs
 * @param e    Envelope of message
 */
static void add_message_id(struct ListHead *head, struct Envelope *e)
{
  if (e->message_id)
  {
    mutt_list_insert_head(head, mutt_str_strdup(e->message_id));
  }
}

/**
 * mutt_fix_reply_recipients - Remove duplicate recipients
 * @param env Envelope to fix
 */
void mutt_fix_reply_recipients(struct Envelope *env)
{
  if (!Metoo)
  {
    /* the order is important here.  do the CC: first so that if the
     * the user is the only recipient, it ends up on the TO: field
     */
    env->cc = remove_user(env->cc, (env->to == NULL));
    env->to = remove_user(env->to, (env->cc == NULL) || ReplySelf);
  }

  /* the CC field can get cluttered, especially with lists */
  env->to = mutt_remove_duplicates(env->to);
  env->cc = mutt_remove_duplicates(env->cc);
  env->cc = mutt_remove_xrefs(env->to, env->cc);

  if (env->cc && !env->to)
  {
    env->to = env->cc;
    env->cc = NULL;
  }
}

/**
 * mutt_make_forward_subject - Create a subject for a forwarded email
 * @param env Envelope for result
 * @param ctx Mailbox
 * @param cur Header of email
 */
void mutt_make_forward_subject(struct Envelope *env, struct Context *ctx, struct Header *cur)
{
  if (!env)
    return;

  char buffer[STRING];

  /* set the default subject for the message. */
  mutt_make_string(buffer, sizeof(buffer), NONULL(ForwardFormat), ctx, cur);
  mutt_str_replace(&env->subject, buffer);
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
   * been taken from a List-Post header.  Is that correct?
   */
  if (curenv->real_subj)
  {
    FREE(&env->subject);
    env->subject = mutt_mem_malloc(mutt_str_strlen(curenv->real_subj) + 5);
    sprintf(env->subject, "Re: %s", curenv->real_subj);
  }
  else if (!env->subject)
    env->subject = mutt_str_strdup(EmptySubject);
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
  if (OptNewsSend && XCommentTo && curenv->from)
    env->x_comment_to = mutt_str_strdup(mutt_get_name(curenv->from));
#endif
}

/**
 * make_reference_headers - Generate reference headers for an email
 * @param curenv Envelope of source email
 * @param env    Envelope for result
 * @param ctx    Mailbox
 */
static void make_reference_headers(struct Envelope *curenv,
                                   struct Envelope *env, struct Context *ctx)
{
  if (!env || !ctx)
    return;

  if (!curenv)
  {
    for (int i = 0; i < ctx->msgcount; i++)
    {
      if (message_is_tagged(ctx, i))
        mutt_add_to_reference_headers(env, ctx->hdrs[i]->env);
    }
  }
  else
    mutt_add_to_reference_headers(env, curenv);

  /* if there's more than entry in In-Reply-To (i.e. message has
     multiple parents), don't generate a References: header as it's
     discouraged by RFC2822, sect. 3.6.4 */
  if (ctx->tagged > 0 && !STAILQ_EMPTY(&env->in_reply_to) &&
      STAILQ_NEXT(STAILQ_FIRST(&env->in_reply_to), entries))
  {
    mutt_list_free(&env->references);
  }
}

/**
 * envelope_defaults - Fill in some defaults for a new email
 * @param env   Envelope for result
 * @param ctx   Mailbox
 * @param cur   Header of email
 * @param flags Flags, e.g. #SEND_REPLY
 * @retval  0 Success
 * @retval -1 Failure
 */
static int envelope_defaults(struct Envelope *env, struct Context *ctx,
                             struct Header *cur, int flags)
{
  struct Envelope *curenv = NULL;
  bool tag = false;

  if (!cur)
  {
    tag = true;
    for (int i = 0; i < ctx->msgcount; i++)
    {
      if (!message_is_tagged(ctx, i))
        continue;

      cur = ctx->hdrs[i];
      curenv = cur->env;
      break;
    }

    if (!cur)
    {
      /* This could happen if the user tagged some messages and then did
       * a limit such that none of the tagged message are visible.
       */
      mutt_error(_("No tagged messages are visible!"));
      return -1;
    }
  }
  else
    curenv = cur->env;

  if (!curenv)
    return -1;

  if (flags & SEND_REPLY)
  {
#ifdef USE_NNTP
    if ((flags & SEND_NEWS))
    {
      /* in case followup set Newsgroups: with Followup-To: if it present */
      if (!env->newsgroups &&
          (mutt_str_strcasecmp(curenv->followup_to, "poster") != 0))
      {
        env->newsgroups = mutt_str_strdup(curenv->followup_to);
      }
    }
    else
#endif
        if (tag)
    {
      for (int i = 0; i < ctx->msgcount; i++)
      {
        if (!message_is_tagged(ctx, i))
          continue;

        if (mutt_fetch_recips(env, ctx->hdrs[i]->env, flags) == -1)
          return -1;
      }
    }
    else if (mutt_fetch_recips(env, curenv, flags) == -1)
      return -1;

    if ((flags & SEND_LIST_REPLY) && !env->to)
    {
      mutt_error(_("No mailing lists found!"));
      return -1;
    }

    mutt_make_misc_reply_headers(env, curenv);
    make_reference_headers(tag ? NULL : curenv, env, ctx);
  }
  else if (flags & SEND_FORWARD)
  {
    mutt_make_forward_subject(env, ctx, cur);
    if (ForwardReferences)
      make_reference_headers(tag ? NULL : curenv, env, ctx);
  }

  return 0;
}

/**
 * generate_body - Create a new email body
 * @param tempfp stream for outgoing message
 * @param msg    header for outgoing message
 * @param flags  compose mode
 * @param ctx    current mailbox
 * @param cur    current message
 * @retval  0 Success
 * @retval -1 Error
 */
static int generate_body(FILE *tempfp, struct Header *msg, int flags,
                         struct Context *ctx, struct Header *cur)
{
  int i;
  struct Body *tmp = NULL;

  if (flags & SEND_REPLY)
  {
    i = query_quadoption(Include, _("Include message in reply?"));
    if (i == MUTT_ABORT)
      return -1;

    if (i == MUTT_YES)
    {
      mutt_message(_("Including quoted message..."));
      if (!cur)
      {
        for (i = 0; i < ctx->msgcount; i++)
        {
          if (!message_is_tagged(ctx, i))
            continue;

          if (include_reply(ctx, ctx->hdrs[i], tempfp) == -1)
          {
            mutt_error(_("Could not include all requested messages!"));
            return -1;
          }
          fputc('\n', tempfp);
        }
      }
      else
        include_reply(ctx, cur, tempfp);
    }
  }
  else if (flags & SEND_FORWARD)
  {
    i = query_quadoption(MimeForward, _("Forward as attachment?"));
    if (i == MUTT_YES)
    {
      struct Body *last = msg->content;

      mutt_message(_("Preparing forwarded message..."));

      while (last && last->next)
        last = last->next;

      if (cur)
      {
        tmp = mutt_make_message_attach(ctx, cur, false);
        if (last)
          last->next = tmp;
        else
          msg->content = tmp;
      }
      else
      {
        for (i = 0; i < ctx->msgcount; i++)
        {
          if (!message_is_tagged(ctx, i))
            continue;

          tmp = mutt_make_message_attach(ctx, ctx->hdrs[i], false);
          if (last)
          {
            last->next = tmp;
            last = tmp;
          }
          else
            last = msg->content = tmp;
        }
      }
    }
    else if (i != -1)
    {
      if (cur)
        include_forward(ctx, cur, tempfp);
      else
      {
        for (i = 0; i < ctx->msgcount; i++)
        {
          if (message_is_tagged(ctx, i))
            include_forward(ctx, ctx->hdrs[i], tempfp);
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

    if (((WithCrypto & APPLICATION_PGP) != 0) && (b = crypt_pgp_make_key_attachment()) == NULL)
    {
      return -1;
    }

    b->next = msg->content;
    msg->content = b;
  }

  mutt_clear_error();

  return 0;
}

/**
 * mutt_set_followup_to - Set followup-to field
 * @param e Envelope to modify
 */
void mutt_set_followup_to(struct Envelope *e)
{
  struct Address *t = NULL;
  struct Address *from = NULL;

  /* Only generate the Mail-Followup-To if the user has requested it, and
   * it hasn't already been set
   */

  if (!FollowupTo)
    return;
#ifdef USE_NNTP
  if (OptNewsSend)
  {
    if (!e->followup_to && e->newsgroups && (strrchr(e->newsgroups, ',')))
      e->followup_to = mutt_str_strdup(e->newsgroups);
    return;
  }
#endif

  if (!e->mail_followup_to)
  {
    if (mutt_is_list_cc(0, e->to, e->cc))
    {
      /* this message goes to known mailing lists, so create a proper
       * mail-followup-to header
       */

      t = mutt_addr_append(&e->mail_followup_to, e->to, false);
      mutt_addr_append(&t, e->cc, true);
    }

    /* remove ourselves from the mail-followup-to header */
    e->mail_followup_to = remove_user(e->mail_followup_to, 0);

    /* If we are not subscribed to any of the lists in question,
     * re-add ourselves to the mail-followup-to header.  The
     * mail-followup-to header generated is a no-op with group-reply,
     * but makes sure list-reply has the desired effect.
     */

    if (e->mail_followup_to && !mutt_is_list_recipient(false, e->to, e->cc))
    {
      if (e->reply_to)
        from = mutt_addr_copy_list(e->reply_to, false);
      else if (e->from)
        from = mutt_addr_copy_list(e->from, false);
      else
        from = mutt_default_from();

      if (from)
      {
        /* Normally, this loop will not even be entered. */
        for (t = from; t && t->next; t = t->next)
          ;

        t->next = e->mail_followup_to; /* t cannot be NULL at this point. */
        e->mail_followup_to = from;
      }
    }

    e->mail_followup_to = mutt_remove_duplicates(e->mail_followup_to);
  }
}

/**
 * set_reverse_name - Try to set the 'from' field from the recipients
 * @param env Envelope to use
 * @retval ptr  Newly allocated Address
 * @retval NULL A suitable Address wasn't found
 *
 * Look through the recipients of the message we are replying to, and if we
 * find an address that matches $alternates, we use that as the default from
 * field
 */
static struct Address *set_reverse_name(struct Envelope *env)
{
  struct Address *tmp = NULL;

  for (tmp = env->to; tmp; tmp = tmp->next)
  {
    if (mutt_addr_is_user(tmp))
      break;
  }
  if (!tmp)
  {
    for (tmp = env->cc; tmp; tmp = tmp->next)
    {
      if (mutt_addr_is_user(tmp))
        break;
    }
  }
  if (!tmp && mutt_addr_is_user(env->from))
    tmp = env->from;
  if (tmp)
  {
    tmp = mutt_addr_copy(tmp);
    /* when $reverse_realname is not set, clear the personal name so that it
     * may be set via a reply- or send-hook.
     */
    if (!ReverseRealname)
      FREE(&tmp->personal);
  }
  return tmp;
}

/**
 * mutt_default_from - Get a default 'from' Address
 * @retval ptr Newly allocated Address
 */
struct Address *mutt_default_from(void)
{
  struct Address *addr = NULL;

  /* Note: We let $from override $realname here.
   *       Is this the right thing to do?
   */

  if (From)
    addr = mutt_addr_copy(From);
  else
  {
    addr = mutt_addr_new();
    if (UseDomain)
    {
      const char *fqdn = mutt_fqdn(true);
      addr->mailbox =
          mutt_mem_malloc(mutt_str_strlen(Username) + mutt_str_strlen(fqdn) + 2);
      sprintf(addr->mailbox, "%s@%s", NONULL(Username), NONULL(fqdn));
    }
    else
    {
      addr->mailbox = mutt_str_strdup(NONULL(Username));
    }
  }

  return addr;
}

/**
 * send_message - Send an email
 * @param msg Header of the email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int send_message(struct Header *msg)
{
  char tempfile[PATH_MAX];
  int i;
#ifdef USE_SMTP
  short old_write_bcc;
#endif

  /* Write out the message in MIME form. */
  mutt_mktemp(tempfile, sizeof(tempfile));
  FILE *tempfp = mutt_file_fopen(tempfile, "w");
  if (!tempfp)
    return -1;

#ifdef USE_SMTP
  old_write_bcc = WriteBcc;
  if (SmtpUrl)
    WriteBcc = false;
#endif
#ifdef MIXMASTER
  mutt_rfc822_write_header(tempfp, msg->env, msg->content, 0, !STAILQ_EMPTY(&msg->chain));
#endif
#ifndef MIXMASTER
  mutt_rfc822_write_header(tempfp, msg->env, msg->content, 0, false);
#endif
#ifdef USE_SMTP
  if (old_write_bcc)
    WriteBcc = true;
#endif

  fputc('\n', tempfp); /* tie off the header. */

  if ((mutt_write_mime_body(msg->content, tempfp) == -1))
  {
    mutt_file_fclose(&tempfp);
    unlink(tempfile);
    return -1;
  }

  if (fclose(tempfp) != 0)
  {
    mutt_perror(tempfile);
    unlink(tempfile);
    return -1;
  }

#ifdef MIXMASTER
  if (!STAILQ_EMPTY(&msg->chain))
    return mix_send_message(&msg->chain, tempfile);
#endif

#ifdef USE_SMTP
#ifdef USE_NNTP
  if (!OptNewsSend)
#endif
    if (SmtpUrl)
    {
      return mutt_smtp_send(msg->env->from, msg->env->to, msg->env->cc, msg->env->bcc,
                            tempfile, (msg->content->encoding == ENC_8BIT));
    }
#endif /* USE_SMTP */

  i = mutt_invoke_sendmail(msg->env->from, msg->env->to, msg->env->cc, msg->env->bcc,
                           tempfile, (msg->content->encoding == ENC_8BIT));
  return i;
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
      rfc2047_encode(&t->description, NULL, sizeof("Content-Description:"), SendCharset);
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
 * mutt_compose_to_sender - Compose an email to the sender
 * @param hdr Header of original email
 * @retval  0 Message was successfully sent
 * @retval -1 Message was aborted or an error occurred
 * @retval  1 Message was postponed
 */
int mutt_compose_to_sender(struct Header *hdr)
{
  struct Header *msg = mutt_header_new();

  msg->env = mutt_env_new();
  if (!hdr)
  {
    for (int i = 0; i < Context->msgcount; i++)
    {
      if (message_is_tagged(Context, i))
        mutt_addr_append(&msg->env->to, Context->hdrs[i]->env->from, false);
    }
  }
  else
    msg->env->to = mutt_addr_copy_list(hdr->env->from, false);

  return ci_send_message(0, msg, NULL, NULL, NULL);
}

/**
 * mutt_resend_message - Resend an email
 * @param fp  File containing email
 * @param ctx Mailbox
 * @param cur Header of email to resend
 * @retval  0 Message was successfully sent
 * @retval -1 Message was aborted or an error occurred
 * @retval  1 Message was postponed
 */
int mutt_resend_message(FILE *fp, struct Context *ctx, struct Header *cur)
{
  struct Header *msg = mutt_header_new();

  if (mutt_prepare_template(fp, ctx, msg, cur, true) < 0)
  {
    mutt_header_free(&msg);
    return -1;
  }

  if (WithCrypto)
  {
    /* mutt_prepare_template doesn't always flip on an application bit.
     * so fix that here */
    if (!(msg->security & (APPLICATION_SMIME | APPLICATION_PGP)))
    {
      if (((WithCrypto & APPLICATION_SMIME) != 0) && SmimeIsDefault)
        msg->security |= APPLICATION_SMIME;
      else if (WithCrypto & APPLICATION_PGP)
        msg->security |= APPLICATION_PGP;
      else
        msg->security |= APPLICATION_SMIME;
    }

    if (CryptOpportunisticEncrypt)
    {
      msg->security |= OPPENCRYPT;
      crypt_opportunistic_encrypt(msg);
    }
  }

  return ci_send_message(SEND_RESEND, msg, NULL, ctx, cur);
}

/**
 * is_reply - Is one email a reply to another?
 * @param reply Header of email to test
 * @param orig  Header of original email
 * @retval 1 It is a reply
 * @retval 0 It is not a reply
 */
static int is_reply(struct Header *reply, struct Header *orig)
{
  if (!reply || !reply->env || !orig || !orig->env)
    return 0;
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
  /* Search for the regex in AbortNoattachRegex within a file */
  if (!AbortNoattachRegex || !AbortNoattachRegex->regex || !QuoteRegex ||
      !QuoteRegex->regex)
  {
    return false;
  }

  FILE *attf = mutt_file_fopen(filename, "r");
  if (!attf)
    return false;

  char *inputline = mutt_mem_malloc(LONG_STRING);
  bool found = false;
  while (!feof(attf))
  {
    fgets(inputline, LONG_STRING, attf);
    if (regexec(QuoteRegex->regex, inputline, 0, NULL, 0) != 0 &&
        regexec(AbortNoattachRegex->regex, inputline, 0, NULL, 0) == 0)
    {
      found = true;
      break;
    }
  }
  FREE(&inputline);
  mutt_file_fclose(&attf);
  return found;
}

/**
 * ci_send_message - Send an email
 * @param flags    send mode, e.g. #SEND_RESEND
 * @param msg      template to use for new message
 * @param tempfile file specified by -i or -H
 * @param ctx      current mailbox
 * @param cur      current message
 * @retval  0 Message was successfully sent
 * @retval -1 Message was aborted or an error occurred
 * @retval  1 Message was postponed
 */
int ci_send_message(int flags, struct Header *msg, char *tempfile,
                    struct Context *ctx, struct Header *cur)
{
  char buffer[LONG_STRING];
  char fcc[PATH_MAX] = ""; /* where to copy this message */
  FILE *tempfp = NULL;
  struct Body *pbody = NULL;
  int i;
  bool killfrom = false;
  bool fcc_error = false;
  bool free_clear_content = false;

  struct Body *save_content = NULL;
  struct Body *clear_content = NULL;
  char *pgpkeylist = NULL;
  /* save current value of "pgp_sign_as"  and "smime_default_key" */
  char *pgp_signas = NULL;
  char *smime_signas = NULL;
  char *tag = NULL, *err = NULL;
  char *ctype = NULL;
  char *finalpath = NULL;

  int rc = -1;

#ifdef USE_NNTP
  if (flags & SEND_NEWS)
    OptNewsSend = true;
  else
    OptNewsSend = false;
#endif

  if (!flags && !msg && Recall != MUTT_NO && mutt_num_postponed(1))
  {
    /* If the user is composing a new message, check to see if there
     * are any postponed messages first.
     */
    i = query_quadoption(Recall, _("Recall postponed message?"));
    if (i == MUTT_ABORT)
      return rc;

    if (i == MUTT_YES)
      flags |= SEND_POSTPONED;
  }

  if (flags & SEND_POSTPONED)
  {
    if (WithCrypto & APPLICATION_PGP)
      pgp_signas = mutt_str_strdup(PgpSignAs);
    if (WithCrypto & APPLICATION_SMIME)
      smime_signas = mutt_str_strdup(SmimeSignAs);
  }

  /* Delay expansion of aliases until absolutely necessary--shouldn't
   * be necessary unless we are prompting the user or about to execute a
   * send-hook.
   */

  if (!msg)
  {
    msg = mutt_header_new();

    if (flags == SEND_POSTPONED)
    {
      flags = mutt_get_postponed(ctx, msg, &cur, fcc, sizeof(fcc));
      if (flags < 0)
        goto cleanup;
#ifdef USE_NNTP
      /* If postponed message is a news article, it have
       * a "Newsgroups:" header line, then set appropriate flag.
       */
      if (msg->env->newsgroups)
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
      tempfp = mutt_file_fopen(msg->content->filename, "a+");
      if (!tempfp)
      {
        mutt_perror(msg->content->filename);
        goto cleanup;
      }
    }

    if (!msg->env)
      msg->env = mutt_env_new();
  }

  /* Parse and use an eventual list-post header */
  if ((flags & SEND_LIST_REPLY) && cur && cur->env && cur->env->list_post)
  {
    /* Use any list-post header as a template */
    url_parse_mailto(msg->env, NULL, cur->env->list_post);
    /* We don't let them set the sender's address. */
    mutt_addr_free(&msg->env->from);
  }

  if (!(flags & (SEND_KEY | SEND_POSTPONED | SEND_RESEND)))
  {
    /* When SEND_DRAFT_FILE is set, the caller has already
     * created the "parent" body structure.
     */
    if (!(flags & SEND_DRAFT_FILE))
    {
      pbody = mutt_body_new();
      pbody->next = msg->content; /* don't kill command-line attachments */
      msg->content = pbody;

      ctype = mutt_str_strdup(ContentType);
      if (!ctype)
        ctype = mutt_str_strdup("text/plain");
      mutt_parse_content_type(ctype, msg->content);
      FREE(&ctype);
      msg->content->unlink = true;
      msg->content->use_disp = false;
      msg->content->disposition = DISP_INLINE;

      if (!tempfile)
      {
        mutt_mktemp(buffer, sizeof(buffer));
        tempfp = mutt_file_fopen(buffer, "w+");
        msg->content->filename = mutt_str_strdup(buffer);
      }
      else
      {
        tempfp = mutt_file_fopen(tempfile, "a+");
        msg->content->filename = mutt_str_strdup(tempfile);
      }
    }
    else
      tempfp = mutt_file_fopen(msg->content->filename, "a+");

    if (!tempfp)
    {
      mutt_debug(1, "can't create tempfile %s (errno=%d)\n", msg->content->filename, errno);
      mutt_perror(msg->content->filename);
      goto cleanup;
    }
  }

  /* this is handled here so that the user can match ~f in send-hook */
  if (cur && ReverseName && !(flags & (SEND_POSTPONED | SEND_RESEND)))
  {
    /* we shouldn't have to worry about freeing `msg->env->from' before
     * setting it here since this code will only execute when doing some
     * sort of reply.  the pointer will only be set when using the -H command
     * line option.
     *
     * We shouldn't have to worry about alias expansion here since we are
     * either replying to a real or postponed message, therefore no aliases
     * should exist since the user has not had the opportunity to add
     * addresses to the list.  We just have to ensure the postponed messages
     * have their aliases expanded.
     */

    if (msg->env->from)
    {
      mutt_debug(5, "msg->env->from before set_reverse_name: %s\n",
                 msg->env->from->mailbox);
    }
    msg->env->from = set_reverse_name(cur->env);
  }
  if (cur && ReplyWithXorig && !(flags & (SEND_POSTPONED | SEND_RESEND | SEND_FORWARD)))
  {
    /* We shouldn't have to worry about freeing `msg->env->from' before
     * setting it here since this code will only execute when doing some
     * sort of reply. The pointer will only be set when using the -H command
     * line option.
     *
     * If there is already a from address recorded in `msg->env->from',
     * then it theoretically comes from ReverseName handling, and we don't use
     * the `X-Orig-To header'.
     */
    if (cur->env->x_original_to && !msg->env->from)
    {
      msg->env->from = mutt_addr_copy(cur->env->x_original_to);
      mutt_debug(5, "msg->env->from extracted from X-Original-To: header: %s\n",
                 msg->env->from->mailbox);
    }
  }

  if (!(flags & (SEND_POSTPONED | SEND_RESEND)) && !((flags & SEND_DRAFT_FILE) && ResumeDraftFiles))
  {
    if ((flags & (SEND_REPLY | SEND_FORWARD)) && ctx &&
        envelope_defaults(msg->env, ctx, cur, flags) == -1)
    {
      goto cleanup;
    }

    if (Hdrs)
      process_user_recips(msg->env);

    /* Expand aliases and remove duplicates/crossrefs */
    mutt_expand_aliases_env(msg->env);

    if (flags & SEND_REPLY)
      mutt_fix_reply_recipients(msg->env);

#ifdef USE_NNTP
    if ((flags & SEND_NEWS) && ctx && ctx->magic == MUTT_NNTP && !msg->env->newsgroups)
      msg->env->newsgroups = mutt_str_strdup(((struct NntpData *) ctx->data)->group);
#endif

    if (!(flags & (SEND_MAILX | SEND_BATCH)) && !(Autoedit && EditHeaders) &&
        !((flags & SEND_REPLY) && FastReply))
    {
      if (edit_envelope(msg->env, flags) == -1)
        goto cleanup;
    }

    /* the from address must be set here regardless of whether or not
     * $use_from is set so that the `~P' (from you) operator in send-hook
     * patterns will work.  if $use_from is unset, the from address is killed
     * after send-hooks are evaluated */

    if (!msg->env->from)
    {
      msg->env->from = mutt_default_from();
      killfrom = true;
    }

    if ((flags & SEND_REPLY) && cur)
    {
      /* change setting based upon message we are replying to */
      mutt_message_hook(ctx, cur, MUTT_REPLY_HOOK);

      /* set the replied flag for the message we are generating so that the
       * user can use ~Q in a send-hook to know when reply-hook's are also
       * being used.
       */
      msg->replied = true;
    }

    /* change settings based upon recipients */

    mutt_message_hook(NULL, msg, MUTT_SEND_HOOK);

    /* Unset the replied flag from the message we are composing since it is
     * no longer required.  This is done here because the FCC'd copy of
     * this message was erroneously get the 'R'eplied flag when stored in
     * a maildir-style mailbox.
     */
    msg->replied = false;

    if (!(flags & SEND_KEY))
    {
      if (TextFlowed && msg->content->type == TYPE_TEXT &&
          (mutt_str_strcasecmp(msg->content->subtype, "plain") == 0))
      {
        mutt_param_set(&msg->content->parameter, "format", "flowed");
      }
    }

    /* $use_from and/or $from might have changed in a send-hook */
    if (killfrom)
    {
      mutt_addr_free(&msg->env->from);
      if (UseFrom && !(flags & (SEND_POSTPONED | SEND_RESEND)))
        msg->env->from = mutt_default_from();
      killfrom = false;
    }

    if (Hdrs)
      process_user_header(msg->env);

    if (flags & SEND_BATCH)
      mutt_file_copy_stream(stdin, tempfp);

    if (SigOnTop && !(flags & (SEND_MAILX | SEND_KEY | SEND_BATCH)) && Editor &&
        (mutt_str_strcmp(Editor, "builtin") != 0))
    {
      append_signature(tempfp);
    }

    /* include replies/forwarded messages, unless we are given a template */
    if (!tempfile && (ctx || !(flags & (SEND_REPLY | SEND_FORWARD))) &&
        generate_body(tempfp, msg, flags, ctx, cur) == -1)
    {
      goto cleanup;
    }

    if (!SigOnTop && !(flags & (SEND_MAILX | SEND_KEY | SEND_BATCH)) &&
        Editor && (mutt_str_strcmp(Editor, "builtin") != 0))
    {
      append_signature(tempfp);
    }
  }

  /* This hook is even called for postponed messages, and can, e.g., be
   * used for setting the editor, the sendmail path, or the
   * envelope sender.
   */
  mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);

  /* wait until now to set the real name portion of our return address so
     that $realname can be set in a send-hook */
  if (msg->env->from && !msg->env->from->personal && !(flags & (SEND_RESEND | SEND_POSTPONED)))
    msg->env->from->personal = mutt_str_strdup(Realname);

  if (!(((WithCrypto & APPLICATION_PGP) != 0) && (flags & SEND_KEY)))
    mutt_file_fclose(&tempfp);

  if (flags & SEND_MAILX)
  {
    if (mutt_builtin_editor(msg->content->filename, msg, cur) == -1)
      goto cleanup;
  }
  else if (!(flags & SEND_BATCH))
  {
    struct stat st;
    time_t mtime = mutt_file_decrease_mtime(msg->content->filename, NULL);

    mutt_update_encoding(msg->content);

    /* Select whether or not the user's editor should be called now.  We
     * don't want to do this when:
     * 1) we are sending a key/cert
     * 2) we are forwarding a message and the user doesn't want to edit it.
     *    This is controlled by the quadoption $forward_edit.  However, if
     *    both $edit_headers and $autoedit are set, we want to ignore the
     *    setting of $forward_edit because the user probably needs to add the
     *    recipients.
     */
    if (!(flags & SEND_KEY) &&
        ((flags & SEND_FORWARD) == 0 || (EditHeaders && Autoedit) ||
         query_quadoption(ForwardEdit, _("Edit forwarded message?")) == MUTT_YES))
    {
      /* If the this isn't a text message, look for a mailcap edit command */
      if (mutt_needs_mailcap(msg->content))
      {
        if (!mutt_edit_attachment(msg->content))
          goto cleanup;
      }
      else if (!Editor || (mutt_str_strcmp("builtin", Editor) == 0))
        mutt_builtin_editor(msg->content->filename, msg, cur);
      else if (EditHeaders)
      {
        mutt_env_to_local(msg->env);
        mutt_edit_headers(Editor, msg->content->filename, msg, fcc, sizeof(fcc));
        mutt_env_to_intl(msg->env, NULL, NULL);
      }
      else
      {
        mutt_edit_file(Editor, msg->content->filename);
        if (stat(msg->content->filename, &st) == 0)
        {
          if (mtime != st.st_mtime)
            fix_end_of_file(msg->content->filename);
        }
        else
          mutt_perror(msg->content->filename);
      }

      /* If using format=flowed, perform space stuffing.  Avoid stuffing when
       * recalling a postponed message where the stuffing was already
       * performed.  If it has already been performed, the format=flowed
       * parameter will be present.
       */
      if (TextFlowed && msg->content->type == TYPE_TEXT &&
          (mutt_str_strcasecmp("plain", msg->content->subtype) == 0))
      {
        char *p = mutt_param_get(&msg->content->parameter, "format");
        if (mutt_str_strcasecmp("flowed", NONULL(p)) != 0)
          rfc3676_space_stuff(msg);
      }

      mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
    }

    if (!(flags & (SEND_POSTPONED | SEND_FORWARD | SEND_KEY | SEND_RESEND | SEND_DRAFT_FILE)))
    {
      if (stat(msg->content->filename, &st) == 0)
      {
        /* if the file was not modified, bail out now */
        if (mtime == st.st_mtime && !msg->content->next &&
            query_quadoption(AbortUnmodified, _("Abort unmodified message?")) == MUTT_YES)
        {
          mutt_message(_("Aborted unmodified message."));
          goto cleanup;
        }
      }
      else
        mutt_perror(msg->content->filename);
    }
  }

  /* Set the message security unless:
   * 1) crypto support is not enabled (WithCrypto==0)
   * 2) pgp: header field was present during message editing with $edit_headers (msg->security != 0)
   * 3) we are resending a message
   * 4) we are recalling a postponed message (don't override the user's saved settings)
   * 5) we are in mailx mode
   * 6) we are in batch mode
   *
   * This is done after allowing the user to edit the message so that security
   * settings can be configured with send2-hook and $edit_headers.
   */
  if ((WithCrypto != 0) && (msg->security == 0) &&
      !(flags & (SEND_BATCH | SEND_MAILX | SEND_POSTPONED | SEND_RESEND)))
  {
    if (CryptAutosign)
      msg->security |= SIGN;
    if (CryptAutoencrypt)
      msg->security |= ENCRYPT;
    if (CryptReplyencrypt && cur && (cur->security & ENCRYPT))
      msg->security |= ENCRYPT;
    if (CryptReplysign && cur && (cur->security & SIGN))
      msg->security |= SIGN;
    if (CryptReplysignencrypted && cur && (cur->security & ENCRYPT))
      msg->security |= SIGN;
    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        ((msg->security & (ENCRYPT | SIGN)) || CryptOpportunisticEncrypt))
    {
      if (PgpAutoinline)
        msg->security |= INLINE;
      if (PgpReplyinline && cur && (cur->security & INLINE))
        msg->security |= INLINE;
    }

    if (msg->security || CryptOpportunisticEncrypt)
    {
      /* When replying / forwarding, use the original message's
       * crypto system.  According to the documentation,
       * smime_is_default should be disregarded here.
       *
       * Problem: At least with forwarding, this doesn't really
       * make much sense. Should we have an option to completely
       * disable individual mechanisms at run-time?
       */
      if (cur)
      {
        if (((WithCrypto & APPLICATION_PGP) != 0) && CryptAutopgp &&
            (cur->security & APPLICATION_PGP))
        {
          msg->security |= APPLICATION_PGP;
        }
        else if (((WithCrypto & APPLICATION_SMIME) != 0) && CryptAutosmime &&
                 (cur->security & APPLICATION_SMIME))
        {
          msg->security |= APPLICATION_SMIME;
        }
      }

      /* No crypto mechanism selected? Use availability + smime_is_default
       * for the decision.
       */
      if (!(msg->security & (APPLICATION_SMIME | APPLICATION_PGP)))
      {
        if (((WithCrypto & APPLICATION_SMIME) != 0) && CryptAutosmime && SmimeIsDefault)
        {
          msg->security |= APPLICATION_SMIME;
        }
        else if (((WithCrypto & APPLICATION_PGP) != 0) && CryptAutopgp)
        {
          msg->security |= APPLICATION_PGP;
        }
        else if (((WithCrypto & APPLICATION_SMIME) != 0) && CryptAutosmime)
        {
          msg->security |= APPLICATION_SMIME;
        }
      }
    }

    /* opportunistic encrypt relies on SMIME or PGP already being selected */
    if (CryptOpportunisticEncrypt)
    {
      /* If something has already enabled encryption, e.g. CryptAutoencrypt
       * or CryptReplyencrypt, then don't enable opportunistic encrypt for
       * the message.
       */
      if (!(msg->security & ENCRYPT))
      {
        msg->security |= OPPENCRYPT;
        crypt_opportunistic_encrypt(msg);
      }
    }

    /* No permissible mechanisms found.  Don't sign or encrypt. */
    if (!(msg->security & (APPLICATION_SMIME | APPLICATION_PGP)))
      msg->security = 0;
  }

  /* Deal with the corner case where the crypto module backend is not available.
   * This can happen if configured without pgp/smime and with gpgme, but
   * $crypt_use_gpgme is unset.
   */
  if (msg->security && !crypt_has_module_backend(msg->security))
  {
    mutt_error(_(
        "No crypto backend configured.  Disabling message security setting."));
    msg->security = 0;
  }

  /* specify a default fcc.  if we are in batchmode, only save a copy of
   * the message if the value of $copy is yes or ask-yes */

  if (!fcc[0] && !(flags & (SEND_POSTPONED_FCC)) && (!(flags & SEND_BATCH) || (Copy & 0x1)))
  {
    /* set the default FCC */
    if (!msg->env->from)
    {
      msg->env->from = mutt_default_from();
      killfrom = true; /* no need to check $use_from because if the user specified
                       a from address it would have already been set by now */
    }
    mutt_select_fcc(fcc, sizeof(fcc), msg);
    if (killfrom)
    {
      mutt_addr_free(&msg->env->from);
    }
  }

  mutt_update_encoding(msg->content);

  if (!(flags & (SEND_MAILX | SEND_BATCH)))
  {
  main_loop:

    fcc_error = false; /* reset value since we may have failed before */
    mutt_pretty_mailbox(fcc, sizeof(fcc));
    i = mutt_compose_menu(msg, fcc, sizeof(fcc), cur,
                          ((flags & SEND_NO_FREE_HEADER) ? MUTT_COMPOSE_NOFREEHEADER : 0));
    if (i == -1)
    {
/* abort */
#ifdef USE_NNTP
      if (flags & SEND_NEWS)
        mutt_message(_("Article not posted."));
      else
#endif
        mutt_message(_("Mail not sent."));
      goto cleanup;
    }
    else if (i == 1)
    {
      /* postpone the message until later. */
      if (msg->content->next)
        msg->content = mutt_make_multipart(msg->content);

      if ((WithCrypto != 0) && PostponeEncrypt && (msg->security & ENCRYPT))
      {
        char *encrypt_as = NULL;

        if (((WithCrypto & APPLICATION_PGP) != 0) && (msg->security & APPLICATION_PGP))
          encrypt_as = PgpDefaultKey;
        else if (((WithCrypto & APPLICATION_SMIME) != 0) && (msg->security & APPLICATION_SMIME))
          encrypt_as = SmimeDefaultKey;
        if (!(encrypt_as && *encrypt_as))
          encrypt_as = PostponeEncryptAs;

        if (encrypt_as && *encrypt_as)
        {
          int is_signed = msg->security & SIGN;
          if (is_signed)
            msg->security &= ~SIGN;

          pgpkeylist = mutt_str_strdup(encrypt_as);
          if (mutt_protect(msg, pgpkeylist) == -1)
          {
            if (is_signed)
              msg->security |= SIGN;
            FREE(&pgpkeylist);
            msg->content = mutt_remove_multipart(msg->content);
            goto main_loop;
          }

          if (is_signed)
            msg->security |= SIGN;
          FREE(&pgpkeylist);
        }
      }

      /* make sure the message is written to the right part of a maildir
       * postponed folder.
       */
      msg->read = false;
      msg->old = false;

      mutt_encode_descriptions(msg->content, true);
      mutt_prepare_envelope(msg->env, false);
      mutt_env_to_intl(msg->env, NULL, NULL); /* Handle bad IDNAs the next time. */

      if (!Postponed || mutt_write_fcc(NONULL(Postponed), msg,
                                       (cur && (flags & SEND_REPLY)) ? cur->env->message_id : NULL,
                                       true, fcc, NULL) < 0)
      {
        msg->content = mutt_remove_multipart(msg->content);
        decode_descriptions(msg->content);
        mutt_unprepare_envelope(msg->env);
        goto main_loop;
      }
      mutt_update_num_postponed();
      mutt_message(_("Message postponed."));
      rc = 1;
      goto cleanup;
    }
  }

#ifdef USE_NNTP
  if (!(flags & SEND_NEWS))
#endif
    if ((mutt_addr_has_recips(msg->env->to) == 0) &&
        (mutt_addr_has_recips(msg->env->cc) == 0) &&
        (mutt_addr_has_recips(msg->env->bcc) == 0))
    {
      if (!(flags & SEND_BATCH))
      {
        mutt_error(_("No recipients specified."));
        goto main_loop;
      }
      else
      {
        puts(_("No recipients specified."));
        goto cleanup;
      }
    }

  if (mutt_env_to_intl(msg->env, &tag, &err))
  {
    mutt_error(_("Bad IDN in '%s': '%s'"), tag, err);
    FREE(&err);
    if (!(flags & SEND_BATCH))
      goto main_loop;
    else
      goto cleanup;
  }

  if (!msg->env->subject && !(flags & SEND_BATCH) &&
      (i = query_quadoption(AbortNosubject, _("No subject, abort sending?"))) != MUTT_NO)
  {
    /* if the abort is automatic, print an error message */
    if (AbortNosubject == MUTT_YES)
      mutt_error(_("No subject specified."));
    goto main_loop;
  }
#ifdef USE_NNTP
  if ((flags & SEND_NEWS) && !msg->env->subject)
  {
    mutt_error(_("No subject specified."));
    goto main_loop;
  }

  if ((flags & SEND_NEWS) && !msg->env->newsgroups)
  {
    mutt_error(_("No newsgroup specified."));
    goto main_loop;
  }
#endif

  if (!(flags & SEND_BATCH) && (AbortNoattach != MUTT_NO) &&
      !msg->content->next && (msg->content->type == TYPE_TEXT) &&
      (mutt_str_strcasecmp(msg->content->subtype, "plain") == 0) &&
      search_attach_keyword(msg->content->filename) &&
      query_quadoption(AbortNoattach, _("No attachments, cancel sending?")) != MUTT_NO)
  {
    /* if the abort is automatic, print an error message */
    if (AbortNoattach == MUTT_YES)
    {
      mutt_error(_("Message contains text matching "
                   "\"$abort_noattach_regex\". Not sending."));
    }
    goto main_loop;
  }

  if (msg->content->next)
    msg->content = mutt_make_multipart(msg->content);

  /* Ok, we need to do it this way instead of handling all fcc stuff in
   * one place in order to avoid going to main_loop with encoded "env"
   * in case of error.  Ugh.
   */

  mutt_encode_descriptions(msg->content, true);

  /* Make sure that clear_content and free_clear_content are
   * properly initialized -- we may visit this particular place in
   * the code multiple times, including after a failed call to
   * mutt_protect().
   */

  clear_content = NULL;
  free_clear_content = false;

  if (WithCrypto)
  {
    if (msg->security & (ENCRYPT | SIGN))
    {
      /* save the decrypted attachments */
      clear_content = msg->content;

      if ((crypt_get_keys(msg, &pgpkeylist, 0) == -1) || mutt_protect(msg, pgpkeylist) == -1)
      {
        msg->content = mutt_remove_multipart(msg->content);

        FREE(&pgpkeylist);

        decode_descriptions(msg->content);
        goto main_loop;
      }
      mutt_encode_descriptions(msg->content, false);
    }

    /* at this point, msg->content is one of the following three things:
     * - multipart/signed.  In this case, clear_content is a child.
     * - multipart/encrypted.  In this case, clear_content exists
     *   independently
     * - application/pgp.  In this case, clear_content exists independently.
     * - something else.  In this case, it's the same as clear_content.
     */

    /* This is ugly -- lack of "reporting back" from mutt_protect(). */

    if (clear_content && (msg->content != clear_content) && (msg->content->parts != clear_content))
      free_clear_content = true;
  }

  if (!OptNoCurses && !(flags & SEND_MAILX))
    mutt_message(_("Sending message..."));

  mutt_prepare_envelope(msg->env, true);

  /* save a copy of the message, if necessary. */

  mutt_expand_path(fcc, sizeof(fcc));

  /* Don't save a copy when we are in batch-mode, and the FCC
   * folder is on an IMAP server: This would involve possibly lots
   * of user interaction, which is not available in batch mode.
   *
   * Note: A patch to fix the problems with the use of IMAP servers
   * from non-curses mode is available from Brendan Cully.  However,
   * I'd like to think a bit more about this before including it.
   */

#ifdef USE_IMAP
  if ((flags & SEND_BATCH) && fcc[0] && mx_is_imap(fcc))
    fcc[0] = '\0';
#endif

  if (*fcc && (mutt_str_strcmp("/dev/null", fcc) != 0))
  {
    struct Body *tmpbody = msg->content;
    struct Body *save_sig = NULL;
    struct Body *save_parts = NULL;

    if ((WithCrypto != 0) && (msg->security & (ENCRYPT | SIGN)) && FccClear)
      msg->content = clear_content;

    /* check to see if the user wants copies of all attachments */
    if (msg->content->type == TYPE_MULTIPART)
    {
      if ((WithCrypto != 0) && (msg->security & (ENCRYPT | SIGN)) &&
          ((mutt_str_strcmp(msg->content->subtype, "encrypted") == 0) ||
           (mutt_str_strcmp(msg->content->subtype, "signed") == 0)))
      {
        if (clear_content->type == TYPE_MULTIPART &&
            query_quadoption(FccAttach, _("Save attachments in Fcc?")) == MUTT_NO)
        {
          if (!(msg->security & ENCRYPT) && (msg->security & SIGN))
          {
            /* save initial signature and attachments */
            save_sig = msg->content->parts->next;
            save_parts = clear_content->parts->next;
          }

          /* this means writing only the main part */
          msg->content = clear_content->parts;

          if (mutt_protect(msg, pgpkeylist) == -1)
          {
            /* we can't do much about it at this point, so
             * fallback to saving the whole thing to fcc
             */
            msg->content = tmpbody;
            save_sig = NULL;
            goto full_fcc;
          }

          save_content = msg->content;
        }
      }
      else
      {
        if (query_quadoption(FccAttach, _("Save attachments in Fcc?")) == MUTT_NO)
          msg->content = msg->content->parts;
      }
    }

  full_fcc:
    if (msg->content)
    {
      /* update received time so that when storing to a mbox-style folder
       * the From_ line contains the current time instead of when the
       * message was first postponed.
       */
      msg->received = time(NULL);
      if (mutt_write_multiple_fcc(fcc, msg, NULL, false, NULL, &finalpath) == -1)
      {
        /* Error writing FCC, we should abort sending.  */
        fcc_error = true;
      }
    }

    msg->content = tmpbody;

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
      msg->content->parts->next = save_sig;
      msg->content->parts->parts->next = save_parts;
    }
    else if ((WithCrypto != 0) && save_content)
    {
      /* destroy the new encrypted body. */
      mutt_body_free(&save_content);
    }
  }

  /* Don't attempt to send the message if the FCC failed.  Just pretend
   * the send failed as well so we give the user a chance to fix the
   * error.
   */
  if (fcc_error || (i = send_message(msg)) < 0)
  {
    if (!(flags & SEND_BATCH))
    {
      if (!WithCrypto)
        ;
      else if ((msg->security & ENCRYPT) ||
               ((msg->security & SIGN) && msg->content->type == TYPE_APPLICATION))
      {
        mutt_body_free(&msg->content); /* destroy PGP data */
        msg->content = clear_content;  /* restore clear text. */
      }
      else if ((msg->security & SIGN) && msg->content->type == TYPE_MULTIPART)
      {
        mutt_body_free(&msg->content->parts->next); /* destroy sig */
        msg->content = mutt_remove_multipart(msg->content);
      }

      msg->content = mutt_remove_multipart(msg->content);
      decode_descriptions(msg->content);
      mutt_unprepare_envelope(msg->env);
      FREE(&finalpath);
      goto main_loop;
    }
    else
    {
      puts(_("Could not send the message."));
      goto cleanup;
    }
  }
  else if (!OptNoCurses && !(flags & SEND_MAILX))
  {
    mutt_message(i != 0 ? _("Sending in background.") :
                          (flags & SEND_NEWS) ? _("Article posted.") : /* USE_NNTP */
                              _("Mail sent."));
#ifdef USE_NOTMUCH
    if (NmRecord)
      nm_record_message(ctx, finalpath, cur);
#endif
  }

  if ((WithCrypto != 0) && (msg->security & ENCRYPT))
    FREE(&pgpkeylist);

  if ((WithCrypto != 0) && free_clear_content)
    mutt_body_free(&clear_content);

  /* set 'replied' flag only if the user didn't change/remove
     In-Reply-To: and References: headers during edit */
  if (flags & SEND_REPLY)
  {
    if (cur && ctx)
      mutt_set_flag(ctx, cur, MUTT_REPLIED, is_reply(cur, msg));
    else if (!(flags & SEND_POSTPONED) && ctx && ctx->tagged)
    {
      for (i = 0; i < ctx->msgcount; i++)
      {
        if (message_is_tagged(ctx, i))
        {
          mutt_set_flag(ctx, ctx->hdrs[i], MUTT_REPLIED, is_reply(ctx->hdrs[i], msg));
        }
      }
    }
  }

  rc = 0;

cleanup:

  if (flags & SEND_POSTPONED)
  {
    if (WithCrypto & APPLICATION_PGP)
    {
      FREE(&PgpSignAs);
      PgpSignAs = pgp_signas;
    }
    if (WithCrypto & APPLICATION_SMIME)
    {
      FREE(&SmimeSignAs);
      SmimeSignAs = smime_signas;
    }
  }

  mutt_file_fclose(&tempfp);
  if (!(flags & SEND_NO_FREE_HEADER))
    mutt_header_free(&msg);

  FREE(&finalpath);
  return rc;
}
