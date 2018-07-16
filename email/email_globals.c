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
#include <stdbool.h>
#include "mutt/mutt.h"

/* Config items */
bool MarkOld;
struct Regex *ReplyRegex;
char *SendCharset;
char *SpamSeparator;
bool Weed;

/* Global variables */
struct RegexList *NoSpamList;
struct ReplaceList *SpamList;
struct ListHead Ignore = STAILQ_HEAD_INITIALIZER(Ignore);
struct ListHead UnIgnore = STAILQ_HEAD_INITIALIZER(UnIgnore);
