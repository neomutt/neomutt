/**
 * @file
 * GUI display a file/email/help in a viewport with paging
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page pager_pager GUI display a file/email/help in a viewport with paging
 *
 * GUI display a file/email/help in a viewport with paging
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_attach.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "pbar.h"
#include "private_data.h"
#include "protos.h"
#include "recvattach.h"
#include "recvcmd.h"
#include "status.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/mdata.h" // IWYU pragma: keep
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/**
 * pager_window_new - Create a new Pager Window (list of Emails)
 * @retval ptr New Window
 */
struct MuttWindow *pager_window_new(void)
{
  struct MuttWindow *win = menu_new_window(MENU_MAIN, NeoMutt->sub);

  return win;
}
