/**
 * @file
 * Prepare and send an email
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SEND_H
#define MUTT_SEND_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct Address;
struct Body;
struct Context;
struct Email;
struct EmailList;
struct Envelope;
struct Mailbox;

/* These Config Variables are only used in send.c */
extern unsigned char C_AbortNoattach; /* forgotten attachment detector */
extern struct Regex *C_AbortNoattachRegex;
extern unsigned char C_AbortNosubject;
extern unsigned char C_AbortUnmodified;
extern bool          C_AskFollowUp;
extern bool          C_AskXCommentTo;
extern char *        C_ContentType;
extern bool          C_CryptAutoencrypt;
extern bool          C_CryptAutopgp;
extern bool          C_CryptAutosign;
extern bool          C_CryptAutosmime;
extern bool          C_CryptReplyencrypt;
extern bool          C_CryptReplysign;
extern bool          C_CryptReplysignencrypted;
extern char *        C_EmptySubject;
extern bool          C_FastReply;
extern unsigned char C_FccAttach;
extern bool          C_FccBeforeSend;
extern bool          C_FccClear;
extern bool          C_FollowupTo;
extern char *        C_ForwardAttributionIntro;
extern char *        C_ForwardAttributionTrailer;
extern unsigned char C_ForwardEdit;
extern char *        C_ForwardFormat;
extern bool          C_ForwardReferences;
extern bool          C_Hdrs;
extern unsigned char C_HonorFollowupTo;
extern bool          C_IgnoreListReplyTo;
extern unsigned char C_Include;
extern bool          C_Metoo;
extern bool          C_NmRecord;
extern bool          C_PgpReplyinline;
extern char *        C_PostIndentString;
extern bool          C_PostponeEncrypt;
extern char *        C_PostponeEncryptAs;
extern unsigned char C_Recall;
extern bool          C_ReplySelf;
extern unsigned char C_ReplyTo;
extern bool          C_ReplyWithXorig;
extern bool          C_ReverseName;
extern bool          C_ReverseRealname;
extern bool          C_SigDashes;
extern char *        C_Signature;
extern bool          C_SigOnTop;
extern bool          C_UseFrom;

typedef uint16_t SendFlags;             ///< Flags for ci_send_message(), e.g. #SEND_REPLY
#define SEND_NO_FLAGS               0   ///< No flags are set
#define SEND_REPLY            (1 << 0)  ///< Reply to sender
#define SEND_GROUP_REPLY      (1 << 1)  ///< Reply to all
#define SEND_LIST_REPLY       (1 << 2)  ///< Reply to mailing list
#define SEND_FORWARD          (1 << 3)  ///< Forward email
#define SEND_POSTPONED        (1 << 4)  ///< Recall a postponed email
#define SEND_BATCH            (1 << 5)  ///< Send email in batch mode (without user interaction)
#define SEND_MAILX            (1 << 6)  ///< Send email in Mailx compatibility mode
#define SEND_KEY              (1 << 7)  ///< Mail a PGP public key
#define SEND_RESEND           (1 << 8)  ///< Reply using the current email as a template
#define SEND_POSTPONED_FCC    (1 << 9)  ///< Used by mutt_get_postponed() to signal that the x-mutt-fcc header field was present
#define SEND_NO_FREE_HEADER   (1 << 10) ///< Used by the -E flag
#define SEND_DRAFT_FILE       (1 << 11) ///< Used by the -H flag
#define SEND_TO_SENDER        (1 << 12) ///< Compose new email to sender
#define SEND_GROUP_CHAT_REPLY (1 << 13) ///< Reply to all recipients preserving To/Cc
#define SEND_NEWS             (1 << 14) ///< Reply to a news article

int             ci_send_message(SendFlags flags, struct Email *e_templ, const char *tempfile, struct Context *ctx, struct EmailList *el);
void            mutt_add_to_reference_headers(struct Envelope *env, struct Envelope *curenv);
struct Address *mutt_default_from(void);
int             mutt_edit_address(struct AddressList *al, const char *field, bool expand_aliases);
void            mutt_encode_descriptions(struct Body *b, bool recurse);
int             mutt_fetch_recips(struct Envelope *out, struct Envelope *in, SendFlags flags);
void            mutt_fix_reply_recipients(struct Envelope *env);
void            mutt_forward_intro(struct Mailbox *m, struct Email *e, FILE *fp);
void            mutt_forward_trailer(struct Mailbox *m, struct Email *e, FILE *fp);
void            mutt_make_attribution(struct Mailbox *m, struct Email *e, FILE *fp_out);
void            mutt_make_forward_subject(struct Envelope *env, struct Mailbox *m, struct Email *e);
void            mutt_make_misc_reply_headers(struct Envelope *env, struct Envelope *curenv);
void            mutt_make_post_indent(struct Mailbox *m, struct Email *e, FILE *fp_out);
struct Address *mutt_remove_xrefs(struct Address *a, struct Address *b);
int             mutt_resend_message(FILE *fp, struct Context *ctx, struct Email *e_cur);
void            mutt_set_followup_to(struct Envelope *e);

#endif /* MUTT_SEND_H */
