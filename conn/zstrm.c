/**
 * @file
 * Zlib compression of network traffic
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

/**
 * @page conn_zstrm Zlib compression of network traffic
 *
 * Zlib compression of network traffic
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <zconf.h>
#include <zlib.h>
#include "mutt/lib.h"
#include "conn/lib.h"
#include "zstrm.h"

/**
 * struct ZstrmDirection - A stream of data being (de-)compressed
 */
struct ZstrmDirection
{
  z_stream z;          ///< zlib compression handle
  char *buf;           ///< Buffer for data being (de-)compressed
  unsigned int len;    ///< Length of data
  unsigned int pos;    ///< Current position
  bool conn_eof : 1;   ///< Connection end-of-file reached
  bool stream_eof : 1; ///< Stream end-of-file reached
};

/**
 * struct ZstrmContext - Data compression layer
 */
struct ZstrmContext
{
  struct ZstrmDirection read;  ///< Data being read and de-compressed
  struct ZstrmDirection write; ///< Data being compressed and written
  struct Connection next_conn; ///< Underlying stream
};

/**
 * zstrm_malloc - Redirector function for zlib's malloc()
 * @param opaque Opaque zlib handle
 * @param items  Number of blocks
 * @param size   Size of blocks
 * @retval ptr Memory on the heap
 */
static void *zstrm_malloc(void *opaque, unsigned int items, unsigned int size)
{
  return mutt_mem_calloc(items, size);
}

/**
 * zstrm_free - Redirector function for zlib's free()
 * @param opaque  Opaque zlib handle
 * @param address Memory to free
 */
static void zstrm_free(void *opaque, void *address)
{
  FREE(&address);
}

/**
 * zstrm_open - Open a socket - Implements Connection::open()
 * @retval -1 Always
 *
 * Cannot open a zlib connection, must wrap an existing one
 */
static int zstrm_open(struct Connection *conn)
{
  return -1;
}

/**
 * zstrm_close - Close a socket - Implements Connection::close()
 */
static int zstrm_close(struct Connection *conn)
{
  struct ZstrmContext *zctx = conn->sockdata;

  int rc = zctx->next_conn.close(&zctx->next_conn);

  mutt_debug(LL_DEBUG5, "read %llu->%llu (%.1fx) wrote %llu<-%llu (%.1fx)\n",
             zctx->read.z.total_in, zctx->read.z.total_out,
             (float) zctx->read.z.total_out / (float) zctx->read.z.total_in,
             zctx->write.z.total_in, zctx->write.z.total_out,
             (float) zctx->write.z.total_in / (float) zctx->write.z.total_out);

  // Restore the Connection's original functions
  conn->sockdata = zctx->next_conn.sockdata;
  conn->open = zctx->next_conn.open;
  conn->close = zctx->next_conn.close;
  conn->read = zctx->next_conn.read;
  conn->write = zctx->next_conn.write;
  conn->poll = zctx->next_conn.poll;

  inflateEnd(&zctx->read.z);
  deflateEnd(&zctx->write.z);
  FREE(&zctx->read.buf);
  FREE(&zctx->write.buf);
  FREE(&zctx);

  return rc;
}

/**
 * zstrm_read - Read compressed data from a socket - Implements Connection::read()
 */
static int zstrm_read(struct Connection *conn, char *buf, size_t len)
{
  struct ZstrmContext *zctx = conn->sockdata;
  int rc = 0;
  int zrc = 0;

retry:
  if (zctx->read.stream_eof)
    return 0;

  /* when avail_out was 0 on last call, we need to call inflate again, because
   * more data might be available using the current input, so avoid callling
   * read on the underlying stream in that case (for it might block) */
  if ((zctx->read.pos == 0) && !zctx->read.conn_eof)
  {
    rc = zctx->next_conn.read(&zctx->next_conn, zctx->read.buf, zctx->read.len);
    mutt_debug(LL_DEBUG5, "consuming data from next stream: %d bytes\n", rc);
    if (rc < 0)
      return rc;
    else if (rc == 0)
      zctx->read.conn_eof = true;
    else
      zctx->read.pos += rc;
  }

  zctx->read.z.avail_in = (uInt) zctx->read.pos;
  zctx->read.z.next_in = (Bytef *) zctx->read.buf;
  zctx->read.z.avail_out = (uInt) len;
  zctx->read.z.next_out = (Bytef *) buf;

  zrc = inflate(&zctx->read.z, Z_SYNC_FLUSH);
  mutt_debug(LL_DEBUG5, "rc=%d, consumed %u/%u bytes, produced %u/%u bytes\n",
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
        mutt_debug(LL_DEBUG5, "inflate just consumed\n");
        goto retry;
      }
      break;

    case Z_STREAM_END: /* everything flushed, nothing remaining */
      mutt_debug(LL_DEBUG5, "inflate returned Z_STREAM_END.\n");
      zrc = len - zctx->read.z.avail_out; /* "returned" bytes */
      zctx->read.stream_eof = true;
      break;

    case Z_BUF_ERROR: /* no progress was possible */
      if (!zctx->read.conn_eof)
      {
        mutt_debug(LL_DEBUG5, "inflate returned Z_BUF_ERROR. retrying.\n");
        goto retry;
      }
      zrc = 0;
      break;

    default:
      /* bail on other rcs, such as Z_DATA_ERROR, or Z_MEM_ERROR */
      mutt_debug(LL_DEBUG5, "inflate returned %d. aborting.\n", zrc);
      zrc = -1;
      break;
  }

  return zrc;
}

