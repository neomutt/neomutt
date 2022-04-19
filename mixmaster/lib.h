/**
 * @file
 * Support of Mixmaster anonymous remailer
 *
 * @authors
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page lib_mixmaster Mixmaster
 *
 * Support of Mixmaster anonymous remailer
 *
 * | File                      | Description                      |
 * | :------------------------ | :------------------------------- |
 * | mixmaster/functions.c     | @subpage mixmaster_functions     |
 * | mixmaster/mixmaster.c     | @subpage mixmaster_mixmaster     |
 */

#ifndef MUTT_MIXMASTER_LIB_H
#define MUTT_MIXMASTER_LIB_H

struct Email;
struct ListHead;

int mix_send_message(struct ListHead *chain, const char *tempfile);
int mix_check_message(struct Email *e);
void dlg_select_mixmaster_chain(struct ListHead *chainhead);

#endif /* MUTT_MIXMASTER_LIB_H */
