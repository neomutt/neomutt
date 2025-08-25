/**
 * @file
 * ctype(3) wrapper functions
 *
 * @authors
 * Copyright (C) 2025 Thomas Klausne <wiz@gatalith.at>
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

#ifndef MUTT_MUTT_CTYPE_H
#define MUTT_MUTT_CTYPE_H

#include <ctype.h>

#define mutt_isalnum(arg)	(isascii(arg) ? isalnum((unsigned char)arg) : 0)
#define mutt_isalpha(arg)	(isascii(arg) ? isalpha((unsigned char)arg) : 0)
#define mutt_isdigit(arg)	(isascii(arg) ? isdigit((unsigned char)arg) : 0)
#define mutt_ispunct(arg)	(isascii(arg) ? ispunct((unsigned char)arg) : 0)
#define mutt_isspace(arg)	(isascii(arg) ? isspace((unsigned char)arg) : 0)
#define mutt_isxdigit(arg)	(isascii(arg) ? isxdigit((unsigned char)arg) : 0)
#define mutt_tolower(arg)	(isascii(arg) ? tolower((unsigned char)arg) : (arg))
#define mutt_toupper(arg)	(isascii(arg) ? toupper((unsigned char)arg) : (arg))

#endif /* MUTT_MUTT_CTYPE_H */
