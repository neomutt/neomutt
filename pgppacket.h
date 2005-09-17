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

/* 
 * Definitions for a rudimentary PGP packet parser which is shared
 * by mutt proper and the PGP public key ring lister.
 */

#ifndef _PGPPACKET_H
# define _PGPPACKET_H

enum packet_tags
{
  PT_RES0 = 0,			/* reserved */
  PT_ESK,			/* Encrypted Session Key */
  PT_SIG,			/* Signature Packet */
  PT_CESK,			/* Conventionally Encrypted Session Key Packet */
  PT_OPS,			/* One-Pass Signature Packet */
  PT_SECKEY,			/* Secret Key Packet */
  PT_PUBKEY,			/* Public Key Packet */
  PT_SUBSECKEY,			/* Secret Subkey Packet */
  PT_COMPRESSED,		/* Compressed Data Packet */
  PT_SKE,			/* Symmetrically Encrypted Data Packet */
  PT_MARKER,			/* Marker Packet */
  PT_LITERAL,			/* Literal Data Packet */
  PT_TRUST,			/* Trust Packet */
  PT_NAME,			/* Name Packet */
  PT_SUBKEY,			/* Subkey Packet */
  PT_RES15,			/* Reserved */
  PT_COMMENT			/* Comment Packet */
};

unsigned char *pgp_read_packet (FILE * fp, size_t * len);
void pgp_release_packet (void);

#endif
