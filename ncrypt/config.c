/**
 * @file
 * Config used by libncrypt
 *
 * @authors
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * @page crypt_config Config used by libncrypt
 *
 * Config used by libncrypt
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "expando/lib.h"
#include "pgp.h"
#include "pgplib.h"
#include "smime.h"
#include "sort.h"

/**
 * KeySortMethods - Sort methods for encryption keys
 */
static const struct Mapping KeySortMethods[] = {
  // clang-format off
  { "address", KEY_SORT_ADDRESS },
  { "date",    KEY_SORT_DATE },
  { "keyid",   KEY_SORT_KEYID },
  { "trust",   KEY_SORT_TRUST },
  { NULL, 0 },
  // clang-format on
};

/**
 * parse_pgp_date - Parse a Date Expando - Implements ExpandoDefinition::parse() - @ingroup expando_parse_api
 *
 * Parse a custom Expando of the form, "%[string]".
 * The "string" will be passed to strftime().
 */
struct ExpandoNode *parse_pgp_date(const char *str, struct ExpandoFormat *fmt,
                                   int did, int uid, ExpandoParserFlags flags,
                                   const char **parsed_until, struct ExpandoParseError *err)
{
  if (flags & EP_CONDITIONAL)
  {
    return node_conddate_parse(str, did, uid, parsed_until, err);
  }

  return node_expando_parse_enclosure(str, did, uid, ']', fmt, parsed_until, err);
}

/**
 * PgpEntryFormatDef - Expando definitions
 *
 * Config:
 * - $pgp_entry_format
 */
