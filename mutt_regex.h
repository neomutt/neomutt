/**
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 *
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

/*
 * A (more) generic interface to regular expression matching
 */

#ifndef _MUTT_REGEX_H
#define _MUTT_REGEX_H 1

#include <regex.h>

/* this is a non-standard option supported by Solaris 2.5.x which allows
 * patterns of the form \<...\>
 */
#ifndef REG_WORDS
#define REG_WORDS 0
#endif

#define REGCOMP(X,Y,Z) regcomp(X, Y, REG_WORDS|REG_EXTENDED|(Z))
#define REGEXEC(X,Y) regexec(&X, Y, (size_t)0, (regmatch_t *)0, (int)0)

typedef struct
{
  char *pattern;	/* printable version */
  regex_t *rx; 		/* compiled expression */
  int not;		/* do not match */
} REGEXP;

WHERE REGEXP Mask;
WHERE REGEXP QuoteRegexp;
WHERE REGEXP ReplyRegexp;
WHERE REGEXP Smileys;
WHERE REGEXP GecosMask;

#endif /* _MUTT_REGEX_H */
