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
#include "private.h"
#include "init.h"

// clang-format off
#ifdef CRYPT_BACKEND_GPGME
bool            C_CryptUsePka;                         ///< Config: Use GPGME to use PKA (lookup PGP keys using DNS)
#endif
bool            C_CryptConfirmhook;                    ///< Config: Prompt the user to confirm keys before use
bool            C_CryptOpportunisticEncrypt;           ///< Config: Enable encryption when the recipient's key is available
bool            C_CryptOpportunisticEncryptStrongKeys; ///< Config: Enable encryption only when strong a key is available
bool            C_CryptProtectedHeadersRead;           ///< Config: Display protected headers (Memory Hole) in the pager
bool            C_CryptProtectedHeadersSave;           ///< Config: Save the cleartext Subject with the headers
bool            C_CryptProtectedHeadersWrite;          ///< Config: Generate protected header (Memory Hole) for signed and encrypted emails
bool            C_SmimeIsDefault;                      ///< Config: Use SMIME rather than PGP by default
bool            C_PgpIgnoreSubkeys;                    ///< Config: Only use the principal PGP key
bool            C_PgpLongIds;                          ///< Config: Display long PGP key IDs to the user
bool            C_PgpShowUnusable;                     ///< Config: Show non-usable keys in the key selection
bool            C_PgpAutoinline;                       ///< Config: Use old-style inline PGP messages (not recommended)
char *          C_PgpDefaultKey;                       ///< Config: Default key to use for PGP operations
char *          C_PgpSignAs;                           ///< Config: Use this alternative key for signing messages
char *          C_PgpEntryFormat;                      ///< Config: printf-like format string for the PGP key selection menu
char *          C_SmimeDefaultKey;                     ///< Config: Default key for SMIME operations
char *          C_SmimeSignAs;                         ///< Config: Use this alternative key for signing messages
char *          C_SmimeEncryptWith;                    ///< Config: Algorithm for encryption
char *          C_CryptProtectedHeadersSubject;        ///< Config: Use this as the subject for encrypted emails
struct Address *C_EnvelopeFromAddress;                 ///< Config: Manually set the sender for outgoing messages
bool            C_CryptTimestamp;                      ///< Config: Add a timestamp to PGP or SMIME output to prevent spoofing
unsigned char   C_PgpEncryptSelf;
unsigned char   C_PgpMimeAuto;                         ///< Config: Prompt the user to use MIME if inline PGP fails
bool            C_PgpRetainableSigs;                   ///< Config: Create nested multipart/signed or encrypted messages
bool            C_PgpSelfEncrypt;                      ///< Config: Encrypted messages will also be encrypted to C_PgpDefaultKey too
bool            C_PgpStrictEnc;                        ///< Config: Encode PGP signed messages with quoted-printable (don't unset)
unsigned char   C_SmimeEncryptSelf;
bool            C_SmimeSelfEncrypt;                    ///< Config: Encrypted messages will also be encrypt to C_SmimeDefaultKey too
#ifdef CRYPT_BACKEND_GPGME
bool            C_CryptUseGpgme;                       ///< Config: Use GPGME crypto backend
#endif
bool            C_PgpCheckExit;                        ///< Config: Check the exit code of PGP subprocess
bool            C_PgpCheckGpgDecryptStatusFd;          ///< Config: File descriptor used for status info
struct Regex *  C_PgpDecryptionOkay;                   ///< Config: Text indicating a successful decryption
struct Regex *  C_PgpGoodSign;                         ///< Config: Text indicating a good signature
long            C_PgpTimeout;                          ///< Config: Time in seconds to cache a passphrase
bool            C_PgpUseGpgAgent;                      ///< Config: Use a PGP agent for caching passwords
char *          C_PgpClearsignCommand;                 ///< Config: (pgp) External command to inline-sign a message
char *          C_PgpDecodeCommand;                    ///< Config: (pgp) External command to decode a PGP attachment
char *          C_PgpDecryptCommand;                   ///< Config: (pgp) External command to decrypt a PGP message
char *          C_PgpEncryptOnlyCommand;               ///< Config: (pgp) External command to encrypt, but not sign a message
char *          C_PgpEncryptSignCommand;               ///< Config: (pgp) External command to encrypt and sign a message
char *          C_PgpExportCommand;                    ///< Config: (pgp) External command to export a public key from the user's keyring
char *          C_PgpGetkeysCommand;                   ///< Config: (pgp) External command to download a key for an email address
char *          C_PgpImportCommand;                    ///< Config: (pgp) External command to import a key into the user's keyring
char *          C_PgpListPubringCommand;               ///< Config: (pgp) External command to list the public keys in a user's keyring
char *          C_PgpListSecringCommand;               ///< Config: (pgp) External command to list the private keys in a user's keyring
char *          C_PgpSignCommand;                      ///< Config: (pgp) External command to create a detached PGP signature
char *          C_PgpVerifyCommand;                    ///< Config: (pgp) External command to verify PGP signatures
char *          C_PgpVerifyKeyCommand;                 ///< Config: (pgp) External command to verify key information
short           C_PgpSortKeys;                         ///< Config: Sort order for PGP keys
bool            C_SmimeAskCertLabel;                   ///< Config: Prompt the user for a label for SMIME certificates
char *          C_SmimeCaLocation;                     ///< Config: File containing trusted certificates
char *          C_SmimeCertificates;                   ///< Config: File containing user's public certificates
char *          C_SmimeDecryptCommand;                 ///< Config: (smime) External command to decrypt an SMIME message
bool            C_SmimeDecryptUseDefaultKey;           ///< Config: Use the default key for decryption
char *          C_SmimeEncryptCommand;                 ///< Config: (smime) External command to encrypt a message
char *          C_SmimeGetCertCommand;                 ///< Config: (smime) External command to extract a certificate from a message
char *          C_SmimeGetCertEmailCommand;            ///< Config: (smime) External command to get a certificate for an email
char *          C_SmimeGetSignerCertCommand;           ///< Config: (smime) External command to extract a certificate from an email
char *          C_SmimeImportCertCommand;              ///< Config: (smime) External command to import a certificate
char *          C_SmimeKeys;                           ///< Config: File containing user's private certificates
char *          C_SmimePk7outCommand;                  ///< Config: (smime) External command to extract a public certificate
char *          C_SmimeSignCommand;                    ///< Config: (smime) External command to sign a message
char *          C_SmimeSignDigestAlg;                  ///< Config: Digest algorithm
long            C_SmimeTimeout;                        ///< Config: Time in seconds to cache a passphrase
char *          C_SmimeVerifyCommand;                  ///< Config: (smime) External command to verify a signed message
char *          C_SmimeVerifyOpaqueCommand;            ///< Config: (smime) External command to verify a signature
bool            C_PgpAutoDecode;                       ///< Config: Automatically decrypt PGP messages
unsigned char   C_CryptVerifySig;                      ///< Config: Verify PGP or SMIME signatures
// clang-format on

