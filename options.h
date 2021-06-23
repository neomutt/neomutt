/**
 * @file
 * Handling of global boolean variables
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_OPTIONS_H
#define MUTT_OPTIONS_H

#include <stdbool.h>
#include "where.h"

/* pseudo options */

WHERE bool OptAttachMsg;           ///< (pseudo) used by attach-message
#ifdef USE_AUTOCRYPT
WHERE bool OptAutocryptGpgme;      ///< (pseudo) use Autocrypt context inside ncrypt/crypt_gpgme.c
#endif
WHERE bool OptDontHandlePgpKeys;   ///< (pseudo) used to extract PGP keys
WHERE bool OptForceRefresh;        ///< (pseudo) refresh even during macros
WHERE bool OptIgnoreMacroEvents;   ///< (pseudo) don't process macro/push/exec events while set
WHERE bool OptKeepQuiet;           ///< (pseudo) shut up the message and refresh functions while we are executing an external program
WHERE bool OptMenuPopClearScreen;  ///< (pseudo) clear the screen when popping the last menu
WHERE bool OptMsgErr;              ///< (pseudo) used by mutt_error/mutt_message
WHERE bool OptNeedRescore;         ///< (pseudo) set when the 'score' command is used
WHERE bool OptNeedResort;          ///< (pseudo) used to force a re-sort
#ifdef USE_NNTP
WHERE bool OptNews;                ///< (pseudo) used to change reader mode
WHERE bool OptNewsSend;            ///< (pseudo) used to change behavior when posting
#endif
WHERE bool OptNoCurses;            ///< (pseudo) when sending in batch mode
WHERE bool OptPgpCheckTrust;       ///< (pseudo) used by dlg_select_pgp_key()
WHERE bool OptRedrawTree;          ///< (pseudo) redraw the thread tree
WHERE bool OptResortInit;          ///< (pseudo) used to force the next resort to be from scratch
WHERE bool OptSearchInvalid;       ///< (pseudo) used to invalidate the search pattern
WHERE bool OptSearchReverse;       ///< (pseudo) used by ci_search_command
WHERE bool OptSortSubthreads;      ///< (pseudo) used when $sort_aux changes
WHERE bool OptViewAttach;          ///< (pseudo) signals that we are viewing attachments

#endif /* MUTT_OPTIONS_H */
