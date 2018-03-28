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

/* This module peeks at a PGP signature and figures out the hash
 * algorithm.
 */

#include "config.h"
#include <ctype.h>
#include <iconv.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "pgppacket.h"
#include "protos.h"
#include "state.h"

static const struct
{
  short id;
  const char *name;
} HashAlgorithms[] = {
  { 1, "pgp-md5" },     { 2, "pgp-sha1" },     { 3, "pgp-ripemd160" },
  { 5, "pgp-md2" },     { 6, "pgp-tiger192" }, { 7, "pgp-haval-5-160" },
  { 8, "pgp-sha256" },  { 9, "pgp-sha384" },   { 10, "pgp-sha512" },
  { 11, "pgp-sha224" }, { -1, NULL },
};

static const char *pgp_hash_to_micalg(short id)
{
  for (int i = 0; HashAlgorithms[i].id >= 0; i++)
    if (HashAlgorithms[i].id == id)
      return HashAlgorithms[i].name;
  return "x-unknown";
}

static void pgp_dearmor(FILE *in, FILE *out)
{
  char line[HUGE_STRING];
  LOFF_T start;
  LOFF_T end;
  char *r = NULL;

  struct State state;

  memset(&state, 0, sizeof(struct State));
  state.fpin = in;
  state.fpout = out;

  /* find the beginning of ASCII armor */

  while ((r = fgets(line, sizeof(line), in)) != NULL)
  {
    if (strncmp(line, "-----BEGIN", 10) == 0)
      break;
  }
  if (!r)
  {
    mutt_debug(1, "Can't find begin of ASCII armor.\n");
    return;
  }

  /* skip the armor header */

  while ((r = fgets(line, sizeof(line), in)) != NULL)
  {
    SKIPWS(r);
    if (!*r)
      break;
  }
  if (!r)
  {
    mutt_debug(1, "Armor header doesn't end.\n");
    return;
  }

  /* actual data starts here */
  start = ftello(in);
  if (start < 0)
    return;

  /* find the checksum */

  while ((r = fgets(line, sizeof(line), in)) != NULL)
  {
    if (*line == '=' || (strncmp(line, "-----END", 8) == 0))
      break;
  }
  if (!r)
  {
    mutt_debug(1, "Can't find end of ASCII armor.\n");
    return;
  }

  end = ftello(in) - strlen(line);
  if (end < start)
  {
    mutt_debug(1, "end < start???\n");
    return;
  }

  if (fseeko(in, start, SEEK_SET) == -1)
  {
    mutt_debug(1, "Can't seekto start.\n");
    return;
  }

  mutt_decode_base64(&state, end - start, 0, (iconv_t) -1);
}

static short pgp_mic_from_packet(unsigned char *p, size_t len)
{
  /* is signature? */
  if ((p[0] & 0x3f) != PT_SIG)
  {
    mutt_debug(1, "tag = %d, want %d.\n", p[0] & 0x3f, PT_SIG);
    return -1;
  }

  if (len >= 18 && p[1] == 3)
  {
    /* version 3 signature */
    return (short) p[17];
  }
  else if (len >= 5 && p[1] == 4)
  {
    /* version 4 signature */
    return (short) p[4];
  }
  else
  {
    mutt_debug(1, "Bad signature packet.\n");
    return -1;
  }
}

static short pgp_find_hash(const char *fname)
{
  char tempfile[_POSIX_PATH_MAX];
  size_t l;
  short rc = -1;

  mutt_mktemp(tempfile, sizeof(tempfile));
  FILE *out = mutt_file_fopen(tempfile, "w+");
  if (!out)
  {
    mutt_perror(tempfile);
    goto bye;
  }
  unlink(tempfile);

  FILE *in = fopen(fname, "r");
  if (!in)
  {
    mutt_perror(fname);
    goto bye;
  }

  pgp_dearmor(in, out);
  rewind(out);

  unsigned char *p = pgp_read_packet(out, &l);
  if (p)
  {
    rc = pgp_mic_from_packet(p, l);
  }
  else
  {
    mutt_debug(1, "No packet.\n");
  }

bye:

  mutt_file_fclose(&in);
  mutt_file_fclose(&out);
  pgp_release_packet();
  return rc;
}

const char *pgp_micalg(const char *fname)
{
  return pgp_hash_to_micalg(pgp_find_hash(fname));
}