/**
 * zstrm_poll - Checks whether reads would block - Implements Connection::poll()
 */
static int zstrm_poll(struct Connection *conn, time_t wait_secs)
{
  struct ZstrmContext *zctx = conn->sockdata;

  mutt_debug(LL_DEBUG5, "%s\n",
             (zctx->read.z.avail_out == 0) || (zctx->read.pos > 0) ?
                 "last read wrote full buffer" :
                 "falling back on next stream");
  if ((zctx->read.z.avail_out == 0) || (zctx->read.pos > 0))
    return 1;

  return zctx->next_conn.poll(&zctx->next_conn, wait_secs);
}

/**
 * zstrm_write - Write compressed data to a socket - Implements Connection::write()
 */
static int zstrm_write(struct Connection *conn, const char *buf, size_t count)
{
  struct ZstrmContext *zctx = conn->sockdata;
  int rc;

  zctx->write.z.avail_in = (uInt) count;
  zctx->write.z.next_in = (Bytef *) buf;
  zctx->write.z.avail_out = (uInt) zctx->write.len;
  zctx->write.z.next_out = (Bytef *) zctx->write.buf;

  do
  {
    int zrc = deflate(&zctx->write.z, Z_PARTIAL_FLUSH);
    if (zrc == Z_OK)
    {
      /* push out produced data to the underlying stream */
      zctx->write.pos = zctx->write.len - zctx->write.z.avail_out;
      char *wbufp = zctx->write.buf;
      mutt_debug(LL_DEBUG5, "deflate consumed %d/%d bytes\n",
                 count - zctx->write.z.avail_in, count);
      while (zctx->write.pos > 0)
      {
        rc = zctx->next_conn.write(&zctx->next_conn, wbufp, zctx->write.pos);
        mutt_debug(LL_DEBUG5, "next stream wrote: %d bytes\n", rc);
        if (rc < 0)
          return -1; /* we can't recover from write failure */

        wbufp += rc;
        zctx->write.pos -= rc;
      }

      /* see if there's more for us to do, retry if the output buffer
       * was full (there may be something in zlib buffers), and retry
       * when there is still available input data */
      if ((zctx->write.z.avail_out != 0) && (zctx->write.z.avail_in == 0))
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
  } while (true);

  rc = (int) count;
  return (rc <= 0) ? 1 : rc; /* avoid wrong behaviour due to overflow */
}

/**
 * mutt_zstrm_wrap_conn - Wrap a compression layer around a Connection
 * @param conn Connection to wrap
 *
 * Replace the read/write functions with our compression functions.
 * After reading from the socket, we decompress and pass on the data.
 * Before writing to a socket, we compress the data.
 */
void mutt_zstrm_wrap_conn(struct Connection *conn)
{
  struct ZstrmContext *zctx = mutt_mem_calloc(1, sizeof(struct ZstrmContext));

  /* store wrapped stream as next stream */
  zctx->next_conn.fd = conn->fd;
  zctx->next_conn.sockdata = conn->sockdata;
  zctx->next_conn.open = conn->open;
  zctx->next_conn.close = conn->close;
  zctx->next_conn.read = conn->read;
  zctx->next_conn.write = conn->write;
  zctx->next_conn.poll = conn->poll;

  /* replace connection with our wrappers, where appropriate */
  conn->sockdata = zctx;
  conn->open = zstrm_open;
  conn->read = zstrm_read;
  conn->write = zstrm_write;
  conn->close = zstrm_close;
  conn->poll = zstrm_poll;

  /* allocate/setup (de)compression buffers */
  zctx->read.len = 8192;
  zctx->read.buf = mutt_mem_malloc(zctx->read.len);
  zctx->read.pos = 0;
  zctx->write.len = 8192;
  zctx->write.buf = mutt_mem_malloc(zctx->write.len);
  zctx->write.pos = 0;

  /* initialise zlib for inflate and deflate for RFC4978 */
  zctx->read.z.zalloc = zstrm_malloc;
  zctx->read.z.zfree = zstrm_free;
  zctx->read.z.opaque = NULL;
  zctx->read.z.avail_out = zctx->read.len;
  inflateInit2(&zctx->read.z, -15);
  zctx->write.z.zalloc = zstrm_malloc;
  zctx->write.z.zfree = zstrm_free;
  zctx->write.z.opaque = NULL;
  zctx->write.z.avail_out = zctx->write.len;
  deflateInit2(&zctx->write.z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
}
