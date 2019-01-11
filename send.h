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
#include <stdio.h>

struct Address;
struct Body;
struct Context;
struct Email;
struct EmailList;
struct Envelope;
struct Mailbox;

/* These Config Variables are only used in send.c */
extern unsigned char AbortNoattach; /* forgotten attachment detector */
extern struct Regex *AbortNoattachRegex;
extern unsigned char AbortNosubject;
extern unsigned char AbortUnmodified;
extern bool          AskFollowUp;
extern bool          AskXCommentTo;
extern char *        ContentType;
extern bool          CryptAutoencrypt;
extern bool          CryptAutopgp;
extern bool          CryptAutosign;
extern bool          CryptAutosmime;
extern bool          CryptReplyencrypt;
extern bool          CryptReplysign;
extern bool          CryptReplysignencrypted;
extern char *        EmptySubject;
extern bool          FastReply;
extern unsigned char FccAttach;
extern bool          FccClear;
extern bool          FollowupTo;
extern char *        ForwardAttributionIntro;
extern char *        ForwardAttributionTrailer;
extern unsigned char ForwardEdit;
extern char *        ForwardFormat;
extern bool          ForwardReferences;
extern bool          Hdrs;
extern unsigned char HonorFollowupTo;
extern bool          IgnoreListReplyTo;
extern unsigned char Include;
extern bool          Metoo;
extern bool          NmRecord;
extern bool          PgpReplyinline;
extern char *        PostIndentString;
extern bool          PostponeEncrypt;
extern char *        PostponeEncryptAs;
extern unsigned char Recall;
extern bool          ReplySelf;
extern unsigned char ReplyTo;
extern bool          ReplyWithXorig;
extern bool          ReverseName;
extern bool          ReverseRealname;
extern bool          SigDashes;
extern char *        Signature;
extern bool          SigOnTop;
extern bool          UseFrom;

/* flags to ci_send_message() */
#define SEND_REPLY          (1 << 0)
#define SEND_GROUP_REPLY    (1 << 1)
#define SEND_LIST_REPLY     (1 << 2)
#define SEND_FORWARD        (1 << 3)
#define SEND_POSTPONED      (1 << 4)
#define SEND_BATCH          (1 << 5)
#define SEND_MAILX          (1 << 6)
#define SEND_KEY            (1 << 7)
#define SEND_RESEND         (1 << 8)
#define SEND_POSTPONED_FCC  (1 << 9)  /**< used by mutt_get_postponed() to signal that the x-mutt-fcc header field was present */
#define SEND_NO_FREE_HEADER (1 << 10) /**< Used by the -E flag */
#define SEND_DRAFT_FILE     (1 << 11) /**< Used by the -H flag */
#define SEND_TO_SENDER      (1 << 12)
#define SEND_NEWS           (1 << 13)

int             ci_send_message(int flags, struct Email *msg, const char *tempfile, struct Context *ctx, struct EmailList *el);
void            mutt_add_to_reference_headers(struct Envelope *env, struct Envelope *curenv);
struct Address *mutt_default_from(void);
void            mutt_encode_descriptions(struct Body *b, bool recurse);
int             mutt_fetch_recips(struct Envelope *out, struct Envelope *in, int flags);
void            mutt_fix_reply_recipients(struct Envelope *env);
void            mutt_forward_intro(struct Mailbox *m, struct Email *e, FILE *fp);
void            mutt_forward_trailer(struct Mailbox *m, struct Email *e, FILE *fp);
void            mutt_make_attribution(struct Mailbox *m, struct Email *e, FILE *out);
void            mutt_make_forward_subject(struct Envelope *env, struct Mailbox *m, struct Email *e);
void            mutt_make_misc_reply_headers(struct Envelope *env, struct Envelope *curenv);
void            mutt_make_post_indent(struct Mailbox *m, struct Email *e, FILE *out);
struct Address *mutt_remove_xrefs(struct Address *a, struct Address *b);
int             mutt_resend_message(FILE *fp, struct Context *ctx, struct Email *cur);
void            mutt_set_followup_to(struct Envelope *e);

#endif /* MUTT_SEND_H */
