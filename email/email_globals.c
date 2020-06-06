/**
 * @file
 * Email Global Variables
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page email_globals Email Global Variables
 *
 * These global variables are private to the email library.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"

/* Config items */
bool C_AutoSubscribe; ///< Config: Automatically check if the user is subscribed to a mailing list
bool C_MarkOld = false; ///< Config: Mark new emails as old when leaving the mailbox
struct Regex *C_ReplyRegex = NULL; ///< Config: Regex to match message reply subjects like "re: "
char *C_SendCharset = NULL;   ///< Config: Character sets for outgoing mail
char *C_SpamSeparator = NULL; ///< Config: Separator for multiple spam headers
bool C_Weed = false; ///< Config: Filter headers when displaying/forwarding/printing/replying

/* Global variables */
struct RegexList NoSpamList = STAILQ_HEAD_INITIALIZER(NoSpamList);
struct ReplaceList SpamList = STAILQ_HEAD_INITIALIZER(SpamList);
struct ListHead Ignore = STAILQ_HEAD_INITIALIZER(Ignore);
struct ListHead UnIgnore = STAILQ_HEAD_INITIALIZER(UnIgnore);
struct ListHead MailToAllow = STAILQ_HEAD_INITIALIZER(MailToAllow);
struct HashTable *AutoSubscribeCache;
struct RegexList UnSubscribedLists = STAILQ_HEAD_INITIALIZER(UnSubscribedLists);
struct RegexList MailLists = STAILQ_HEAD_INITIALIZER(MailLists);
struct RegexList UnMailLists = STAILQ_HEAD_INITIALIZER(UnMailLists);
struct RegexList SubscribedLists = STAILQ_HEAD_INITIALIZER(SubscribedLists);
struct ReplaceList SubjectRegexList = STAILQ_HEAD_INITIALIZER(SubjectRegexList);
