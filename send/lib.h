/**
 * @file
 * Convenience wrapper for the send headers
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page send SEND: Shared code for sending Emails
 *
 * | File             | Description             |
 * | :--------------- | :---------------------- |
 * | send/body.c      | @subpage send_body      |
 * | send/config.c    | @subpage send_config    |
 * | send/header.c    | @subpage send_header    |
 * | send/multipart.c | @subpage send_multipart |
 * | send/send.c      | @subpage send_send      |
 * | send/sendlib.c   | @subpage send_sendlib   |
 * | send/sendmail.c  | @subpage send_sendmail  |
 * | send/smtp.c      | @subpage send_smtp      |
 */

#ifndef MUTT_SEND_LIB_H
#define MUTT_SEND_LIB_H

#include <stdbool.h>
// IWYU pragma: begin_exports
#include "body.h"
#include "header.h"
#include "multipart.h"
#include "send.h"
#include "sendlib.h"
#include "sendmail.h"
#include "smtp.h"
// IWYU pragma: end_exports

extern unsigned char C_AbortNoattach;
extern struct Regex *C_AbortNoattachRegex;
extern unsigned char C_AbortNosubject;
extern unsigned char C_AbortUnmodified;
extern bool          C_Allow8bit;
extern bool          C_AskFollowUp;
extern bool          C_AskXCommentTo;
extern char *        C_AttachCharset;
extern bool          C_BounceDelivered;
extern char *        C_ContentType;
extern bool          C_CryptAutoencrypt;
extern bool          C_CryptAutopgp;
extern bool          C_CryptAutosign;
extern bool          C_CryptAutosmime;
extern bool          C_CryptReplyencrypt;
extern bool          C_CryptReplysign;
extern bool          C_CryptReplysignencrypted;
extern char *        C_DsnNotify;
extern char *        C_DsnReturn;
extern char *        C_EmptySubject;
extern bool          C_EncodeFrom;
extern bool          C_FastReply;
extern unsigned char C_FccAttach;
extern bool          C_FccBeforeSend;
extern bool          C_FccClear;
extern bool          C_FollowupTo;
extern char *        C_ForwardAttributionIntro;
extern char *        C_ForwardAttributionTrailer;
extern bool          C_ForwardDecrypt;
extern unsigned char C_ForwardEdit;
extern char *        C_ForwardFormat;
extern bool          C_ForwardReferences;
extern bool          C_Hdrs;
extern bool          C_HiddenHost;
extern unsigned char C_HonorFollowupTo;
extern bool          C_IgnoreListReplyTo;
extern unsigned char C_Include;
extern char *        C_Inews;
extern bool          C_Metoo;
extern bool          C_MimeForwardDecode;
extern bool          C_MimeSubject;
extern char *        C_MimeTypeQueryCommand;
extern bool          C_MimeTypeQueryFirst;
extern bool          C_NmRecord;
extern bool          C_PgpReplyinline;
extern char *        C_PostIndentString;
extern char *        C_PostponeEncryptAs;
extern bool          C_PostponeEncrypt;
extern unsigned char C_Recall;
extern bool          C_ReplySelf;
extern unsigned char C_ReplyTo;
extern bool          C_ReplyWithXorig;
extern bool          C_ReverseName;
extern bool          C_ReverseRealname;
extern char *        C_Sendmail;
extern short         C_SendmailWait;
extern bool          C_SigDashes;
extern char *        C_Signature;
extern bool          C_SigOnTop;
extern struct Slist *C_SmtpAuthenticators;
extern char *        C_SmtpOauthRefreshCommand;
extern char *        C_SmtpPass;
extern char *        C_SmtpUrl;
extern char *        C_SmtpUser;
extern bool          C_Use8bitmime;
extern bool          C_UseEnvelopeFrom;
extern bool          C_UseFrom;
extern bool          C_UserAgent;
extern short         C_WrapHeaders;

#endif /* MUTT_SEND_LIB_H */
