/**
 * @file
 * Ncrypt private Module data
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_NCRYPT_MODULE_DATA_H
#define MUTT_NCRYPT_MODULE_DATA_H

#include <time.h>
#include "mutt/lib.h"
#include "crypt_mod.h"
#ifdef CRYPT_BACKEND_GPGME
#include <gpgme.h>
#include "crypt_gpgme.h"
#endif

/**
 * struct NcryptModuleData - Ncrypt private Module data
 */
struct NcryptModuleData
{

  struct MenuDefinition  *menu_pgp;                    ///< PGP menu definition
  struct MenuDefinition  *menu_smime;                  ///< S/MIME menu definition
  struct CryptCache      *gpgme_id_defaults;           ///< GPGME IdDefaults cache
#ifdef CRYPT_BACKEND_GPGME
  gpgme_key_t             signature_key;               ///< GPGME Signature key
  int                     key_info_padding[KIP_MAX];   ///< Padding for key info prompts
#endif
  char                   *current_sender;              ///< Current sender for GPGME
  struct CryptModuleList  crypt_modules;               ///< Linked list of crypto modules
  char                   *charset;                     ///< gnupgparse charset
  char                    pgp_pass[1024];              ///< Cached PGP Passphrase
  time_t                  pgp_exptime;                 ///< Unix time when pgp_pass expires
  struct PgpCache        *pgp_id_defaults;             ///< PGP IdDefaults cache
  unsigned char          *packet_buf;                  ///< Cached PGP data packet
  size_t                  packet_buf_len;              ///< Length of cached packet
  char                    smime_pass[256];             ///< Cached S/MIME Passphrase
  time_t                  smime_exp_time;              ///< Unix time when smime_pass expires
  struct Buffer           smime_key_to_use;            ///< S/MIME key to use
  struct Buffer           smime_cert_to_use;           ///< S/MIME certificate to use
  struct Buffer           smime_intermediate_to_use;   ///< S/MIME intermediate certificate to use
};

#endif /* MUTT_NCRYPT_MODULE_DATA_H */
