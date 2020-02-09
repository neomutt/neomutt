/**
 * @file
 * Parse PGP data packets
 *
 * @authors
 * Copyright (C) 2001-2002,2007 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page crypt_pgppacket Parse PGP data packets
 *
 * Parse PGP data packets
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "pgppacket.h"

#define CHUNK_SIZE 1024 ///< Amount of data to read at once

static unsigned char *pbuf = NULL; ///< Cache PGP data packet
static size_t plen = 0;            ///< Length of cached packet

/**
 * read_material - Read PGP data into a buffer
 * @param[in]     material Number of bytes to read
 * @param[in,out] used     Number of bytes already read
 * @param[in]     fp       File to read from
 * @retval  0 Success
 * @retval -1 Failure (see errno)
 *
 * This function uses a cache to store the data: #pbuf, #plen.
 */
static int read_material(size_t material, size_t *used, FILE *fp)
{
  if (*used + material >= plen)
  {
    size_t nplen = *used + material + CHUNK_SIZE;

    unsigned char *p = realloc(pbuf, nplen);
    if (!p)
    {
      perror("realloc");
      return -1;
    }
    plen = nplen;
    pbuf = p;
  }

  if (fread(pbuf + *used, 1, material, fp) < material)
  {
    perror("fread");
    return -1;
  }

  *used += material;
  return 0;
}

/**
 * pgp_read_packet - Read a PGP packet from a file
 * @param[in]  fp  File to read from
 * @param[out] len Number of bytes read
 *
 * This function uses a cache to store the data: #pbuf, #plen.
 */
unsigned char *pgp_read_packet(FILE *fp, size_t *len)
{
  size_t used = 0;
  LOFF_T startpos;
  unsigned char ctb;
  unsigned char b;
  size_t material;

  startpos = ftello(fp);
  if (startpos < 0)
    return NULL;

  if (plen == 0)
  {
    plen = CHUNK_SIZE;
    pbuf = mutt_mem_malloc(plen);
  }

  if (fread(&ctb, 1, 1, fp) < 1)
  {
    if (!feof(fp))
      perror("fread");
    goto bail;
  }

  if (!(ctb & 0x80))
  {
    goto bail;
  }

  if (ctb & 0x40) /* handle PGP 5.0 packets. */
  {
    bool partial = false;
    pbuf[0] = ctb;
    used++;

    do
    {
      if (fread(&b, 1, 1, fp) < 1)
      {
        perror("fread");
        goto bail;
      }

      if (b < 192)
      {
        material = b;
        partial = false;
      }
      else if (b <= 223)
      {
        material = (b - 192) * 256;
        if (fread(&b, 1, 1, fp) < 1)
        {
          perror("fread");
          goto bail;
        }
        material += b + 192;
        partial = false;
      }
      else if (b < 255)
      {
        material = 1 << (b & 0x1f);
        partial = true;
      }
      else
      /* b == 255 */
      {
        unsigned char buf[4];
        if (fread(buf, 4, 1, fp) < 1)
        {
          perror("fread");
          goto bail;
        }
        material = (size_t) buf[0] << 24;
        material |= buf[1] << 16;
        material |= buf[2] << 8;
        material |= buf[3];
        partial = false;
      }

      if (read_material(material, &used, fp) == -1)
        goto bail;

    } while (partial);
  }
  else
  /* Old-Style PGP */
  {
    int bytes = 0;
    pbuf[0] = 0x80 | ((ctb >> 2) & 0x0f);
    used++;

    switch (ctb & 0x03)
    {
      case 0:
      {
        if (fread(&b, 1, 1, fp) < 1)
        {
          perror("fread");
          goto bail;
        }

        material = b;
        break;
      }

      case 1:
        bytes = 2;
        /* fallthrough */

      case 2:
      {
        if (!bytes)
          bytes = 4;

        material = 0;

        for (int i = 0; i < bytes; i++)
        {
          if (fread(&b, 1, 1, fp) < 1)
          {
            perror("fread");
            goto bail;
          }

          material = (material << 8) + b;
        }
        break;
      }

      default:
        goto bail;
    }

    if (read_material(material, &used, fp) == -1)
      goto bail;
  }

  if (len)
    *len = used;

  return pbuf;

bail:

  fseeko(fp, startpos, SEEK_SET);
  return NULL;
}

/**
 * pgp_release_packet - Free the cached PGP packet
 *
 * Free the data stored in #pbuf.
 */
void pgp_release_packet(void)
{
  plen = 0;
  FREE(&pbuf);
}
