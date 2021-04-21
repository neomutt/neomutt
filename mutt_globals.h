/**
 * @file
 * Hundreds of global variables to back the user variables
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2016 Michael R. Elkins <me@mutt.org>
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
#include "keymap.h"
#include "where.h"

#ifdef MAIN_C
/* so that global vars get included */
#include "mx.h"
#include "sort.h"
#include "ncrypt/lib.h"
#endif /* MAIN_C */

WHERE struct Context *Context;

WHERE bool ErrorBufMessage;            ///< true if the last message was an error
WHERE char ErrorBuf[256];              ///< Copy of the last error message

WHERE char *HomeDir;       ///< User's home directory
WHERE char *ShortHostname; ///< Short version of the hostname

WHERE char *Username; ///< User's login name

WHERE char *CurrentFolder; ///< Currently selected mailbox
WHERE char *LastFolder;    ///< Previously selected mailbox

extern const char *GitVer;

WHERE struct HashTable *TagFormats; ///< Hash Table of tag-formats (tag -> format string)

/* Lists of strings */
WHERE struct ListHead AlternativeOrderList INITVAL(STAILQ_HEAD_INITIALIZER(AlternativeOrderList)); ///< List of preferred mime types to display
WHERE struct ListHead AutoViewList INITVAL(STAILQ_HEAD_INITIALIZER(AutoViewList));                 ///< List of mime types to auto view
WHERE struct ListHead HeaderOrderList INITVAL(STAILQ_HEAD_INITIALIZER(HeaderOrderList));           ///< List of header fields in the order they should be displayed
WHERE struct ListHead MimeLookupList INITVAL(STAILQ_HEAD_INITIALIZER(MimeLookupList));             ///< List of mime types that that shouldn't use the mailcap entry
WHERE struct ListHead Muttrc INITVAL(STAILQ_HEAD_INITIALIZER(Muttrc));                             ///< List of config files to read
WHERE struct ListHead TempAttachmentsList INITVAL(STAILQ_HEAD_INITIALIZER(TempAttachmentsList));   ///< List of temporary files for displaying attachments
WHERE struct ListHead UserHeader INITVAL(STAILQ_HEAD_INITIALIZER(UserHeader));                     ///< List of custom headers to add to outgoing emails

/* flags for received signals */
WHERE SIG_ATOMIC_VOLATILE_T SigInt;   ///< true after SIGINT is received
WHERE SIG_ATOMIC_VOLATILE_T SigWinch; ///< true after SIGWINCH is received

WHERE enum MenuType CurrentMenu; ///< Current Menu, e.g. #MENU_PAGER

#endif /* MUTT_GLOBALS_H */
