/**
 * @file
 * Global variables
 *
 * @authors
 * Copyright (C) 2023-2026 Richard Russon <rich@flatcap.org>
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
 * @page main_globals Global variables
 *
 * Global variables
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>

bool ErrorBufMessage; ///< true if the last message was an error
char ErrorBuf[1024];  ///< Copy of the last error message

char *ShortHostname = NULL; ///< Short version of the hostname

char *CurrentFolder = NULL; ///< Currently selected mailbox
char *LastFolder = NULL;    ///< Previously selected mailbox

/* pseudo options */
// clang-format off
#ifdef USE_AUTOCRYPT
bool OptAutocryptGpgme;     ///< (pseudo) use Autocrypt context inside ncrypt/crypt_gpgme.c
#endif
bool OptDontHandlePgpKeys;  ///< (pseudo) used to extract PGP keys
bool OptForceRefresh;       ///< (pseudo) refresh even during macros
bool OptGui;                ///< (pseudo) when the gui (and curses) are started
bool OptKeepQuiet;          ///< (pseudo) shut up the message and refresh functions while we are executing an external program
bool OptMsgErr;             ///< (pseudo) used by mutt_error/mutt_message
bool OptNeedRescore;        ///< (pseudo) set when the 'score' command is used
bool OptNeedResort;         ///< (pseudo) used to force a re-sort
bool OptNews;               ///< (pseudo) used to change reader mode
bool OptNewsSend;           ///< (pseudo) used to change behavior when posting
bool OptPgpCheckTrust;      ///< (pseudo) used by dlg_pgp()
bool OptResortInit;         ///< (pseudo) used to force the next resort to be from scratch
bool OptSortSubthreads;     ///< (pseudo) used when $sort_aux changes
// clang-format on
