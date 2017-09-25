/**
 * @file
 * Manage IMAP BODYSTRUCTURE parsing
 *
 * @authors
 * Copyright (C) 2017 Mehdi Abaakouk <sileht@sileht.net>
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

#ifndef _IMAP_BODYSTRUCTURE_H
#define _IMAP_BODYSTRUCTURE_H

struct ImapAccountData;
struct Body;

char *body_struct_parse(struct ImapAccountData *adata, struct Body *body, char *s);

#endif /* _IMAP_BODYSTRUCTURE_H */
