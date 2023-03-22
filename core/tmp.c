/**
 * @file
 * Create Temporary Files
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page core_tmp Create Temporary Files
 *
 * Create Temporary Files
 */

#include "config.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "globals.h"
#include "neomutt.h"

/**
 * mutt_buffer_mktemp_full - Create a temporary file
 * @param buf    Buffer for result
 * @param prefix Prefix for filename
 * @param suffix Suffix for filename
 * @param src    Source file of caller
 * @param line   Source line number of caller
 */
void mutt_buffer_mktemp_full(struct Buffer *buf, const char *prefix,
                             const char *suffix, const char *src, int line)
{
  const char *const c_tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
  mutt_buffer_printf(buf, "%s/%s-%s-%d-%d-%" PRIu64 "%s%s", NONULL(c_tmp_dir),
                     NONULL(prefix), NONULL(ShortHostname), (int) getuid(),
                     (int) getpid(), mutt_rand64(), suffix ? "." : "", NONULL(suffix));

  mutt_debug(LL_DEBUG3, "%s:%d: mutt_mktemp returns \"%s\"\n", src, line,
             mutt_buffer_string(buf));
  if (unlink(mutt_buffer_string(buf)) && (errno != ENOENT))
  {
    mutt_debug(LL_DEBUG1, "%s:%d: ERROR: unlink(\"%s\"): %s (errno %d)\n", src,
               line, mutt_buffer_string(buf), strerror(errno), errno);
  }
}

/**
 * mutt_file_mkstemp_full - Create temporary file safely
 * @param file Source file of caller
 * @param line Source line number of caller
 * @param func Function name of caller
 * @retval ptr  FILE handle
 * @retval NULL Error, see errno
 *
 * Create and immediately unlink a temp file using mkstemp().
 */
FILE *mutt_file_mkstemp_full(const char *file, int line, const char *func)
{
  char name[PATH_MAX] = { 0 };

  const char *const c_tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
  int n = snprintf(name, sizeof(name), "%s/neomutt-XXXXXX", NONULL(c_tmp_dir));
  if (n < 0)
    return NULL;

  int fd = mkstemp(name);
  if (fd == -1)
    return NULL;

  FILE *fp = fdopen(fd, "w+");

  if ((unlink(name) != 0) && (errno != ENOENT))
  {
    mutt_file_fclose(&fp);
    return NULL;
  }

  MuttLogger(0, file, line, func, 1, "created temp file '%s'\n", name);
  return fp;
}

/**
 * mutt_mktemp_full - Create a temporary filename
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 * @param prefix Prefix for filename
 * @param suffix Suffix for filename
 * @param src    Source file of caller
 * @param line   Source line number of caller
 *
 * @note This doesn't create the file, only the name
 */
void mutt_mktemp_full(char *buf, size_t buflen, const char *prefix,
                      const char *suffix, const char *src, int line)
{
  const char *const c_tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
  size_t n = snprintf(buf, buflen, "%s/%s-%s-%d-%d-%" PRIu64 "%s%s",
                      NONULL(c_tmp_dir), NONULL(prefix), NONULL(ShortHostname),
                      (int) getuid(), (int) getpid(), mutt_rand64(),
                      suffix ? "." : "", NONULL(suffix));
  if (n >= buflen)
  {
    mutt_debug(LL_DEBUG1, "%s:%d: ERROR: insufficient buffer space to hold temporary filename! buflen=%zu but need %zu\n",
               src, line, buflen, n);
  }
  mutt_debug(LL_DEBUG3, "%s:%d: mutt_mktemp returns \"%s\"\n", src, line, buf);
  if ((unlink(buf) != 0) && (errno != ENOENT))
  {
    mutt_debug(LL_DEBUG1, "%s:%d: ERROR: unlink(\"%s\"): %s (errno %d)\n", src,
               line, buf, strerror(errno), errno);
  }
}
