/**
 * @file
 * Structs that make up an email
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

/**
 * @page mutt Structs that make up an email
 *
 * Structs that make up an email
 *
 * | File              | Description        |
 * | :---------------- | :----------------- |
 * | email/address.c   | @subpage address   |
 * | email/attach.c    | @subpage attach    |
 * | email/body.c      | @subpage body      |
 * | email/envelope.c  | @subpage envelope  |
 * | email/header.c    | @subpage header    |
 * | email/parameter.c | @subpage parameter |
 * | email/tags.c      | @subpage tags      |
 * | email/thread.c    | @subpage thread    |
 *
 * @note The library is self-contained -- some files may depend on others in
 *       the library, but none depends on source from outside.
 */

#ifndef _EMAIL_EMAIL_H
#define _EMAIL_EMAIL_H

#include "address.h"
#include "attach.h"
#include "body.h"
#include "content.h"
#include "envelope.h"
#include "header.h"
#include "parameter.h"
#include "tags.h"
#include "thread.h"

#endif /* _EMAIL_EMAIL_H */
