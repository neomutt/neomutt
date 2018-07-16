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

#ifndef _EMAIL_GLOBALS_H
#define _EMAIL_GLOBALS_H

#include <stdbool.h>
#include "mutt/mutt.h"

/* Config items */
extern bool                MarkOld;
extern struct Regex *      ReplyRegex;
extern char *              SendCharset;
extern char *              SpamSeparator;
extern bool                Weed;

/* Global variables */
extern struct ListHead     Ignore;
extern struct RegexList *  NoSpamList;
extern struct ReplaceList *SpamList;
extern struct ListHead     UnIgnore;

#endif /* _EMAIL_GLOBALS_H */
