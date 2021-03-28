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
#include "mutt/lib.h"

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
