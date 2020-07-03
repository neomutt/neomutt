/**
 * @file
 * Config used by libsend
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
 * @page send_config Config used by libsend
 *
 * Config used by libsend
 */

#include "config.h"
#include <stdbool.h>

// clang-format off
unsigned char C_AbortNoattach;             ///< Config: Abort sending the email if attachments are missing
struct Regex *C_AbortNoattachRegex;        ///< Config: Regex to match text indicating attachments are expected
unsigned char C_AbortNosubject;            ///< Config: Abort creating the email if subject is missing
unsigned char C_AbortUnmodified;           ///< Config: Abort the sending if the message hasn't been edited
bool          C_Allow8bit;                 ///< Config: Allow 8-bit messages, don't use quoted-printable or base64
bool          C_AskFollowUp;               ///< Config: (nntp) Ask the user for follow-up groups before editing
bool          C_AskXCommentTo;             ///< Config: (nntp) Ask the user for the 'X-Comment-To' field before editing
char *        C_AttachCharset;             ///< Config: When attaching files, use one of these character sets
bool          C_BounceDelivered;           ///< Config: Add 'Delivered-To' to bounced messages
char *        C_ContentType;               ///< Config: Default "Content-Type" for newly composed messages
bool          C_CryptAutoencrypt;          ///< Config: Automatically PGP encrypt all outgoing mail
bool          C_CryptAutopgp;              ///< Config: Allow automatic PGP functions
bool          C_CryptAutosign;             ///< Config: Automatically PGP sign all outgoing mail
bool          C_CryptAutosmime;            ///< Config: Allow automatic SMIME functions
bool          C_CryptReplyencrypt;         ///< Config: Encrypt replies to encrypted messages
bool          C_CryptReplysign;            ///< Config: Sign replies to signed messages
bool          C_CryptReplysignencrypted;   ///< Config: Sign replies to encrypted messages
char *        C_DsnNotify;                 ///< Config: Request notification for message delivery or delay
char *        C_DsnReturn;                 ///< Config: What to send as a notification of message delivery or delay
char *        C_EmptySubject;              ///< Config: Subject to use when replying to an email with none
bool          C_EncodeFrom;                ///< Config: Encode 'From ' as 'quote-printable' at the beginning of lines
bool          C_FastReply;                 ///< Config: Don't prompt for the recipients and subject when replying/forwarding
unsigned char C_FccAttach;                 ///< Config: Save send message with all their attachments
bool          C_FccBeforeSend;             ///< Config: Save FCCs before sending the message
bool          C_FccClear;                  ///< Config: Save sent messages unencrypted and unsigned
bool          C_FollowupTo;                ///< Config: Add the 'Mail-Followup-To' header is generated when sending mail
char *        C_ForwardAttributionIntro;   ///< Config: Prefix message for forwarded messages
char *        C_ForwardAttributionTrailer; ///< Config: Suffix message for forwarded messages
bool          C_ForwardDecrypt;            ///< Config: Decrypt the message when forwarding it
unsigned char C_ForwardEdit;               ///< Config: Automatically start the editor when forwarding a message
char *        C_ForwardFormat;             ///< Config: printf-like format string to control the subject when forwarding a message
bool          C_ForwardReferences;         ///< Config: Set the 'In-Reply-To' and 'References' headers when forwarding a message
bool          C_Hdrs;                      ///< Config: Add custom headers to outgoing mail
bool          C_HiddenHost;                ///< Config: Don't use the hostname, just the domain, when generating the message id
unsigned char C_HonorFollowupTo;           ///< Config: Honour the 'Mail-Followup-To' header when group replying
bool          C_IgnoreListReplyTo;         ///< Config: Ignore the 'Reply-To' header when using `<reply>` on a mailing list
unsigned char C_Include;                   ///< Config: Include a copy of the email that's being replied to
char *        C_Inews;                     ///< Config: (nntp) External command to post news articles
bool          C_Metoo;                     ///< Config: Remove the user's address from the list of recipients
bool          C_MimeForwardDecode;         ///< Config: Decode the forwarded message before attaching it
bool          C_MimeSubject;               ///< Config: (nntp) Encode the article subject in base64
char *        C_MimeTypeQueryCommand;      ///< Config: External command to determine the MIME type of an attachment
bool          C_MimeTypeQueryFirst;        ///< Config: Run the #C_MimeTypeQueryCommand before the mime.types lookup
bool          C_NmRecord;                  ///< Config: (notmuch) If the 'record' mailbox (sent mail) should be indexed
bool          C_PgpReplyinline;            ///< Config: Reply using old-style inline PGP messages (not recommended)
char *        C_PostIndentString;          ///< Config: Suffix message to add after reply text
char *        C_PostponeEncryptAs;         ///< Config: Fallback encryption key for postponed messages
bool          C_PostponeEncrypt;           ///< Config: Self-encrypt postponed messages
unsigned char C_Recall;                    ///< Config: Recall postponed mesaages when asked to compose a message
bool          C_ReplySelf;                 ///< Config: Really reply to yourself, when replying to your own email
unsigned char C_ReplyTo;                   ///< Config: Address to use as a 'Reply-To' header
bool          C_ReplyWithXorig;            ///< Config: Create 'From' header from 'X-Original-To' header
bool          C_ReverseName;               ///< Config: Set the 'From' from the address the email was sent to
bool          C_ReverseRealname;           ///< Config: Set the 'From' from the full 'To' address the email was sent to
char *        C_Sendmail;                  ///< Config: External command to send email
short         C_SendmailWait;              ///< Config: Time to wait for sendmail to finish
bool          C_SigDashes;                 ///< Config: Insert '-- ' before the signature
char *        C_Signature;                 ///< Config: File containing a signature to append to all mail
bool          C_SigOnTop;                  ///< Config: Insert the signature before the quoted text
struct Slist *C_SmtpAuthenticators;        ///< Config: (smtp) List of allowed authentication methods
char *        C_SmtpOauthRefreshCommand;   ///< Config: (smtp) External command to generate OAUTH refresh token
char *        C_SmtpPass;                  ///< Config: (smtp) Password for the SMTP server
char *        C_SmtpUrl;                   ///< Config: (smtp) Url of the SMTP server
char *        C_SmtpUser;                  ///< Config: (smtp) Username for the SMTP server
bool          C_Use8bitmime;               ///< Config: Use 8-bit messages and ESMTP to send messages
bool          C_UseEnvelopeFrom;           ///< Config: Set the envelope sender of the message
bool          C_UseFrom;                   ///< Config: Set the 'From' header for outgoing mail
bool          C_UserAgent;                 ///< Config: Add a 'User-Agent' head to outgoing mail
short         C_WrapHeaders;               ///< Config: Width to wrap headers in outgoing messages
// clang-format off