// clang-format off
struct ConfigDef NcryptVars[] = {
  { "crypt_confirmhook",                       DT_BOOL,                   &C_CryptConfirmhook,                    true },
  { "crypt_opportunistic_encrypt",             DT_BOOL,                   &C_CryptOpportunisticEncrypt,           false },
  { "crypt_opportunistic_encrypt_strong_keys", DT_BOOL,                   &C_CryptOpportunisticEncryptStrongKeys, false },
  { "crypt_protected_headers_read",            DT_BOOL,                   &C_CryptProtectedHeadersRead,           true },
  { "crypt_protected_headers_subject",         DT_STRING,                 &C_CryptProtectedHeadersSubject,        IP "..." },
  { "crypt_protected_headers_write",           DT_BOOL,                   &C_CryptProtectedHeadersWrite,          false },
  { "crypt_timestamp",                         DT_BOOL,                   &C_CryptTimestamp,                      true },
#ifdef CRYPT_BACKEND_GPGME
  { "crypt_use_gpgme",                         DT_BOOL,                   &C_CryptUseGpgme,                       true },
  { "crypt_use_pka",                           DT_BOOL,                   &C_CryptUsePka,                         false },
#endif
  { "envelope_from_address",                   DT_ADDRESS,                &C_EnvelopeFromAddress,                 0 },
  { "pgp_autoinline",                          DT_BOOL,                   &C_PgpAutoinline,                       false },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp_check_exit",                          DT_BOOL,                   &C_PgpCheckExit,                        true },
  { "pgp_check_gpg_decrypt_status_fd",         DT_BOOL,                   &C_PgpCheckGpgDecryptStatusFd,          true },
  { "pgp_clearsign_command",                   DT_STRING|DT_COMMAND,      &C_PgpClearsignCommand,                 0 },
  { "pgp_decode_command",                      DT_STRING|DT_COMMAND,      &C_PgpDecodeCommand,                    0 },
  { "pgp_decrypt_command",                     DT_STRING|DT_COMMAND,      &C_PgpDecryptCommand,                   0 },
  { "pgp_decryption_okay",                     DT_REGEX,                  &C_PgpDecryptionOkay,                   0 },
#endif
  { "pgp_default_key",                         DT_STRING,                 &C_PgpDefaultKey,                       0 },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp_encrypt_only_command",                DT_STRING|DT_COMMAND,      &C_PgpEncryptOnlyCommand,               0 },
  { "pgp_encrypt_sign_command",                DT_STRING|DT_COMMAND,      &C_PgpEncryptSignCommand,               0 },
#endif
  { "pgp_entry_format",                        DT_STRING|DT_NOT_EMPTY,    &C_PgpEntryFormat,                      IP "%4n %t%f %4l/0x%k %-4a %2c %u" },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp_export_command",                      DT_STRING|DT_COMMAND,      &C_PgpExportCommand,                    0 },
  { "pgp_getkeys_command",                     DT_STRING|DT_COMMAND,      &C_PgpGetkeysCommand,                   0 },
  { "pgp_good_sign",                           DT_REGEX,                  &C_PgpGoodSign,                         0 },
#endif
  { "pgp_ignore_subkeys",                      DT_BOOL,                   &C_PgpIgnoreSubkeys,                    true },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp_import_command",                      DT_STRING|DT_COMMAND,      &C_PgpImportCommand,                    0 },
  { "pgp_list_pubring_command",                DT_STRING|DT_COMMAND,      &C_PgpListPubringCommand,               0 },
  { "pgp_list_secring_command",                DT_STRING|DT_COMMAND,      &C_PgpListSecringCommand,               0 },
#endif
  { "pgp_long_ids",                            DT_BOOL,                   &C_PgpLongIds,                          true },
  { "pgp_mime_auto",                           DT_QUAD,                   &C_PgpMimeAuto,                         MUTT_ASKYES },
  { "pgp_retainable_sigs",                     DT_BOOL,                   &C_PgpRetainableSigs,                   false },
  { "pgp_self_encrypt",                        DT_BOOL,                   &C_PgpSelfEncrypt,                      true },
  { "pgp_show_unusable",                       DT_BOOL,                   &C_PgpShowUnusable,                     true },
  { "pgp_sign_as",                             DT_STRING,                 &C_PgpSignAs,                           0 },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp_sign_command",                        DT_STRING|DT_COMMAND,      &C_PgpSignCommand,                      0 },
#endif
  { "pgp_sort_keys",                           DT_SORT|DT_SORT_KEYS,      &C_PgpSortKeys,                         SORT_ADDRESS },
  { "pgp_strict_enc",                          DT_BOOL,                   &C_PgpStrictEnc,                        true },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp_timeout",                             DT_LONG|DT_NOT_NEGATIVE,   &C_PgpTimeout,                          300 },
  { "pgp_use_gpg_agent",                       DT_BOOL,                   &C_PgpUseGpgAgent,                      true },
  { "pgp_verify_command",                      DT_STRING|DT_COMMAND,      &C_PgpVerifyCommand,                    0 },
  { "pgp_verify_key_command",                  DT_STRING|DT_COMMAND,      &C_PgpVerifyKeyCommand,                 0 },
#endif
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "smime_ask_cert_label",                    DT_BOOL,                   &C_SmimeAskCertLabel,                   true },
  { "smime_ca_location",                       DT_PATH|DT_PATH_FILE,      &C_SmimeCaLocation,                     0 },
  { "smime_certificates",                      DT_PATH|DT_PATH_DIR,       &C_SmimeCertificates,                   0 },
  { "smime_decrypt_command",                   DT_STRING|DT_COMMAND,      &C_SmimeDecryptCommand,                 0 },
  { "smime_decrypt_use_default_key",           DT_BOOL,                   &C_SmimeDecryptUseDefaultKey,           true },
