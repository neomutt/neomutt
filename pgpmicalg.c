/*
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

/* This module peeks at a PGP signature and figures out the hash
 * algorithm.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "pgp.h"
#include "pgppacket.h"
#include "charset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const struct 
{
  short id;
  const char *name;
} 
HashAlgorithms[] = 
{
  { 1,		"pgp-md5"  		},
  { 2,  	"pgp-sha1" 		},
  { 3,  	"pgp-ripemd160" 	},
  { 5,  	"pgp-md2"		},
  { 6,  	"pgp-tiger192"		},
  { 7,		"pgp-haval-5-160" 	},
  { 8,		"pgp-sha256"		},
  { 9,		"pgp-sha384"		},
  { 10,		"pgp-sha512"		},
  { 11,		"pgp-sha224"		},
  { -1, 	NULL }
};

static const char *pgp_hash_to_micalg (short id)
{
  int i;
  
  for (i = 0; HashAlgorithms[i].id >= 0; i++)
    if (HashAlgorithms[i].id == id)
      return HashAlgorithms[i].name;
  return "x-unknown";
}

static void pgp_dearmor (FILE *in, FILE *out)
{
  char line[HUGE_STRING];
  LOFF_T start;
  LOFF_T end;
  char *r;

  STATE state;
  
  memset (&state, 0, sizeof (STATE));
  state.fpin = in;
  state.fpout = out;
  
  /* find the beginning of ASCII armor */
  
  while ((r = fgets (line, sizeof (line), in)) != NULL)
  {
    if (!strncmp (line, "-----BEGIN", 10))
      break;
  }
  if (r == NULL)
  {
    dprint (1, (debugfile, "pgp_dearmor: Can't find begin of ASCII armor.\n"));
    return;
  }

  /* skip the armor header */
  
  while ((r = fgets (line, sizeof (line), in)) != NULL)
  {
    SKIPWS (r);
    if (!*r) break;
  }
  if (r == NULL)
  {
    dprint (1, (debugfile, "pgp_dearmor: Armor header doesn't end.\n"));
    return;
  }
  
  /* actual data starts here */
  start = ftello (in);
  
  /* find the checksum */
  
  while ((r = fgets (line, sizeof (line), in)) != NULL)
  {
    if (*line == '=' || !strncmp (line, "-----END", 8))
      break;
  }
  if (r == NULL)
  {
    dprint (1, (debugfile, "pgp_dearmor: Can't find end of ASCII armor.\n"));
    return;
  }
  
  if ((end = ftello (in) - strlen (line)) < start)
  {
    dprint (1, (debugfile, "pgp_dearmor: end < start???\n"));
    return;
  }
  
  if (fseeko (in, start, SEEK_SET) == -1)
  {
    dprint (1, (debugfile, "pgp_dearmor: Can't seekto start.\n"));
    return;
  }

  mutt_decode_base64 (&state, end - start, 0, (iconv_t) -1);
}

static short pgp_mic_from_packet (unsigned char *p, size_t len)
{
  /* is signature? */
  if ((p[0] & 0x3f) != PT_SIG)
  {
    dprint (1, (debugfile, "pgp_mic_from_packet: tag = %d, want %d.\n",
		p[0]&0x3f, PT_SIG));
    return -1;
  }
  
  if (len >= 18 && p[1] == 3)
    /* version 3 signature */
    return (short) p[17];
  else if (len >= 5 && p[1] == 4)
    /* version 4 signature */
    return (short) p[4];
  else
  {
    dprint (1, (debugfile, "pgp_mic_from_packet: Bad signature packet.\n"));
    return -1;
  }
}

static short pgp_find_hash (const char *fname)
{
  FILE *in = NULL;
  FILE *out = NULL;
  
  char tempfile[_POSIX_PATH_MAX];
  
  unsigned char *p;
  size_t l;
  
  short rv = -1;
  
  mutt_mktemp (tempfile, sizeof (tempfile));
  if ((out = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    goto bye;
  }
  unlink (tempfile);
  
  if ((in = fopen (fname, "r")) == NULL)
  {
    mutt_perror (fname);
    goto bye;
  }
  
  pgp_dearmor (in, out);
  rewind (out);

  if ((p = pgp_read_packet (out, &l)) != NULL)
  {
    rv = pgp_mic_from_packet (p, l);
  }
  else
  {
    dprint (1, (debugfile, "pgp_find_hash: No packet.\n"));
  }
  
  bye:
  
  safe_fclose (&in);
  safe_fclose (&out);
  pgp_release_packet ();
  return rv;
}

const char *pgp_micalg (const char *fname)
{
  return pgp_hash_to_micalg (pgp_find_hash (fname));
}

