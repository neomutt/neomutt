/**
 * @file
 * Representation of an email address
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_ADDRESS_H
#define _MUTT_ADDRESS_H

#include <stdbool.h>

/**
 * struct Address - An email address
 */
struct Address
{
  char *personal; /**< real name of address */
  char *mailbox;  /**< mailbox and host address */
  int group;      /**< group mailbox? */
  struct Address *next;
  bool is_intl : 1;
  bool intl_checked : 1;
};

#endif /* _MUTT_ADDRESS_H */
