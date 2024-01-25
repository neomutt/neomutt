/**
 * @file
 * MH shared functions
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
 * @page mh_shared MH shared functions
 *
 * MH shared functions
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "shared.h"
#include "globals.h"
#include "mdata.h"

/**
 * mh_umask - Create a umask from the mailbox directory
 * @param  m   Mailbox
 * @retval num Umask
 */
mode_t mh_umask(struct Mailbox *m)
{
  struct MhMboxData *mhata = mh_mdata_get(m);
  if (mhata && mhata->umask)
    return mhata->umask;

  struct stat st = { 0 };
  if (stat(mailbox_path(m), &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "stat failed on %s\n", mailbox_path(m));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * mh_mkstemp - Create a temporary file
 * @param[in]  m   Mailbox to create the file in
 * @param[out] fp  File handle
 * @param[out] tgt File name
 * @retval true Success
 * @retval false Failure
 */
bool mh_mkstemp(struct Mailbox *m, FILE **fp, char **tgt)
{
  int fd;
  char path[PATH_MAX] = { 0 };

  mode_t omask = umask(mh_umask(m));
  while (true)
  {
    snprintf(path, sizeof(path), "%s/.neomutt-%s-%d-%" PRIu64, mailbox_path(m),
             NONULL(ShortHostname), (int) getpid(), mutt_rand64());
    fd = open(path, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd == -1)
    {
      if (errno != EEXIST)
      {
        mutt_perror("%s", path);
        umask(omask);
        return false;
      }
    }
    else
    {
      *tgt = mutt_str_dup(path);
      break;
    }
  }
  umask(omask);

  *fp = fdopen(fd, "w");
  if (!*fp)
  {
    FREE(tgt);
    close(fd);
    unlink(path);
    return false;
  }

  return true;
}
