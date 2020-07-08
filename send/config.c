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
  { "abort_noattach",              DT_QUAD,                           NULL, MUTT_NO },
  { "abort_noattach_regex",        DT_REGEX,                          NULL, IP "\\<(attach|attached|attachments?)\\>" },
  { "abort_nosubject",             DT_QUAD,                           NULL, MUTT_ASKYES },
  { "abort_unmodified",            DT_QUAD,                           NULL, MUTT_YES },
  { "allow_8bit",                  DT_BOOL,                           NULL, true },
  { "ask_follow_up",               DT_BOOL,                           NULL, false },
#ifdef USE_NNTP
  { "ask_x_comment_to",            DT_BOOL,                           NULL, false },
#endif
  { "attach_charset",              DT_STRING,                         NULL, 0, 0, charset_validator },
  { "bounce_delivered",            DT_BOOL,                           NULL, true },
  { "content_type",                DT_STRING,                         NULL, IP "text/plain" },
  { "crypt_autoencrypt",           DT_BOOL,                           NULL, false },
  { "crypt_autopgp",               DT_BOOL,                           NULL, true },
  { "crypt_autosign",              DT_BOOL,                           NULL, false },
  { "crypt_autosmime",             DT_BOOL,                           NULL, true },
  { "crypt_replyencrypt",          DT_BOOL,                           NULL, true },
  { "crypt_replysign",             DT_BOOL,                           NULL, false },
  { "crypt_replysignencrypted",    DT_BOOL,                           NULL, false },
  { "dsn_notify",                  DT_STRING,                         NULL, 0 },
  { "dsn_return",                  DT_STRING,                         NULL, 0 },
  { "empty_subject",               DT_STRING,                         NULL, IP "Re: your mail" },
  { "encode_from",                 DT_BOOL,                           NULL, false },
  { "fast_reply",                  DT_BOOL,                           NULL, false },
  { "fcc_attach",                  DT_QUAD,                           NULL, MUTT_YES },
  { "fcc_before_send",             DT_BOOL,                           NULL, false },
  { "fcc_clear",                   DT_BOOL,                           NULL, false },
  { "followup_to",                 DT_BOOL,                           NULL, true },
  { "forward_attribution_intro",   DT_STRING,                         NULL, IP "----- Forwarded message from %f -----" },
  { "forward_attribution_trailer", DT_STRING,                         NULL, IP "----- End forwarded message -----" },
  { "forward_decrypt",             DT_BOOL,                           NULL, true },
  { "forward_edit",                DT_QUAD,                           NULL, MUTT_YES },
  { "forward_format",              DT_STRING|DT_NOT_EMPTY,            NULL, IP "[%a: %s]" },
  { "forward_references",          DT_BOOL,                           NULL, false },
  { "hdrs",                        DT_BOOL,                           NULL, true },
  { "hidden_host",                 DT_BOOL,                           NULL, false },
  { "honor_followup_to",           DT_QUAD,                           NULL, MUTT_YES },
  { "ignore_list_reply_to",        DT_BOOL,                           NULL, false },
  { "include",                     DT_QUAD,                           NULL, MUTT_ASKYES },
#ifdef USE_NNTP
  { "inews",                       DT_STRING|DT_COMMAND,              NULL, 0 },
#endif
  { "metoo",                       DT_BOOL,                           NULL, false },
  { "mime_forward_decode",         DT_BOOL,                           NULL, false },
#ifdef USE_NNTP
  { "mime_subject",                DT_BOOL,                           NULL, true },
#endif
  { "mime_type_query_command",     DT_STRING|DT_COMMAND,              NULL, 0 },
  { "mime_type_query_first",       DT_BOOL,                           NULL, false },
  { "nm_record",                   DT_BOOL,                           NULL, false },
  { "pgp_replyinline",             DT_BOOL,                           NULL, false },
  { "post_indent_string",          DT_STRING,                         NULL, 0 },
  { "postpone_encrypt",            DT_BOOL,                           NULL, false },
  { "postpone_encrypt_as",         DT_STRING,                         NULL, 0 },
  { "recall",                      DT_QUAD,                           NULL, MUTT_ASKYES },
  { "reply_self",                  DT_BOOL,                           NULL, false },
  { "reply_to",                    DT_QUAD,                           NULL, MUTT_ASKYES },
  { "reply_with_xorig",            DT_BOOL,                           NULL, false },
  { "reverse_name",                DT_BOOL|R_INDEX|R_PAGER,           NULL, false },
  { "reverse_realname",            DT_BOOL|R_INDEX|R_PAGER,           NULL, true },
  { "sendmail",                    DT_STRING|DT_COMMAND,              NULL, IP SENDMAIL " -oem -oi" },
  { "sendmail_wait",               DT_NUMBER,                         NULL, 0 },
  { "sig_dashes",                  DT_BOOL,                           NULL, true },
  { "sig_on_top",                  DT_BOOL,                           NULL, false },
  { "signature",                   DT_PATH|DT_PATH_FILE,              NULL, IP "~/.signature" },
#ifdef USE_SMTP
  { "smtp_authenticators",         DT_SLIST|SLIST_SEP_COLON,          NULL, 0 },
  { "smtp_oauth_refresh_command",  DT_STRING|DT_COMMAND|DT_SENSITIVE, NULL, 0 },
  { "smtp_pass",                   DT_STRING|DT_SENSITIVE,            NULL, 0 },
  { "smtp_user",                   DT_STRING|DT_SENSITIVE,            NULL, 0 },
  { "smtp_url",                    DT_STRING|DT_SENSITIVE,            NULL, 0 },
#endif
  { "use_8bitmime",                DT_BOOL,                           NULL, false },
  { "use_envelope_from",           DT_BOOL,                           NULL, false },
  { "use_from",                    DT_BOOL,                           NULL, true },
  { "user_agent",                  DT_BOOL,                           NULL, false },
  { "wrap_headers",                DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER, NULL, 78, 0, wrapheaders_validator },

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
  return cs_register_variables(cs, SendVars, DT_NO_VARIABLE);
}
