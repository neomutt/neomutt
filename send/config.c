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
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "init.h"

// clang-format off
unsigned char C_AbortNoattach;                    ///< Config: Abort sending the email if attachments are missing
struct Regex *C_AbortNoattachRegex = NULL;        ///< Config: Regex to match text indicating attachments are expected
unsigned char C_AbortNosubject;                   ///< Config: Abort creating the email if subject is missing
unsigned char C_AbortUnmodified;                  ///< Config: Abort the sending if the message hasn't been edited
bool          C_Allow8bit;                        ///< Config: Allow 8-bit messages, don't use quoted-printable or base64
bool          C_AskFollowUp;                      ///< Config: (nntp) Ask the user for follow-up groups before editing
bool          C_AskXCommentTo;                    ///< Config: (nntp) Ask the user for the 'X-Comment-To' field before editing
char *        C_AttachCharset = NULL;             ///< Config: When attaching files, use one of these character sets
bool          C_BounceDelivered;                  ///< Config: Add 'Delivered-To' to bounced messages
char *        C_ContentType = NULL;               ///< Config: Default "Content-Type" for newly composed messages
bool          C_CryptAutoencrypt;                 ///< Config: Automatically PGP encrypt all outgoing mail
bool          C_CryptAutopgp;                     ///< Config: Allow automatic PGP functions
bool          C_CryptAutosign;                    ///< Config: Automatically PGP sign all outgoing mail
bool          C_CryptAutosmime;                   ///< Config: Allow automatic SMIME functions
bool          C_CryptReplyencrypt;                ///< Config: Encrypt replies to encrypted messages
bool          C_CryptReplysign;                   ///< Config: Sign replies to signed messages
bool          C_CryptReplysignencrypted;          ///< Config: Sign replies to encrypted messages
char *        C_DsnNotify = NULL;                 ///< Config: Request notification for message delivery or delay
char *        C_DsnReturn = NULL;                 ///< Config: What to send as a notification of message delivery or delay
char *        C_EmptySubject = NULL;              ///< Config: Subject to use when replying to an email with none
bool          C_EncodeFrom;                       ///< Config: Encode 'From ' as 'quote-printable' at the beginning of lines
bool          C_FastReply;                        ///< Config: Don't prompt for the recipients and subject when replying/forwarding
unsigned char C_FccAttach;                        ///< Config: Save send message with all their attachments
bool          C_FccBeforeSend;                    ///< Config: Save FCCs before sending the message
bool          C_FccClear;                         ///< Config: Save sent messages unencrypted and unsigned
bool          C_FollowupTo;                       ///< Config: Add the 'Mail-Followup-To' header is generated when sending mail
char *        C_ForwardAttributionIntro = NULL;   ///< Config: Prefix message for forwarded messages
char *        C_ForwardAttributionTrailer = NULL; ///< Config: Suffix message for forwarded messages
bool          C_ForwardDecrypt;                   ///< Config: Decrypt the message when forwarding it
unsigned char C_ForwardEdit;                      ///< Config: Automatically start the editor when forwarding a message
char *        C_ForwardFormat = NULL;             ///< Config: printf-like format string to control the subject when forwarding a message
bool          C_ForwardReferences;                ///< Config: Set the 'In-Reply-To' and 'References' headers when forwarding a message
bool          C_Hdrs;                             ///< Config: Add custom headers to outgoing mail
bool          C_HiddenHost;                       ///< Config: Don't use the hostname, just the domain, when generating the message id
unsigned char C_HonorFollowupTo;                  ///< Config: Honour the 'Mail-Followup-To' header when group replying
bool          C_IgnoreListReplyTo;                ///< Config: Ignore the 'Reply-To' header when using `<reply>` on a mailing list
unsigned char C_Include;                          ///< Config: Include a copy of the email that's being replied to
char *        C_Inews = NULL;                     ///< Config: (nntp) External command to post news articles
bool          C_Metoo;                            ///< Config: Remove the user's address from the list of recipients
bool          C_MimeForwardDecode;                ///< Config: Decode the forwarded message before attaching it
bool          C_MimeSubject;                      ///< Config: (nntp) Encode the article subject in base64
char *        C_MimeTypeQueryCommand = NULL;      ///< Config: External command to determine the MIME type of an attachment
bool          C_MimeTypeQueryFirst;               ///< Config: Run the #C_MimeTypeQueryCommand before the mime.types lookup
bool          C_NmRecord;                         ///< Config: (notmuch) If the 'record' mailbox (sent mail) should be indexed
bool          C_PgpReplyinline;                   ///< Config: Reply using old-style inline PGP messages (not recommended)
char *        C_PostIndentString = NULL;          ///< Config: Suffix message to add after reply text
char *        C_PostponeEncryptAs = NULL;         ///< Config: Fallback encryption key for postponed messages
bool          C_PostponeEncrypt;                  ///< Config: Self-encrypt postponed messages
unsigned char C_Recall;                           ///< Config: Recall postponed mesaages when asked to compose a message
bool          C_ReplySelf;                        ///< Config: Really reply to yourself, when replying to your own email
unsigned char C_ReplyTo;                          ///< Config: Address to use as a 'Reply-To' header
bool          C_ReplyWithXorig;                   ///< Config: Create 'From' header from 'X-Original-To' header
bool          C_ReverseName;                      ///< Config: Set the 'From' from the address the email was sent to
bool          C_ReverseRealname;                  ///< Config: Set the 'From' from the full 'To' address the email was sent to
char *        C_Sendmail = NULL;                  ///< Config: External command to send email
short         C_SendmailWait;                     ///< Config: Time to wait for sendmail to finish
bool          C_SigDashes;                        ///< Config: Insert '-- ' before the signature
char *        C_Signature = NULL;                 ///< Config: File containing a signature to append to all mail
bool          C_SigOnTop;                         ///< Config: Insert the signature before the quoted text
struct Slist *C_SmtpAuthenticators = NULL;        ///< Config: (smtp) List of allowed authentication methods
char *        C_SmtpOauthRefreshCommand = NULL;   ///< Config: (smtp) External command to generate OAUTH refresh token
char *        C_SmtpPass = NULL;                  ///< Config: (smtp) Password for the SMTP server
char *        C_SmtpUrl = NULL;                   ///< Config: (smtp) Url of the SMTP server
char *        C_SmtpUser = NULL;                  ///< Config: (smtp) Username for the SMTP server
bool          C_Use8bitmime;                      ///< Config: Use 8-bit messages and ESMTP to send messages
bool          C_UseEnvelopeFrom;                  ///< Config: Set the envelope sender of the message
bool          C_UseFrom;                          ///< Config: Set the 'From' header for outgoing mail
bool          C_UserAgent;                        ///< Config: Add a 'User-Agent' head to outgoing mail
short         C_WrapHeaders;                      ///< Config: Width to wrap headers in outgoing messages
// clang-format on

