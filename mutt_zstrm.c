/**
 * @file
 * IMAP traffic compression
 *
 * Copyright(C) 2019 Fabian Groffen <grobian@gentoo.org>
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

#include "config.h"
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include "conn/conn.h"
#include "mutt.h"
#include "mutt_zstrm.h"
#include "mutt_socket.h"

typedef struct
{
  struct zstrm_direction
  {
    z_stream z;
    char *buf;
    unsigned int len;
    unsigned int pos;
    unsigned int conn_eof : 1;
    unsigned int stream_eof : 1;
  } read, write;

  /* underlying stream */
  struct Connection next_conn;
} zstrmctx;

/* simple wrapper functions to match zlib interface for calling
 * malloc/free */
static void *mutt_zstrm_malloc(void *op, unsigned int sze, unsigned int v)
{
  return mutt_mem_calloc(sze, v);
}

static void mutt_zstrm_free(void *op, void *ptr)
{
  FREE(&ptr);
}

static int mutt_zstrm_open(struct Connection *conn)
{
  /* cannot open a zlib connection, must wrap an existing one */
  return -1;
}

static int mutt_zstrm_close(struct Connection *conn)
{
  zstrmctx *zctx = conn->sockdata;
  int rc = zctx->next_conn.conn_close(&zctx->next_conn);

  mutt_debug(LL_DEBUG4,
             "zstrm_close: read %llu->%llu (%.1fx) "
             "wrote %llu<-%llu (%.1fx)\n",
             zctx->read.z.total_in, zctx->read.z.total_out,
             (float) zctx->read.z.total_out / (float) zctx->read.z.total_in,
             zctx->write.z.total_in, zctx->write.z.total_out,
             (float) zctx->write.z.total_in / (float) zctx->write.z.total_out);

  conn->sockdata   = zctx->next_conn.sockdata;
  conn->conn_open  = zctx->next_conn.conn_open;
  conn->conn_close = zctx->next_conn.conn_close;
  conn->conn_read  = zctx->next_conn.conn_read;
  conn->conn_write = zctx->next_conn.conn_write;
  conn->conn_poll  = zctx->next_conn.conn_poll;

  inflateEnd(&zctx->read.z);
  deflateEnd(&zctx->write.z);
  FREE(&zctx->read.buf);
  FREE(&zctx->write.buf);
  FREE(&zctx);

  return rc;
}

static int mutt_zstrm_read(struct Connection *conn, char *buf, size_t len)
{
  zstrmctx *zctx = conn->sockdata;
  int rc = 0;
  int zrc;

retry:
  if (zctx->read.stream_eof)
    return 0;

  /* when avail_out was 0 on last call, we need to call inflate again,
   * because more data might be available using the current input, so
   * avoid callling read on the underlying stream in that case (for it
   * might block) */
  if (zctx->read.pos == 0 && !zctx->read.conn_eof)
  {
    rc = zctx->next_conn.conn_read(&zctx->next_conn, zctx->read.buf, zctx->read.len);
    mutt_debug(LL_DEBUG4,
               "zstrm_read: consuming data from next "
               "stream: %d bytes\n",
               rc);
    if (rc < 0)
      return rc;
    else if (rc == 0)
      zctx->read.conn_eof = 1;
    else
      zctx->read.pos += rc;
  }

  zctx->read.z.avail_in = (uInt) zctx->read.pos;
  zctx->read.z.next_in = (Bytef *) zctx->read.buf;
  zctx->read.z.avail_out = (uInt) len;
  zctx->read.z.next_out = (Bytef *) buf;

  zrc = inflate(&zctx->read.z, Z_SYNC_FLUSH);
  mutt_debug(LL_DEBUG4,
             "zstrm_read: rc=%d, "
             "consumed %u/%u bytes, produced %u/%u bytes\n",
             zrc, zctx->read.pos - zctx->read.z.avail_in, zctx->read.pos,
             len - zctx->read.z.avail_out, len);

  /* shift any remaining input data to the front of the buffer */
  if ((Bytef *) zctx->read.buf != zctx->read.z.next_in)
  {
    memmove(zctx->read.buf, zctx->read.z.next_in, zctx->read.z.avail_in);
    zctx->read.pos = zctx->read.z.avail_in;
  }

  switch (zrc)
  {
    case Z_OK:                            /* progress has been made */
      zrc = len - zctx->read.z.avail_out; /* "returned" bytes */
      if (zrc == 0)
      {
        /* there was progress, so must have been reading input */
        mutt_debug(LL_DEBUG4, "zstrm_read: inflate just consumed\n");
        goto retry;
      }
      break;
    case Z_STREAM_END: /* everything flushed, nothing remaining */
      mutt_debug(LL_DEBUG4, "zstrm_read: inflate returned Z_STREAM_END.\n");
      zrc = len - zctx->read.z.avail_out; /* "returned" bytes */
      zctx->read.stream_eof = 1;
      break;
    case Z_BUF_ERROR: /* no progress was possible */
      if (!zctx->read.conn_eof)
      {
        mutt_debug(LL_DEBUG5,
                   "zstrm_read: inflate returned Z_BUF_ERROR. retrying.\n");
        goto retry;
      }
      zrc = 0;
      break;
    default:
      /* bail on other rcs, such as Z_DATA_ERROR, or Z_MEM_ERROR */
      mutt_debug(LL_DEBUG4, "zstrm_read: inflate returned %d. aborting.\n", zrc);
      zrc = -1;
      break;
  }

  return zrc;
}