#endif
  { "smime_default_key",                       DT_STRING,                 &C_SmimeDefaultKey,                     0 },
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "smime_encrypt_command",                   DT_STRING|DT_COMMAND,      &C_SmimeEncryptCommand,                 0 },
#endif
  { "smime_encrypt_with",                      DT_STRING,                 &C_SmimeEncryptWith,                    IP "aes256" },
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "smime_get_cert_command",                  DT_STRING|DT_COMMAND,      &C_SmimeGetCertCommand,                 0 },
  { "smime_get_cert_email_command",            DT_STRING|DT_COMMAND,      &C_SmimeGetCertEmailCommand,            0 },
  { "smime_get_signer_cert_command",           DT_STRING|DT_COMMAND,      &C_SmimeGetSignerCertCommand,           0 },
  { "smime_import_cert_command",               DT_STRING|DT_COMMAND,      &C_SmimeImportCertCommand,              0 },
#endif
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "smime_keys",                              DT_PATH|DT_PATH_DIR,       &C_SmimeKeys,                           0 },
  { "smime_pk7out_command",                    DT_STRING|DT_COMMAND,      &C_SmimePk7outCommand,                  0 },
#endif
  { "smime_self_encrypt",                      DT_BOOL,                   &C_SmimeSelfEncrypt,                    true },
  { "smime_sign_as",                           DT_STRING,                 &C_SmimeSignAs,                         0 },
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "smime_sign_command",                      DT_STRING|DT_COMMAND,      &C_SmimeSignCommand,                    0 },
  { "smime_sign_digest_alg",                   DT_STRING,                 &C_SmimeSignDigestAlg,                  IP "sha256" },
  { "smime_timeout",                           DT_NUMBER|DT_NOT_NEGATIVE, &C_SmimeTimeout,                        300 },
  { "smime_verify_command",                    DT_STRING|DT_COMMAND,      &C_SmimeVerifyCommand,                  0 },
  { "smime_verify_opaque_command",             DT_STRING|DT_COMMAND,      &C_SmimeVerifyOpaqueCommand,            0 },