/**
 * wrapheaders_validator - Validate the "wrap_headers" config variable - Implements ConfigDef::validator()
 */
int wrapheaders_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                          intptr_t value, struct Buffer *err)
{
  const int min_length = 78; // Recommendations from RFC5233
  const int max_length = 998;

  if ((value >= min_length) && (value <= max_length))
    return CSR_SUCCESS;

  // L10N: This applies to the "$wrap_headers" config variable.
  mutt_buffer_printf(err, _("Option %s must be between %d and %d inclusive"),
                     cdef->name, min_length, max_length);
  return CSR_ERR_INVALID;
}

// clang-format off
struct ConfigDef SendVars[] = {
  { "abort_noattach",              DT_QUAD,                           &C_AbortNoattach,             MUTT_NO },
  { "abort_noattach_regex",        DT_REGEX,                          &C_AbortNoattachRegex,        IP "\\<(attach|attached|attachments?)\\>" },
  { "abort_nosubject",             DT_QUAD,                           &C_AbortNosubject,            MUTT_ASKYES },
  { "abort_unmodified",            DT_QUAD,                           &C_AbortUnmodified,           MUTT_YES },
  { "allow_8bit",                  DT_BOOL,                           &C_Allow8bit,                 true },
  { "ask_follow_up",               DT_BOOL,                           &C_AskFollowUp,               false },
#ifdef USE_NNTP
  { "ask_x_comment_to",            DT_BOOL,                           &C_AskXCommentTo,             false },
#endif
  { "attach_charset",              DT_STRING,                         &C_AttachCharset,             0, 0, charset_validator },
  { "bounce_delivered",            DT_BOOL,                           &C_BounceDelivered,           true },
  { "content_type",                DT_STRING,                         &C_ContentType,               IP "text/plain" },
  { "crypt_autoencrypt",           DT_BOOL,                           &C_CryptAutoencrypt,          false },
  { "crypt_autopgp",               DT_BOOL,                           &C_CryptAutopgp,              true },
  { "crypt_autosign",              DT_BOOL,                           &C_CryptAutosign,             false },
  { "crypt_autosmime",             DT_BOOL,                           &C_CryptAutosmime,            true },
  { "crypt_replyencrypt",          DT_BOOL,                           &C_CryptReplyencrypt,         true },
  { "crypt_replysign",             DT_BOOL,                           &C_CryptReplysign,            false },
  { "crypt_replysignencrypted",    DT_BOOL,                           &C_CryptReplysignencrypted,   false },
  { "dsn_notify",                  DT_STRING,                         &C_DsnNotify,                 0 },
  { "dsn_return",                  DT_STRING,                         &C_DsnReturn,                 0 },
  { "empty_subject",               DT_STRING,                         &C_EmptySubject,              IP "Re: your mail" },
  { "encode_from",                 DT_BOOL,                           &C_EncodeFrom,                false },
  { "fast_reply",                  DT_BOOL,                           &C_FastReply,                 false },
  { "fcc_attach",                  DT_QUAD,                           &C_FccAttach,                 MUTT_YES },
  { "fcc_before_send",             DT_BOOL,                           &C_FccBeforeSend,             false },
  { "fcc_clear",                   DT_BOOL,                           &C_FccClear,                  false },
  { "followup_to",                 DT_BOOL,                           &C_FollowupTo,                true },
  { "forward_attribution_intro",   DT_STRING,                         &C_ForwardAttributionIntro,   IP "----- Forwarded message from %f -----" },
  { "forward_attribution_trailer", DT_STRING,                         &C_ForwardAttributionTrailer, IP "----- End forwarded message -----" },
  { "forward_decrypt",             DT_BOOL,                           &C_ForwardDecrypt,            true },
  { "forward_edit",                DT_QUAD,                           &C_ForwardEdit,               MUTT_YES },
  { "forward_format",              DT_STRING|DT_NOT_EMPTY,            &C_ForwardFormat,             IP "[%a: %s]" },
  { "forward_references",          DT_BOOL,                           &C_ForwardReferences,         false },
  { "hdrs",                        DT_BOOL,                           &C_Hdrs,                      true },
  { "hidden_host",                 DT_BOOL,                           &C_HiddenHost,                false },
  { "honor_followup_to",           DT_QUAD,                           &C_HonorFollowupTo,           MUTT_YES },
  { "ignore_list_reply_to",        DT_BOOL,                           &C_IgnoreListReplyTo,         false },
  { "include",                     DT_QUAD,                           &C_Include,                   MUTT_ASKYES },
#ifdef USE_NNTP
  { "inews",                       DT_STRING|DT_COMMAND,              &C_Inews,                     0 },
#endif
  { "metoo",                       DT_BOOL,                           &C_Metoo,                     false },
  { "mime_forward_decode",         DT_BOOL,                           &C_MimeForwardDecode,         false },
#ifdef USE_NNTP
  { "mime_subject",                DT_BOOL,                           &C_MimeSubject,               true },
#endif
  { "mime_type_query_command",     DT_STRING|DT_COMMAND,              &C_MimeTypeQueryCommand,      0 },
  { "mime_type_query_first",       DT_BOOL,                           &C_MimeTypeQueryFirst,        false },
  { "nm_record",                   DT_BOOL,                           &C_NmRecord,                  false },
  { "pgp_replyinline",             DT_BOOL,                           &C_PgpReplyinline,            false },
  { "post_indent_string",          DT_STRING,                         &C_PostIndentString,          0 },
  { "postpone_encrypt",            DT_BOOL,                           &C_PostponeEncrypt,           false },
  { "postpone_encrypt_as",         DT_STRING,                         &C_PostponeEncryptAs,         0 },
  { "recall",                      DT_QUAD,                           &C_Recall,                    MUTT_ASKYES },
  { "reply_self",                  DT_BOOL,                           &C_ReplySelf,                 false },
  { "reply_to",                    DT_QUAD,                           &C_ReplyTo,                   MUTT_ASKYES },
  { "reply_with_xorig",            DT_BOOL,                           &C_ReplyWithXorig,            false },
  { "reverse_name",                DT_BOOL|R_INDEX|R_PAGER,           &C_ReverseName,               false },
  { "reverse_realname",            DT_BOOL|R_INDEX|R_PAGER,           &C_ReverseRealname,           true },
  { "sendmail",                    DT_STRING|DT_COMMAND,              &C_Sendmail,                  IP SENDMAIL " -oem -oi" },
  { "sendmail_wait",               DT_NUMBER,                         &C_SendmailWait,              0 },
  { "sig_dashes",                  DT_BOOL,                           &C_SigDashes,                 true },
  { "sig_on_top",                  DT_BOOL,                           &C_SigOnTop,                  false },
  { "signature",                   DT_PATH|DT_PATH_FILE,              &C_Signature,                 IP "~/.signature" },
#ifdef USE_SMTP
  { "smtp_authenticators",         DT_SLIST|SLIST_SEP_COLON,          &C_SmtpAuthenticators,        0 },
  { "smtp_oauth_refresh_command",  DT_STRING|DT_COMMAND|DT_SENSITIVE, &C_SmtpOauthRefreshCommand,   0 },
  { "smtp_pass",                   DT_STRING|DT_SENSITIVE,            &C_SmtpPass,                  0 },
  { "smtp_user",                   DT_STRING|DT_SENSITIVE,            &C_SmtpUser,                  0 },
  { "smtp_url",                    DT_STRING|DT_SENSITIVE,            &C_SmtpUrl,                   0 },
#endif
  { "use_8bitmime",                DT_BOOL,                           &C_Use8bitmime,               false },
  { "use_envelope_from",           DT_BOOL,                           &C_UseEnvelopeFrom,           false },
  { "use_from",                    DT_BOOL,                           &C_UseFrom,                   true },
  { "user_agent",                  DT_BOOL,                           &C_UserAgent,                 false },
  { "wrap_headers",                DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER, &C_WrapHeaders,               78, 0, wrapheaders_validator },

