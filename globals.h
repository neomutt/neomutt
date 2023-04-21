/**
 * @file
 * Global variables
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

#ifndef MUTT_GLOBALS_H
#define MUTT_GLOBALS_H

#include "config.h"
#include <signal.h> // IWYU pragma: keep
#include <stdbool.h>
#include "mutt/lib.h"
#include "menu/lib.h"

extern bool ErrorBufMessage; ///< true if the last message was an error
extern char ErrorBuf[256];   ///< Copy of the last error message

extern char *HomeDir;        ///< User's home directory
extern char *ShortHostname;  ///< Short version of the hostname

extern char *Username;       ///< User's login name

extern char *CurrentFolder;  ///< Currently selected mailbox
extern char *LastFolder;     ///< Previously selected mailbox

extern const char *GitVer;

/* Lists of strings */
extern struct ListHead AlternativeOrderList; ///< List of preferred mime types to display
extern struct ListHead AutoViewList;         ///< List of mime types to auto view
extern struct ListHead HeaderOrderList;      ///< List of header fields in the order they should be displayed
extern struct ListHead MimeLookupList;       ///< List of mime types that that shouldn't use the mailcap entry
extern struct ListHead Muttrc;               ///< List of config files to read
extern struct ListHead TempAttachmentsList;  ///< List of temporary files for displaying attachments
extern struct ListHead UserHeader;           ///< List of custom headers to add to outgoing emails

/* flags for received signals */
extern SIG_ATOMIC_VOLATILE_T SigInt;   ///< true after SIGINT is received
extern SIG_ATOMIC_VOLATILE_T SigWinch; ///< true after SIGWINCH is received

extern enum MenuType CurrentMenu; ///< Current Menu, e.g. #MENU_PAGER

/* pseudo options */
extern bool OptAttachMsg;           ///< (pseudo) used by attach-message
#ifdef USE_AUTOCRYPT
extern bool OptAutocryptGpgme;      ///< (pseudo) use Autocrypt context inside ncrypt/crypt_gpgme.c
#endif
extern bool OptDontHandlePgpKeys;   ///< (pseudo) used to extract PGP keys
extern bool OptForceRefresh;        ///< (pseudo) refresh even during macros
extern bool OptIgnoreMacroEvents;   ///< (pseudo) don't process macro/push/exec events while set
extern bool OptKeepQuiet;           ///< (pseudo) shut up the message and refresh functions while we are executing an external program
extern bool OptMenuPopClearScreen;  ///< (pseudo) clear the screen when popping the last menu
extern bool OptMsgErr;              ///< (pseudo) used by mutt_error/mutt_message
extern bool OptNeedRescore;         ///< (pseudo) set when the 'score' command is used
extern bool OptNeedResort;          ///< (pseudo) used to force a re-sort
#ifdef USE_NNTP
extern bool OptNews;                ///< (pseudo) used to change reader mode
extern bool OptNewsSend;            ///< (pseudo) used to change behavior when posting
#endif
extern bool OptNoCurses;            ///< (pseudo) when sending in batch mode
extern bool OptPgpCheckTrust;       ///< (pseudo) used by dlg_select_pgp_key()
extern bool OptRedrawTree;          ///< (pseudo) redraw the thread tree
extern bool OptResortInit;          ///< (pseudo) used to force the next resort to be from scratch
extern bool OptSearchInvalid;       ///< (pseudo) used to invalidate the search pattern
extern bool OptSearchReverse;       ///< (pseudo) used by ci_search_command
extern bool OptSortSubthreads;      ///< (pseudo) used when $sort_aux changes

#endif /* MUTT_GLOBALS_H */