#endif

  { "smime_is_default",                        DT_BOOL,                   &C_SmimeIsDefault,                      false },
  { "pgp_auto_decode",                         DT_BOOL,                   &C_PgpAutoDecode,                       false },
  { "crypt_verify_sig",                        DT_QUAD,                   &C_CryptVerifySig,                      MUTT_YES },
  { "crypt_protected_headers_save",            DT_BOOL,                   &C_CryptProtectedHeadersSave,           false },

  { "pgp_create_traditional", DT_SYNONYM, NULL, IP "pgp_autoinline",    },
  { "pgp_self_encrypt_as",    DT_SYNONYM, NULL, IP "pgp_default_key",   },
  { "pgp_verify_sig",         DT_SYNONYM, NULL, IP "crypt_verify_sig",  },
  { "smime_self_encrypt_as",  DT_SYNONYM, NULL, IP "smime_default_key", },

  { "pgp_encrypt_self",   DT_DEPRECATED|DT_QUAD, &C_PgpEncryptSelf,   MUTT_NO },
  { "smime_encrypt_self", DT_DEPRECATED|DT_QUAD, &C_SmimeEncryptSelf, MUTT_NO },

  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_ncrypt - Register ncrypt config variables
 */
bool config_init_ncrypt(struct ConfigSet *cs)
{
  return cs_register_variables(cs, NcryptVars, 0);
}