  { "abort_noattach_regexp",  DT_SYNONYM, NULL, IP "abort_noattach_regex",     },
  { "attach_keyword",         DT_SYNONYM, NULL, IP "abort_noattach_regex",     },
  { "envelope_from",          DT_SYNONYM, NULL, IP "use_envelope_from",        },
  { "forw_decrypt",           DT_SYNONYM, NULL, IP "forward_decrypt",          },
  { "forw_format",            DT_SYNONYM, NULL, IP "forward_format",           },
  { "pgp_autoencrypt",        DT_SYNONYM, NULL, IP "crypt_autoencrypt",        },
  { "pgp_autosign",           DT_SYNONYM, NULL, IP "crypt_autosign",           },
  { "pgp_auto_traditional",   DT_SYNONYM, NULL, IP "pgp_replyinline",          },
  { "pgp_replyencrypt",       DT_SYNONYM, NULL, IP "crypt_replyencrypt",       },
  { "pgp_replysign",          DT_SYNONYM, NULL, IP "crypt_replysign",          },
  { "pgp_replysignencrypted", DT_SYNONYM, NULL, IP "crypt_replysignencrypted", },
  { "post_indent_str",        DT_SYNONYM, NULL, IP "post_indent_string",       },
  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_send - Register send config variables
 */
bool config_init_send(struct ConfigSet *cs)
{
  return cs_register_variables(cs, SendVars, 0);
}
