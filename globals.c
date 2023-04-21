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

/**
 * @page neo_globals Global variables
 *
 * Global variables
 */

#include "config.h"
#include <signal.h> // IWYU pragma: keep
#include <stdbool.h>
#include "mutt/lib.h"
#include "menu/lib.h"

bool ErrorBufMessage; ///< true if the last message was an error
char ErrorBuf[256];   ///< Copy of the last error message

char *HomeDir;        ///< User's home directory
char *ShortHostname;  ///< Short version of the hostname

char *Username;       ///< User's login name

char *CurrentFolder;  ///< Currently selected mailbox
char *LastFolder;     ///< Previously selected mailbox

/* Lists of strings */
// clang-format off
struct ListHead AlternativeOrderList = STAILQ_HEAD_INITIALIZER(AlternativeOrderList); ///< List of preferred mime types to display
struct ListHead AutoViewList         = STAILQ_HEAD_INITIALIZER(AutoViewList);         ///< List of mime types to auto view
struct ListHead HeaderOrderList      = STAILQ_HEAD_INITIALIZER(HeaderOrderList);      ///< List of header fields in the order they should be displayed
struct ListHead MimeLookupList       = STAILQ_HEAD_INITIALIZER(MimeLookupList);       ///< List of mime types that that shouldn't use the mailcap entry
struct ListHead Muttrc               = STAILQ_HEAD_INITIALIZER(Muttrc);               ///< List of config files to read
struct ListHead TempAttachmentsList  = STAILQ_HEAD_INITIALIZER(TempAttachmentsList);  ///< List of temporary files for displaying attachments
struct ListHead UserHeader           = STAILQ_HEAD_INITIALIZER(UserHeader);           ///< List of custom headers to add to outgoing emails
// clang-format on

/* flags for received signals */
SIG_ATOMIC_VOLATILE_T SigInt;   ///< true after SIGINT is received
SIG_ATOMIC_VOLATILE_T SigWinch; ///< true after SIGWINCH is received

enum MenuType CurrentMenu;      ///< Current Menu, e.g. #MENU_PAGER

/* pseudo options */
// clang-format off
bool OptAttachMsg;          ///< (pseudo) used by attach-message
#ifdef USE_AUTOCRYPT
bool OptAutocryptGpgme;     ///< (pseudo) use Autocrypt context inside ncrypt/crypt_gpgme.c
#endif
bool OptDontHandlePgpKeys;  ///< (pseudo) used to extract PGP keys
bool OptForceRefresh;       ///< (pseudo) refresh even during macros
bool OptIgnoreMacroEvents;  ///< (pseudo) don't process macro/push/exec events while set
bool OptKeepQuiet;          ///< (pseudo) shut up the message and refresh functions while we are executing an external program
bool OptMenuPopClearScreen; ///< (pseudo) clear the screen when popping the last menu
bool OptMsgErr;             ///< (pseudo) used by mutt_error/mutt_message
bool OptNeedRescore;        ///< (pseudo) set when the 'score' command is used
bool OptNeedResort;         ///< (pseudo) used to force a re-sort
#ifdef USE_NNTP
bool OptNews;               ///< (pseudo) used to change reader mode
bool OptNewsSend;           ///< (pseudo) used to change behavior when posting
#endif
bool OptNoCurses;           ///< (pseudo) when sending in batch mode
bool OptPgpCheckTrust;      ///< (pseudo) used by dlg_select_pgp_key()
bool OptRedrawTree;         ///< (pseudo) redraw the thread tree
bool OptResortInit;         ///< (pseudo) used to force the next resort to be from scratch
bool OptSearchInvalid;      ///< (pseudo) used to invalidate the search pattern
bool OptSearchReverse;      ///< (pseudo) used by ci_search_command
bool OptSortSubthreads;     ///< (pseudo) used when $sort_aux changes
// clang-format on
