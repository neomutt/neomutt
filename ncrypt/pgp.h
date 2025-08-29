/**
 * @file
 * PGP sign, encrypt, check routines
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_NCRYPT_PGP_H
#define MUTT_NCRYPT_PGP_H

#include <stdbool.h>
#include <stdio.h>
#include "lib.h"

struct AddressList;
struct Body;
struct Email;
struct PgpKeyInfo;
struct State;

/**
 * struct PgpCommandContext - Data for a PGP command
 *
 * The actual command line formatter.
 */
struct PgpCommandContext
{
  bool need_passphrase;  ///< %p
  const char *fname;     ///< %f
  const char *sig_fname; ///< %s
  const char *signas;    ///< %a
  const char *ids;       ///< %r
};

/**
 * ExpandoDataPgpCmd - Expando UIDs for PGP Commands
 *
 * @sa ED_PGP_CMD, ExpandoDomain
 */
enum ExpandoDataPgpCmd
{
  ED_PGC_FILE_MESSAGE = 1,     ///< PgpCommandContext.fname
  ED_PGC_FILE_SIGNATURE,       ///< PgpCommandContext.sig_fname
  ED_PGC_KEY_IDS,              ///< PgpCommandContext.ids
  ED_PGC_NEED_PASS,            ///< PgpCommandContext.need_passphrase
  ED_PGC_SIGN_AS,              ///< PgpCommandContext.signas
};

char *        pgp_fpr_or_lkeyid                    (struct PgpKeyInfo *k);
char *        pgp_keyid                            (struct PgpKeyInfo *k);
char *        pgp_long_keyid                       (struct PgpKeyInfo *k);
char *        pgp_short_keyid                      (struct PgpKeyInfo *k);
char *        pgp_this_keyid                       (struct PgpKeyInfo *k);
bool          pgp_use_gpg_agent                    (void);

int           pgp_class_application_handler        (struct Body *b, struct State *state);
bool          pgp_class_check_traditional          (FILE *fp, struct Body *b, bool just_one);
int           pgp_class_decrypt_mime               (FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec);
int           pgp_class_encrypted_handler          (struct Body *b, struct State *state);
struct Body * pgp_class_encrypt_message            (struct Body *b, char *keylist, bool sign, const struct AddressList *from);
void          pgp_class_extract_key_from_attachment(FILE *fp, struct Body *b);
char *        pgp_class_find_keys                  (const struct AddressList *addrlist, bool oppenc_mode);
SecurityFlags pgp_class_send_menu                  (struct Email *e);
struct Body * pgp_class_sign_message               (struct Body *b, const struct AddressList *from);
struct Body * pgp_class_traditional_encryptsign    (struct Body *b, SecurityFlags flags, char *keylist);
bool          pgp_class_valid_passphrase           (void);
int           pgp_class_verify_one                 (struct Body *b, struct State *state, const char *tempfile);
void          pgp_class_void_passphrase            (void);

#endif /* MUTT_NCRYPT_PGP_H */
