/**
 * @file
 * Parse PGP data packets
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page ncrypt_pgppacket Parse PGP data packets
 *
 * Parse PGP data packets
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "pgppacket.h"
#include "module_data.h"

#define CHUNK_SIZE 1024 ///< Amount of data to read at once

/**
 * read_material - Read PGP data into a buffer
 * @param[in]     material Number of bytes to read
 * @param[in,out] used     Number of bytes already read
 * @param[in]     fp       File to read from
 * @retval  0 Success
 * @retval -1 Failure (see errno)
 *
 * This function uses a cache to store the data: #NcryptModuleData::packet_buf
 */
static int read_material(size_t material, size_t *used, FILE *fp)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  if ((*used + material) >= mod_data->packet_buf_len)
  {
    mod_data->packet_buf_len = *used + material + CHUNK_SIZE;

    MUTT_MEM_REALLOC(&mod_data->packet_buf, mod_data->packet_buf_len, unsigned char);
  }

  if (fread(mod_data->packet_buf + *used, 1, material, fp) < material)
  {
    mutt_perror("fread");
    return -1;
  }

  *used += material;
  return 0;
}

/**
 * pgp_read_packet - Read a PGP packet from a file
 * @param[in]  fp  File to read from
 * @param[out] len Number of bytes read
 * @retval ptr PGP data packet
 *
 * This function uses a cache to store the data: #NcryptModuleData::packet_buf
 */
unsigned char *pgp_read_packet(FILE *fp, size_t *len)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  size_t used = 0;
  LOFF_T startpos;
  unsigned char ctb;
  unsigned char b;
  size_t material;

  startpos = ftello(fp);
  if (startpos < 0)
    return NULL;

  if (mod_data->packet_buf_len == 0)
  {
    mod_data->packet_buf_len = CHUNK_SIZE;
    mod_data->packet_buf = MUTT_MEM_MALLOC(mod_data->packet_buf_len, unsigned char);
  }

  if (fread(&ctb, 1, 1, fp) < 1)
  {
    if (!feof(fp))
      mutt_perror("fread");
    goto bail;
  }

  if (!(ctb & 0x80))
  {
    goto bail;
  }

  if (ctb & 0x40) /* handle PGP 5.0 packets. */
  {
    bool partial = false;
    mod_data->packet_buf[0] = ctb;
    used++;

    do
    {
      if (fread(&b, 1, 1, fp) < 1)
      {
        mutt_perror("fread");
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
          mutt_perror("fread");
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
      else /* b == 255 */
      {
        unsigned char buf[4];
        if (fread(buf, 4, 1, fp) < 1)
        {
          mutt_perror("fread");
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
  else /* Old-Style PGP */
  {
    int bytes = 0;
    mod_data->packet_buf[0] = 0x80 | ((ctb >> 2) & 0x0f);
    used++;

    switch (ctb & 0x03)
    {
      case 0:
      {
        if (fread(&b, 1, 1, fp) < 1)
        {
          mutt_perror("fread");
          goto bail;
        }

        material = b;
        break;
      }

      case 1:
        bytes = 2;
        FALLTHROUGH;

      case 2:
      {
        if (!bytes)
          bytes = 4;

        material = 0;

        for (int i = 0; i < bytes; i++)
        {
          if (fread(&b, 1, 1, fp) < 1)
          {
            mutt_perror("fread");
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

  return mod_data->packet_buf;

bail:

  (void) mutt_file_seek(fp, startpos, SEEK_SET);
  return NULL;
}

/**
 * pgp_release_packet - Free the cached PGP packet
 *
 * Free the data stored in #NcryptModuleData::packet_buf
 */
void pgp_release_packet(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  mod_data->packet_buf_len = 0;
  FREE(&mod_data->packet_buf);
}
