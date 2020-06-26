/**
 * @file
 * Identify the hash algorithm from a PGP signature
 *
 * @authors
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page crypt_pgpmicalg Identify the hash algorithm from a PGP signature
 *
 * Identify the Message Integrity Check algorithm (micalg) from a PGP signature
 */

#include "config.h"
#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "pgpmicalg.h"
#include "handler.h"
#include "pgppacket.h"
#include "state.h"

/**
 * struct HashAlgorithm - PGP Hashing algorithm
 */
struct HashAlgorithm
{
  short id;         ///< Algorithm Id
  const char *name; ///< Algorithm name
};

/**
 * HashAlgorithms - PGP Hashing algorithms
 */
static const struct HashAlgorithm HashAlgorithms[] = {
  { 1, "pgp-md5" },     { 2, "pgp-sha1" },     { 3, "pgp-ripemd160" },
  { 5, "pgp-md2" },     { 6, "pgp-tiger192" }, { 7, "pgp-haval-5-160" },
  { 8, "pgp-sha256" },  { 9, "pgp-sha384" },   { 10, "pgp-sha512" },
  { 11, "pgp-sha224" }, { -1, NULL },
};

/**
 * pgp_hash_to_micalg - Lookup a hash name, given its id
 * @param id ID
 * @retval ptr Name of hash algorithm
 */
static const char *pgp_hash_to_micalg(short id)
{
  for (int i = 0; HashAlgorithms[i].id >= 0; i++)
    if (HashAlgorithms[i].id == id)
      return HashAlgorithms[i].name;
  return "x-unknown";
}

/**
 * pgp_dearmor - Unwrap an armoured PGP block
 * @param fp_in  File to read from
 * @param fp_out File to write to
 */
static void pgp_dearmor(FILE *fp_in, FILE *fp_out)
{
  char line[8192];
  char *r = NULL;

  struct State state = { 0 };
  state.fp_in = fp_in;
  state.fp_out = fp_out;

  /* find the beginning of ASCII armor */

  while ((r = fgets(line, sizeof(line), fp_in)))
  {
    if (mutt_strn_equal(line, "-----BEGIN", 10))
      break;
  }
  if (!r)
  {
    mutt_debug(LL_DEBUG1, "Can't find begin of ASCII armor\n");
    return;
  }

  /* skip the armor header */

  while ((r = fgets(line, sizeof(line), fp_in)))
  {
    SKIPWS(r);
    if (*r == '\0')
      break;
  }
  if (!r)
  {
    mutt_debug(LL_DEBUG1, "Armor header doesn't end\n");
    return;
  }

  /* actual data starts here */
  LOFF_T start = ftello(fp_in);
  if (start < 0)
    return;

  /* find the checksum */

  while ((r = fgets(line, sizeof(line), fp_in)))
  {
    if ((*line == '=') || mutt_strn_equal(line, "-----END", 8))
      break;
  }
  if (!r)
  {
    mutt_debug(LL_DEBUG1, "Can't find end of ASCII armor\n");
    return;
  }

  LOFF_T end = ftello(fp_in) - strlen(line);
  if (end < start)
  {
    mutt_debug(LL_DEBUG1, "end < start???\n");
    return;
  }

  if (fseeko(fp_in, start, SEEK_SET) == -1)
  {
    mutt_debug(LL_DEBUG1, "Can't seekto start\n");
    return;
  }

  mutt_decode_base64(&state, end - start, false, (iconv_t) -1);
}

/**
 * pgp_mic_from_packet - Get the hash algorithm from a PGP packet
 * @param p   PGP packet
 * @param len Length of packet
 * @retval num Hash algorithm id
 */
static short pgp_mic_from_packet(unsigned char *p, size_t len)
{
  /* is signature? */
  if ((p[0] & 0x3f) != PT_SIG)
  {
    mutt_debug(LL_DEBUG1, "tag = %d, want %d\n", p[0] & 0x3f, PT_SIG);
    return -1;
  }

  if ((len >= 18) && (p[1] == 3))
  {
    /* version 3 signature */
    return (short) p[17];
  }
  else if ((len >= 5) && (p[1] == 4))
  {
    /* version 4 signature */
    return (short) p[4];
  }
  else
  {
    mutt_debug(LL_DEBUG1, "Bad signature packet\n");
    return -1;
  }
}

/**
 * pgp_find_hash - Find the hash algorithm of a file
 * @param fname File to read
 * @retval num Hash algorithm id
 */
static short pgp_find_hash(const char *fname)
{
  size_t l;
  short rc = -1;

  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    goto bye;
  }

  FILE *fp_in = fopen(fname, "r");
  if (!fp_in)
  {
    mutt_perror(fname);
    goto bye;
  }

  pgp_dearmor(fp_in, fp_out);
  rewind(fp_out);

  unsigned char *p = pgp_read_packet(fp_out, &l);
  if (p)
  {
    rc = pgp_mic_from_packet(p, l);
  }
  else
  {
    mutt_debug(LL_DEBUG1, "No packet\n");
  }

bye:

  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);
  pgp_release_packet();
  return rc;
}

/**
 * pgp_micalg - Find the hash algorithm of a file
 * @param fname File to read
 * @retval ptr Name of hash algorithm
 */
const char *pgp_micalg(const char *fname)
{
  return pgp_hash_to_micalg(pgp_find_hash(fname));
}