static const struct ExpandoDefinition PgpEntryFormatDef[] = {
  // clang-format off
  { "*", "padding-soft",      ED_GLOBAL,  ED_GLO_PADDING_SOFT,      node_padding_parse },
  { ">", "padding-hard",      ED_GLOBAL,  ED_GLO_PADDING_HARD,      node_padding_parse },
  { "|", "padding-eol",       ED_GLOBAL,  ED_GLO_PADDING_EOL,       node_padding_parse },
  { "a", "key-algorithm",     ED_PGP_KEY, ED_PGK_KEY_ALGORITHM,     NULL },
  { "A", "pkey-algorithm",    ED_PGP_KEY, ED_PGK_PKEY_ALGORITHM,    NULL },
  { "c", "key-capabilities",  ED_PGP_KEY, ED_PGK_KEY_CAPABILITIES,  NULL },
  { "C", "pkey-capabilities", ED_PGP_KEY, ED_PGK_PKEY_CAPABILITIES, NULL },
  { "f", "key-flags",         ED_PGP_KEY, ED_PGK_KEY_FLAGS,         NULL },
  { "F", "pkey-flags",        ED_PGP_KEY, ED_PGK_PKEY_FLAGS,        NULL },
  { "i", "key-fingerprint",   ED_PGP_KEY, ED_PGK_KEY_FINGERPRINT,   NULL },
  { "I", "pkey-fingerprint",  ED_PGP_KEY, ED_PGK_PKEY_FINGERPRINT,  NULL },
  { "k", "key-id",            ED_PGP_KEY, ED_PGK_KEY_ID,            NULL },
  { "K", "pkey-id",           ED_PGP_KEY, ED_PGK_PKEY_ID,           NULL },
  { "l", "key-length",        ED_PGP_KEY, ED_PGK_KEY_LENGTH,        NULL },
  { "L", "pkey-length",       ED_PGP_KEY, ED_PGK_PKEY_LENGTH,       NULL },
  { "n", "number",            ED_PGP,     ED_PGP_NUMBER,            NULL },
  { "p", "protocol",          ED_PGP_KEY, ED_PGK_PROTOCOL,          NULL },
  { "t", "trust",             ED_PGP,     ED_PGP_TRUST,             NULL },
  { "u", "user-id",           ED_PGP,     ED_PGP_USER_ID,           NULL },
  { "[", "date",              ED_PGP_KEY, ED_PGK_DATE,              parse_pgp_date },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * NcryptVars - Config definitions for the encryption library
 */
struct ConfigDef NcryptVars[] = {
  // clang-format off
  { "crypt_confirm_hook", DT_BOOL, true, 0, NULL,
    "Prompt the user to confirm keys before use"
  },
  { "crypt_opportunistic_encrypt", DT_BOOL, false, 0, NULL,
    "Enable encryption when the recipient's key is available"
  },
  { "crypt_opportunistic_encrypt_strong_keys", DT_BOOL, false, 0, NULL,
    "Enable encryption only when strong a key is available"
  },
  { "crypt_protected_headers_read", DT_BOOL, true, 0, NULL,
    "Display protected headers (Memory Hole) in the pager"
  },
  { "crypt_protected_headers_subject", DT_STRING, IP "...", 0, NULL,
    "Use this as the subject for encrypted emails"
  },
  { "crypt_protected_headers_weed", DT_BOOL, false, 0, NULL,
    "Controls whether NeoMutt will weed protected header fields"
  },
  { "crypt_protected_headers_write", DT_BOOL, true, 0, NULL,
    "Generate protected header (Memory Hole) for signed and encrypted emails"
  },
  { "crypt_encryption_info", DT_BOOL, true, 0, NULL,
    "Add an informative block with details about the encryption"
  },
  { "crypt_timestamp", DT_BOOL, true, 0, NULL,
    "Add a timestamp to PGP or SMIME output to prevent spoofing"
  },
  { "envelope_from_address", DT_ADDRESS, 0, 0, NULL,
    "Manually set the sender for outgoing messages"
  },
  { "pgp_auto_inline", DT_BOOL, false, 0, NULL,
    "Use old-style inline PGP messages (not recommended)"
  },
  { "pgp_default_key", DT_STRING, 0, 0, NULL,
    "Default key to use for PGP operations"
  },
  { "pgp_entry_format", DT_EXPANDO|D_NOT_EMPTY, IP "%4n %t%f %4l/0x%k %-4a %2c %u", IP &PgpEntryFormatDef, NULL,
    "printf-like format string for the PGP key selection menu"
  },
  { "pgp_ignore_subkeys", DT_BOOL, true, 0, NULL,
    "Only use the principal PGP key"
  },
  { "pgp_long_ids", DT_BOOL, true, 0, NULL,
    "Display long PGP key IDs to the user"
  },
  { "pgp_mime_auto", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "Prompt the user to use MIME if inline PGP fails"
  },
  { "pgp_retainable_sigs", DT_BOOL, false, 0, NULL,
    "Create nested multipart/signed or encrypted messages"
  },
  { "pgp_self_encrypt", DT_BOOL, true, 0, NULL,
    "Encrypted messages will also be encrypted to $pgp_default_key too"
  },
  { "pgp_show_unusable", DT_BOOL, true, 0, NULL,
    "Show non-usable keys in the key selection"
  },
  { "pgp_sign_as", DT_STRING, 0, 0, NULL,
    "Use this alternative key for signing messages"
  },
  { "pgp_key_sort", DT_SORT|D_SORT_REVERSE, KEY_SORT_ADDRESS, IP KeySortMethods, NULL,
    "Sort order for PGP keys"
  },
  { "pgp_strict_enc", DT_BOOL, true, 0, NULL,
    "Encode PGP signed messages with quoted-printable (don't unset)"
  },
  { "smime_default_key", DT_STRING, 0, 0, NULL,
    "Default key for SMIME operations"
  },
  { "smime_encrypt_with", DT_STRING, IP "aes256", 0, NULL,
    "Algorithm for encryption"
  },
  { "smime_self_encrypt", DT_BOOL, true, 0, NULL,
    "Encrypted messages will also be encrypt to $smime_default_key too"
  },
  { "smime_sign_as", DT_STRING, 0, 0, NULL,
    "Use this alternative key for signing messages"
  },
  { "smime_is_default", DT_BOOL, false, 0, NULL,
    "Use SMIME rather than PGP by default"
  },
  { "pgp_auto_decode", DT_BOOL, false, 0, NULL,
    "Automatically decrypt PGP messages"
  },
  { "crypt_verify_sig", DT_QUAD, MUTT_YES, 0, NULL,
    "Verify PGP or SMIME signatures"
  },
  { "crypt_protected_headers_save", DT_BOOL, false, 0, NULL,
    "Save the cleartext Subject with the headers"
  },

  { "crypt_confirmhook",      DT_SYNONYM, IP "crypt_confirm_hook", IP "2021-02-11" },
  { "pgp_autoinline",         DT_SYNONYM, IP "pgp_auto_inline",    IP "2021-02-11" },
  { "pgp_create_traditional", DT_SYNONYM, IP "pgp_auto_inline",    IP "2004-04-12" },
  { "pgp_self_encrypt_as",    DT_SYNONYM, IP "pgp_default_key",    IP "2018-01-11" },
  { "pgp_sort_keys",          DT_SYNONYM, IP "pgp_key_sort",       IP "2024-11-20" },
  { "pgp_verify_sig",         DT_SYNONYM, IP "crypt_verify_sig",   IP "2002-01-24" },
  { "smime_self_encrypt_as",  DT_SYNONYM, IP "smime_default_key",  IP "2018-01-11" },

  { "pgp_encrypt_self",   D_INTERNAL_DEPRECATED|DT_QUAD, 0, IP "2019-09-09" },
  { "smime_encrypt_self", D_INTERNAL_DEPRECATED|DT_QUAD, 0, IP "2019-09-09" },

  { NULL },
  // clang-format on
};

#if defined(CRYPT_BACKEND_GPGME)
/**
 * NcryptVarsGpgme - GPGME Config definitions for the encryption library
 */
struct ConfigDef NcryptVarsGpgme[] = {
  // clang-format off
  { "crypt_use_gpgme", DT_BOOL|D_ON_STARTUP, true, 0, NULL,
    "Use GPGME crypto backend"
  },
  { "crypt_use_pka", DT_BOOL, false, 0, NULL,
    "Use GPGME to use PKA (lookup PGP keys using DNS)"
  },
  { NULL },
  // clang-format on
};
#endif

#if defined(CRYPT_BACKEND_CLASSIC_PGP)
/**
 * PgpCommandFormatDef - Expando definitions
 *
 * Config:
 * - $pgp_clear_sign_command
 * - $pgp_decode_command
 * - $pgp_decrypt_command
 * - $pgp_encrypt_only_command
 * - $pgp_encrypt_sign_command
 * - $pgp_export_command
 * - $pgp_get_keys_command
 * - $pgp_import_command
 * - $pgp_list_pubring_command
 * - $pgp_list_secring_command
 * - $pgp_sign_command
 * - $pgp_verify_command
 * - $pgp_verify_key_command
 */
static const struct ExpandoDefinition PgpCommandFormatDef[] = {
  // clang-format off
  { "a", "sign-as",        ED_PGP_CMD, ED_PGC_SIGN_AS,        NULL },
  { "f", "file-message",   ED_PGP_CMD, ED_PGC_FILE_MESSAGE,   NULL },
  { "p", "need-pass",      ED_PGP_CMD, ED_PGC_NEED_PASS,      NULL },
  { "r", "key-ids",        ED_PGP_CMD, ED_PGC_KEY_IDS,        NULL },
  { "s", "file-signature", ED_PGP_CMD, ED_PGC_FILE_SIGNATURE, NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};
#endif

#if defined(CRYPT_BACKEND_CLASSIC_SMIME)
/**
 * SmimeCommandFormatDef - Expando definitions
 *
 * Config:
 * - $smime_decrypt_command
 * - $smime_encrypt_command
 * - $smime_get_cert_command
 * - $smime_get_cert_email_command
 * - $smime_get_signer_cert_command
 * - $smime_import_cert_command
 * - $smime_pk7out_command
 * - $smime_sign_command
 * - $smime_verify_command
 * - $smime_verify_opaque_command
 */
static const struct ExpandoDefinition SmimeCommandFormatDef[] = {
  // clang-format off
  { "a", "algorithm",        ED_SMIME_CMD, ED_SMI_ALGORITHM,        NULL },
  { "c", "certificate-ids",  ED_SMIME_CMD, ED_SMI_CERTIFICATE_IDS,  NULL },
  { "C", "certificate-path", ED_SMIME_CMD, ED_SMI_CERTIFICATE_PATH, NULL },
  { "d", "digest-algorithm", ED_SMIME_CMD, ED_SMI_DIGEST_ALGORITHM, NULL },
  { "f", "message-file",     ED_SMIME_CMD, ED_SMI_MESSAGE_FILE,     NULL },
  { "i", "intermediate-ids", ED_SMIME_CMD, ED_SMI_INTERMEDIATE_IDS, NULL },
  { "k", "key",              ED_SMIME_CMD, ED_SMI_KEY,              NULL },
  { "s", "signature-file",   ED_SMIME_CMD, ED_SMI_SIGNATURE_FILE,   NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};
#endif

#if defined(CRYPT_BACKEND_CLASSIC_PGP)
/**
 * NcryptVarsPgp - PGP Config definitions for the encryption library
 */
struct ConfigDef NcryptVarsPgp[] = {
  // clang-format off
  { "pgp_check_exit", DT_BOOL, true, 0, NULL,
    "Check the exit code of PGP subprocess"
  },
  { "pgp_check_gpg_decrypt_status_fd", DT_BOOL, true, 0, NULL,
    "File descriptor used for status info"
  },
  { "pgp_clear_sign_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to inline-sign a message"
  },
  { "pgp_decode_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to decode a PGP attachment"
  },
  { "pgp_decrypt_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to decrypt a PGP message"
  },
  { "pgp_decryption_okay", DT_REGEX, 0, 0, NULL,
    "Text indicating a successful decryption"
  },
  { "pgp_encrypt_only_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to encrypt, but not sign a message"
  },
  { "pgp_encrypt_sign_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to encrypt and sign a message"
  },
  { "pgp_export_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to export a public key from the user's keyring"
  },
  { "pgp_get_keys_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to download a key for an email address"
  },
  { "pgp_good_sign", DT_REGEX, 0, 0, NULL,
    "Text indicating a good signature"
  },
  { "pgp_import_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to import a key into the user's keyring"
  },
  { "pgp_list_pubring_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to list the public keys in a user's keyring"
  },
  { "pgp_list_secring_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to list the private keys in a user's keyring"
  },
  { "pgp_sign_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to create a detached PGP signature"
  },
  { "pgp_timeout", DT_LONG|D_INTEGER_NOT_NEGATIVE, 300, 0, NULL,
    "Time in seconds to cache a passphrase"
  },
  { "pgp_use_gpg_agent", DT_BOOL, true, 0, NULL,
    "Use a PGP agent for caching passwords"
  },
  { "pgp_verify_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to verify PGP signatures"
  },
  { "pgp_verify_key_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &PgpCommandFormatDef, NULL,
    "(pgp) External command to verify key information"
  },
  { "pgp_clearsign_command",  DT_SYNONYM, IP "pgp_clear_sign_command", IP "2021-02-11" },
  { "pgp_getkeys_command",    DT_SYNONYM, IP "pgp_get_keys_command",   IP "2021-02-11" },
  { NULL },
  // clang-format on
};
#endif

#if defined(CRYPT_BACKEND_CLASSIC_SMIME)
/**
 * NcryptVarsSmime - SMIME Config definitions for the encryption library
 */
struct ConfigDef NcryptVarsSmime[] = {
  // clang-format off
  { "smime_ask_cert_label", DT_BOOL, true, 0, NULL,
    "Prompt the user for a label for SMIME certificates"
  },
  { "smime_ca_location", DT_PATH|D_PATH_FILE, 0, 0, NULL,
    "File containing trusted certificates"
  },
  { "smime_certificates", DT_PATH|D_PATH_DIR, 0, 0, NULL,
    "File containing user's public certificates"
  },
  { "smime_decrypt_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to decrypt an SMIME message"
  },
  { "smime_decrypt_use_default_key", DT_BOOL, true, 0, NULL,
    "Use the default key for decryption"
  },
  { "smime_encrypt_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to encrypt a message"
  },
  { "smime_get_cert_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to extract a certificate from a message"
  },
  { "smime_get_cert_email_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to get a certificate for an email"
  },
  { "smime_get_signer_cert_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to extract a certificate from an email"
  },
  { "smime_import_cert_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to import a certificate"
  },
  { "smime_keys", DT_PATH|D_PATH_DIR, 0, 0, NULL,
    "File containing user's private certificates"
  },
  { "smime_pk7out_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to extract a public certificate"
  },
  { "smime_sign_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to sign a message"
  },
  { "smime_sign_digest_alg", DT_STRING, IP "sha256", 0, NULL,
    "Digest algorithm"
  },
  { "smime_timeout", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 300, 0, NULL,
    "Time in seconds to cache a passphrase"
  },
  { "smime_verify_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to verify a signed message"
  },
  { "smime_verify_opaque_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP &SmimeCommandFormatDef, NULL,
    "(smime) External command to verify a signature"
  },
  { NULL },
  // clang-format on
};
#endif
