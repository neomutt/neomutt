/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 *                    Oliver Ehli <elmy@acm.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */


#if defined(HAVE_PGP) || defined(HAVE_SMIME)

#define ENCRYPT  (1 << 0)
#define SIGN     (1 << 1)
#define GOODSIGN (1 << 2)
#define BADSIGN  (1 << 3)

int mutt_protect (HEADER *, char *);

int mutt_is_multipart_encrypted (BODY *);

int mutt_is_multipart_signed (BODY *);

void mutt_signed_handler (BODY *, STATE *);

int mutt_parse_crypt_hdr (char *, int);

int crypt_query (BODY *);

void crypt_extract_keys_from_messages (HEADER *);

int crypt_get_keys (HEADER *, char **);


void crypt_forget_passphrase (void);

int crypt_valid_passphrase (int);


int crypt_write_signed(BODY *, STATE *, const char *);

void convert_to_7bit (BODY *);


/* private ? */

void crypt_current_time(STATE *, char *);


#endif
