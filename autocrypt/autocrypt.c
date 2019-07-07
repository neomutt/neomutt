/**
 * @file
 * XXX
 *
 * @authors
 * Copyright (C) 2019 Kevin J. McCarthy <kevin@8t8.us>
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "autocrypt_private.h"
#include "mutt.h"
#include "autocrypt.h"
#include "curs_lib.h"
#include "globals.h"

static int autocrypt_dir_init(int can_create)
{
  int rv = 0;
  struct stat sb;
  struct Buffer *prompt = NULL;

  if (!stat(C_AutocryptDir, &sb))
    return 0;

  if (!can_create)
    return -1;

  prompt = mutt_buffer_pool_get();
  mutt_buffer_printf(prompt, _("%s does not exist. Create it?"), C_AutocryptDir);
  if (mutt_yesorno(mutt_b2s(prompt), MUTT_YES) == MUTT_YES)
  {
    if (mutt_file_mkdir(C_AutocryptDir, S_IRWXU) < 0)
    {
      mutt_error(_("Can't create %s: %s."), C_AutocryptDir, strerror(errno));
      rv = -1;
    }
  }

  mutt_buffer_pool_release(&prompt);
  return rv;
}

int mutt_autocrypt_init(int can_create)
{
  if (AutocryptDB)
    return 0;

  if (!C_Autocrypt || !C_AutocryptDir)
    return -1;

  if (autocrypt_dir_init(can_create))
    goto bail;

  if (mutt_autocrypt_gpgme_init())
    goto bail;

  if (mutt_autocrypt_db_init(can_create))
    goto bail;

  return 0;

bail:
  C_Autocrypt = false;
  mutt_autocrypt_db_close();
  return -1;
}

void mutt_autocrypt_cleanup(void)
{
  mutt_autocrypt_db_close();
}