static int mutt_zstrm_poll(struct Connection *conn, time_t wait_secs)
{
  zstrmctx *zctx = conn->sockdata;

  mutt_debug(LL_DEBUG4, "zstrm_poll: %s\n",
             zctx->read.z.avail_out == 0 || zctx->read.pos > 0 ?
                 "last read wrote full buffer" :
                 "falling back on next stream");
  if (zctx->read.z.avail_out == 0 || zctx->read.pos > 0)
    return 1;
  else
    return zctx->next_conn.conn_poll(&zctx->next_conn, wait_secs);
}

static int mutt_zstrm_write(struct Connection *conn, const char *buf, size_t count)
{
  zstrmctx *zctx = conn->sockdata;
  int rc;
  int zrc;
  char *wbufp;

  zctx->write.z.avail_in = (uInt) count;
  zctx->write.z.next_in = (Bytef *) buf;
  zctx->write.z.avail_out = (uInt) zctx->write.len;
  zctx->write.z.next_out = (Bytef *) zctx->write.buf;

  do
  {
    zrc = deflate(&zctx->write.z, Z_PARTIAL_FLUSH);
    if (zrc == Z_OK)
    {
      /* push out produced data to the underlying stream */
      zctx->write.pos = zctx->write.len - zctx->write.z.avail_out;
      wbufp = zctx->write.buf;
      mutt_debug(LL_DEBUG4, "zstrm_write: deflate consumed %d/%d bytes\n",
                 count - zctx->write.z.avail_in, count);
      while (zctx->write.pos > 0)
      {
        rc = zctx->next_conn.conn_write(&zctx->next_conn, wbufp, zctx->write.pos);
        mutt_debug(LL_DEBUG4, "zstrm_write: next stream wrote: %d bytes\n", rc);
        if (rc < 0)
          return -1; /* we can't recover from write failure */

        wbufp += rc;
        zctx->write.pos -= rc;
      }

      /* see if there's more for us to do, retry if the output buffer
       * was full (there may be something in zlib buffers), and retry
       * when there is still available input data */
      if (zctx->write.z.avail_out != 0 && zctx->write.z.avail_in == 0)
        break;

      zctx->write.z.avail_out = (uInt) zctx->write.len;
      zctx->write.z.next_out = (Bytef *) zctx->write.buf;
    }
    else
    {
      /* compression went wrong, but this is basically impossible
       * according to the docs */
      return -1;
    }
  } while (1);

  rc = (int) count;
  return rc <= 0 ? 1 : rc; /* avoid wrong behaviour due to overflow */
}

void mutt_zstrm_wrap_conn(struct Connection *conn)
{
  zstrmctx *zctx;

  zctx = (zstrmctx *) mutt_mem_calloc(1, sizeof(zstrmctx));
  /* store wrapped stream as next stream */
  zctx->next_conn.fd = conn->fd;
  zctx->next_conn.sockdata = conn->sockdata;
  zctx->next_conn.conn_open = conn->conn_open;
  zctx->next_conn.conn_close = conn->conn_close;
  zctx->next_conn.conn_read = conn->conn_read;
  zctx->next_conn.conn_write = conn->conn_write;
  zctx->next_conn.conn_poll = conn->conn_poll;

  /* replace connection with our wrappers, where appropriate */
  conn->sockdata = (void *) zctx;
  conn->conn_open = mutt_zstrm_open;
  conn->conn_read = mutt_zstrm_read;
  conn->conn_write = mutt_zstrm_write;
  conn->conn_close = mutt_zstrm_close;
  conn->conn_poll = mutt_zstrm_poll;

  /* allocate/setup (de)compression buffers */
  zctx->read.len = 8192;
  zctx->read.buf = mutt_mem_malloc(zctx->read.len);
  zctx->read.pos = 0;
  zctx->write.len = 8192;
  zctx->write.buf = mutt_mem_malloc(zctx->write.len);
  zctx->write.pos = 0;

  /* initialise zlib for inflate and deflate for RFC4978 */
  zctx->read.z.zalloc = mutt_zstrm_malloc;
  zctx->read.z.zfree = mutt_zstrm_free;
  zctx->read.z.opaque = NULL;
  zctx->read.z.avail_out = zctx->read.len;
  inflateInit2(&zctx->read.z, -15);
  zctx->write.z.zalloc = mutt_zstrm_malloc;
  zctx->write.z.zfree = mutt_zstrm_free;
  zctx->write.z.opaque = NULL;
  zctx->write.z.avail_out = zctx->write.len;
  deflateInit2(&zctx->write.z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
}
