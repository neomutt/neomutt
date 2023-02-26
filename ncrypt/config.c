/**
 * @file
 * Config used by libncrypt
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
 * @page crypt_config Config used by libncrypt
 *
 * Config used by libncrypt
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "mutt/lib.h"

/**
 * SortKeyMethods - Sort methods for encryption keys
 */
const struct Mapping SortKeyMethods[] = {
  // clang-format off
  { "address", SORT_ADDRESS },
  { "date",    SORT_DATE },
  { "keyid",   SORT_KEYID },
  { "trust",   SORT_TRUST },
  { NULL,      0 },
  // clang-format on
};

/**
 * NcryptVars - Config definitions for the encryption library
 */
static struct ConfigDef NcryptVars[] = {
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
  { "crypt_protected_headers_write", DT_BOOL, false, 0, NULL,
    "Generate protected header (Memory Hole) for signed and encrypted emails"
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
  { "pgp_entry_format", DT_STRING|DT_NOT_EMPTY, IP "%4n %t%f %4l/0x%k %-4a %2c %u", 0, NULL,
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
  { "pgp_sort_keys", DT_SORT|DT_SORT_REVERSE, SORT_ADDRESS, IP SortKeyMethods, NULL,
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
  { "pgp_verify_sig",         DT_SYNONYM, IP "crypt_verify_sig",   IP "2002-01-24" },
  { "smime_self_encrypt_as",  DT_SYNONYM, IP "smime_default_key",  IP "2018-01-11" },

  { "pgp_encrypt_self",   DT_DEPRECATED|DT_QUAD, 0, IP "2019-09-09" },
  { "smime_encrypt_self", DT_DEPRECATED|DT_QUAD, 0, IP "2019-09-09" },

  { NULL },
  // clang-format on
};

#if defined(CRYPT_BACKEND_GPGME)
/**
 * NcryptVarsGpgme - GPGME Config definitions for the encryption library
 */
static struct ConfigDef NcryptVarsGpgme[] = {
  // clang-format off
  { "crypt_use_gpgme", DT_BOOL, true, 0, NULL,
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
 * NcryptVarsPgp - PGP Config definitions for the encryption library
 */
static struct ConfigDef NcryptVarsPgp[] = {
  // clang-format off
  { "pgp_check_exit", DT_BOOL, true, 0, NULL,
    "Check the exit code of PGP subprocess"
  },
  { "pgp_check_gpg_decrypt_status_fd", DT_BOOL, true, 0, NULL,
    "File descriptor used for status info"
  },
  { "pgp_clear_sign_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to inline-sign a message"
  },
  { "pgp_decode_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to decode a PGP attachment"
  },
  { "pgp_decrypt_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to decrypt a PGP message"
  },
  { "pgp_decryption_okay", DT_REGEX, 0, 0, NULL,
    "Text indicating a successful decryption"
  },
  { "pgp_encrypt_only_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to encrypt, but not sign a message"
  },
  { "pgp_encrypt_sign_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to encrypt and sign a message"
  },
  { "pgp_export_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to export a public key from the user's keyring"
  },
  { "pgp_get_keys_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to download a key for an email address"
  },
  { "pgp_good_sign", DT_REGEX, 0, 0, NULL,
    "Text indicating a good signature"
  },
  { "pgp_import_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to import a key into the user's keyring"
  },
  { "pgp_list_pubring_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to list the public keys in a user's keyring"
  },
  { "pgp_list_secring_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to list the private keys in a user's keyring"
  },
  { "pgp_sign_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to create a detached PGP signature"
  },
  { "pgp_timeout", DT_LONG|DT_NOT_NEGATIVE, 300, 0, NULL,
    "Time in seconds to cache a passphrase"
  },
  { "pgp_use_gpg_agent", DT_BOOL, true, 0, NULL,
    "Use a PGP agent for caching passwords"
  },
  { "pgp_verify_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(pgp) External command to verify PGP signatures"
  },
  { "pgp_verify_key_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
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
static struct ConfigDef NcryptVarsSmime[] = {
  // clang-format off
  { "smime_ask_cert_label", DT_BOOL, true, 0, NULL,
    "Prompt the user for a label for SMIME certificates"
  },
  { "smime_ca_location", DT_PATH|DT_PATH_FILE, 0, 0, NULL,
    "File containing trusted certificates"
  },
  { "smime_certificates", DT_PATH|DT_PATH_DIR, 0, 0, NULL,
    "File containing user's public certificates"
  },
  { "smime_decrypt_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to decrypt an SMIME message"
  },
  { "smime_decrypt_use_default_key", DT_BOOL, true, 0, NULL,
    "Use the default key for decryption"
  },
  { "smime_encrypt_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to encrypt a message"
  },
  { "smime_get_cert_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to extract a certificate from a message"
  },
  { "smime_get_cert_email_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to get a certificate for an email"
  },
  { "smime_get_signer_cert_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to extract a certificate from an email"
  },
  { "smime_import_cert_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to import a certificate"
  },
  { "smime_keys", DT_PATH|DT_PATH_DIR, 0, 0, NULL,
    "File containing user's private certificates"
  },
  { "smime_pk7out_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to extract a public certificate"
  },
  { "smime_sign_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to sign a message"
  },
  { "smime_sign_digest_alg", DT_STRING, IP "sha256", 0, NULL,
    "Digest algorithm"
  },
  { "smime_timeout", DT_NUMBER|DT_NOT_NEGATIVE, 300, 0, NULL,
    "Time in seconds to cache a passphrase"
  },
  { "smime_verify_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to verify a signed message"
  },
  { "smime_verify_opaque_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "(smime) External command to verify a signature"
  },
  { NULL },
  // clang-format on
};
#endif

/**
 * config_init_ncrypt - Register ncrypt config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_ncrypt(struct ConfigSet *cs)
{
  bool rc = cs_register_variables(cs, NcryptVars, 0);

#if defined(CRYPT_BACKEND_GPGME)
  rc |= cs_register_variables(cs, NcryptVarsGpgme, 0);
#endif

#if defined(CRYPT_BACKEND_CLASSIC_PGP)
  rc |= cs_register_variables(cs, NcryptVarsPgp, 0);
#endif

#if defined(CRYPT_BACKEND_CLASSIC_SMIME)
  rc |= cs_register_variables(cs, NcryptVarsSmime, 0);
#endif

  return rc;
}
