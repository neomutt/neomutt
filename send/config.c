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
#include "conn/lib.h"
#include "lib.h"
#include "init.h"

bool config_init_send(struct ConfigSet *cs);

/**
 * wrapheaders_validator - Validate the "wrap_headers" config variable - Implements ConfigDef::validator()
 */
static int wrapheaders_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
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

/**
 * smtp_auth_validator - Validate the "smtp_authenticators" config variable - Implements ConfigDef::validator()
 */
static int smtp_auth_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                               intptr_t value, struct Buffer *err)
{
  const struct Slist *smtp_auth_methods = (const struct Slist *) value;
  if (!smtp_auth_methods || (smtp_auth_methods->count == 0))
    return CSR_SUCCESS;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &smtp_auth_methods->head, entries)
  {
    if (smtp_auth_is_valid(np->data))
      continue;
#ifdef USE_SASL
    if (sasl_auth_validator(np->data))
      continue;
#endif
    mutt_buffer_printf(err, _("Option %s: %s is not a valid authenticator"),
                       cdef->name, np->data);
    return CSR_ERR_INVALID;
  }

  return CSR_SUCCESS;
}

struct ConfigDef SendVars[] = {
  // clang-format off
  { "abort_noattach", DT_QUAD, NULL, MUTT_NO, 0, NULL,
    "Abort sending the email if attachments are missing"
  },
  { "abort_noattach_regex", DT_REGEX, NULL, IP "\\<(attach|attached|attachments?)\\>", 0, NULL,
    "Regex to match text indicating attachments are expected"
  },
  { "abort_nosubject", DT_QUAD, NULL, MUTT_ASKYES, 0, NULL,
    "Abort creating the email if subject is missing"
  },
  { "abort_unmodified", DT_QUAD, NULL, MUTT_YES, 0, NULL,
    "Abort the sending if the message hasn't been edited"
  },
  { "allow_8bit", DT_BOOL, NULL, true, 0, NULL,
    "Allow 8-bit messages, don't use quoted-printable or base64"
  },
  { "ask_follow_up", DT_BOOL, NULL, false, 0, NULL,
    "(nntp) Ask the user for follow-up groups before editing"
  },
#ifdef USE_NNTP
  { "ask_x_comment_to", DT_BOOL, NULL, false, 0, NULL,
    "(nntp) Ask the user for the 'X-Comment-To' field before editing"
  },
#endif
  { "attach_charset", DT_STRING, NULL, 0, 0, charset_validator,
    "When attaching files, use one of these character sets"
  },
  { "bounce_delivered", DT_BOOL, NULL, true, 0, NULL,
    "Add 'Delivered-To' to bounced messages"
  },
  { "content_type", DT_STRING, NULL, IP "text/plain", 0, NULL,
    "Default 'Content-Type' for newly composed messages"
  },
  { "crypt_autoencrypt", DT_BOOL, NULL, false, 0, NULL,
    "Automatically PGP encrypt all outgoing mail"
  },
  { "crypt_autopgp", DT_BOOL, NULL, true, 0, NULL,
    "Allow automatic PGP functions"
  },
  { "crypt_autosign", DT_BOOL, NULL, false, 0, NULL,
    "Automatically PGP sign all outgoing mail"
  },
  { "crypt_autosmime", DT_BOOL, NULL, true, 0, NULL,
    "Allow automatic SMIME functions"
  },
  { "crypt_replyencrypt", DT_BOOL, NULL, true, 0, NULL,
    "Encrypt replies to encrypted messages"
  },
  { "crypt_replysign", DT_BOOL, NULL, false, 0, NULL,
    "Sign replies to signed messages"
  },
  { "crypt_replysignencrypted", DT_BOOL, NULL, false, 0, NULL,
    "Sign replies to encrypted messages"
  },
  { "dsn_notify", DT_STRING, NULL, 0, 0, NULL,
    "Request notification for message delivery or delay"
  },
  { "dsn_return", DT_STRING, NULL, 0, 0, NULL,
    "What to send as a notification of message delivery or delay"
  },
  { "empty_subject", DT_STRING, NULL, IP "Re: your mail", 0, NULL,
    "Subject to use when replying to an email with none"
  },
  { "encode_from", DT_BOOL, NULL, false, 0, NULL,
    "Encode 'From ' as 'quote-printable' at the beginning of lines"
  },
  { "fast_reply", DT_BOOL, NULL, false, 0, NULL,
    "Don't prompt for the recipients and subject when replying/forwarding"
  },
  { "fcc_attach", DT_QUAD, NULL, MUTT_YES, 0, NULL,
    "Save send message with all their attachments"
  },
  { "fcc_before_send", DT_BOOL, NULL, false, 0, NULL,
    "Save FCCs before sending the message"
  },
  { "fcc_clear", DT_BOOL, NULL, false, 0, NULL,
    "Save sent messages unencrypted and unsigned"
  },
  { "followup_to", DT_BOOL, NULL, true, 0, NULL,
    "Add the 'Mail-Followup-To' header is generated when sending mail"
  },
  { "forward_attribution_intro", DT_STRING, NULL, IP "----- Forwarded message from %f -----", 0, NULL,
    "Prefix message for forwarded messages"
  },
  { "forward_attribution_trailer", DT_STRING, NULL, IP "----- End forwarded message -----", 0, NULL,
    "Suffix message for forwarded messages"
  },
  { "forward_decrypt", DT_BOOL, NULL, true, 0, NULL,
    "Decrypt the message when forwarding it"
  },
  { "forward_edit", DT_QUAD, NULL, MUTT_YES, 0, NULL,
    "Automatically start the editor when forwarding a message"
  },
  { "forward_format", DT_STRING|DT_NOT_EMPTY, NULL, IP "[%a: %s]", 0, NULL,
    "printf-like format string to control the subject when forwarding a message"
  },
  { "forward_references", DT_BOOL, NULL, false, 0, NULL,
    "Set the 'In-Reply-To' and 'References' headers when forwarding a message"
  },
  { "hdrs", DT_BOOL, NULL, true, 0, NULL,
    "Add custom headers to outgoing mail"
  },
  { "hidden_host", DT_BOOL, NULL, false, 0, NULL,
    "Don't use the hostname, just the domain, when generating the message id"
  },
  { "honor_followup_to", DT_QUAD, NULL, MUTT_YES, 0, NULL,
    "Honour the 'Mail-Followup-To' header when group replying"
  },
  { "ignore_list_reply_to", DT_BOOL, NULL, false, 0, NULL,
    "Ignore the 'Reply-To' header when using `<reply>` on a mailing list"
  },
  { "include", DT_QUAD, NULL, MUTT_ASKYES, 0, NULL,
    "Include a copy of the email that's being replied to"
  },
#ifdef USE_NNTP
  { "inews", DT_STRING|DT_COMMAND, NULL, 0, 0, NULL,
    "(nntp) External command to post news articles"
  },
#endif
  { "metoo", DT_BOOL, NULL, false, 0, NULL,
    "Remove the user's address from the list of recipients"
  },
  { "mime_forward_decode", DT_BOOL, NULL, false, 0, NULL,
    "Decode the forwarded message before attaching it"
  },
#ifdef USE_NNTP
  { "mime_subject", DT_BOOL, NULL, true, 0, NULL,
    "(nntp) Encode the article subject in base64"
  },
#endif
  { "mime_type_query_command", DT_STRING|DT_COMMAND, NULL, 0, 0, NULL,
    "External command to determine the MIME type of an attachment"
  },
  { "mime_type_query_first", DT_BOOL, NULL, false, 0, NULL,
    "Run the #C_MimeTypeQueryCommand before the mime.types lookup"
  },
  { "nm_record", DT_BOOL, NULL, false, 0, NULL,
    "(notmuch) If the 'record' mailbox (sent mail) should be indexed"
  },
  { "pgp_replyinline", DT_BOOL, NULL, false, 0, NULL,
    "Reply using old-style inline PGP messages (not recommended)"
  },
  { "post_indent_string", DT_STRING, NULL, 0, 0, NULL,
    "Suffix message to add after reply text"
  },
  { "postpone_encrypt", DT_BOOL, NULL, false, 0, NULL,
    "Self-encrypt postponed messages"
  },
  { "postpone_encrypt_as", DT_STRING, NULL, 0, 0, NULL,
    "Fallback encryption key for postponed messages"
  },
  { "recall", DT_QUAD, NULL, MUTT_ASKYES, 0, NULL,
    "Recall postponed mesaages when asked to compose a message"
  },
  { "reply_self", DT_BOOL, NULL, false, 0, NULL,
    "Really reply to yourself, when replying to your own email"
  },
  { "reply_to", DT_QUAD, NULL, MUTT_ASKYES, 0, NULL,
    "Address to use as a 'Reply-To' header"
  },
  { "reply_with_xorig", DT_BOOL, NULL, false, 0, NULL,
    "Create 'From' header from 'X-Original-To' header"
  },
  { "reverse_name", DT_BOOL|R_INDEX|R_PAGER, NULL, false, 0, NULL,
    "Set the 'From' from the address the email was sent to"
  },
  { "reverse_realname", DT_BOOL|R_INDEX|R_PAGER, NULL, true, 0, NULL,
    "Set the 'From' from the full 'To' address the email was sent to"
  },
  { "sendmail", DT_STRING|DT_COMMAND, NULL, IP SENDMAIL " -oem -oi", 0, NULL,
    "External command to send email"
  },
  { "sendmail_wait", DT_NUMBER, NULL, 0, 0, NULL,
    "Time to wait for sendmail to finish"
  },
  { "sig_dashes", DT_BOOL, NULL, true, 0, NULL,
    "Insert '-- ' before the signature"
  },
  { "sig_on_top", DT_BOOL, NULL, false, 0, NULL,
    "Insert the signature before the quoted text"
  },
  { "signature", DT_PATH|DT_PATH_FILE, NULL, IP "~/.signature", 0, NULL,
    "File containing a signature to append to all mail"
  },
#ifdef USE_SMTP
  { "smtp_authenticators", DT_SLIST|SLIST_SEP_COLON, NULL, 0, 0, smtp_auth_validator,
    "(smtp) List of allowed authentication methods"
  },
  { "smtp_oauth_refresh_command", DT_STRING|DT_COMMAND|DT_SENSITIVE, NULL, 0, 0, NULL,
    "(smtp) External command to generate OAUTH refresh token"
  },
  { "smtp_pass", DT_STRING|DT_SENSITIVE, NULL, 0, 0, NULL,
    "(smtp) Password for the SMTP server"
  },
  { "smtp_user", DT_STRING|DT_SENSITIVE, NULL, 0, 0, NULL,
    "(smtp) Username for the SMTP server"
  },
  { "smtp_url", DT_STRING|DT_SENSITIVE, NULL, 0, 0, NULL,
    "(smtp) Url of the SMTP server"
  },
#endif
  { "use_8bitmime", DT_BOOL, NULL, false, 0, NULL,
    "Use 8-bit messages and ESMTP to send messages"
  },
  { "use_envelope_from", DT_BOOL, NULL, false, 0, NULL,
    "Set the envelope sender of the message"
  },
  { "use_from", DT_BOOL, NULL, true, 0, NULL,
    "Set the 'From' header for outgoing mail"
  },
  { "user_agent", DT_BOOL, NULL, false, 0, NULL,
    "Add a 'User-Agent' head to outgoing mail"
  },
  { "wrap_headers", DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER, NULL, 78, 0, wrapheaders_validator,
    "Width to wrap headers in outgoing messages"
  },

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
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_send - Register send config variables - Implements ::module_init_config_t
 */
bool config_init_send(struct ConfigSet *cs)
{
  return cs_register_variables(cs, SendVars, DT_NO_VARIABLE);
}
