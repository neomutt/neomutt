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
 * @page email EMAIL: Structs that make up an email
 *
 * Structs that make up an email
 *
 * | File                   | Description              |
 * | :--------------------- | :----------------------- |
 * | email/address.c        | @subpage email_address   |
 * | email/attach.c         | @subpage email_attach    |
 * | email/body.c           | @subpage email_body      |
 * | email/email_globals.c  | @subpage email_globals   |
 * | email/envelope.c       | @subpage email_envelope  |
 * | email/from.c           | @subpage email_from      |
 * | email/header.c         | @subpage email_header    |
 * | email/idna.c           | @subpage email_idna      |
 * | email/mime.c           | @subpage email_mime      |
 * | email/parameter.c      | @subpage email_parameter |
 * | email/parse.c          | @subpage email_parse     |
 * | email/rfc2047.c        | @subpage email_rfc2047   |
 * | email/rfc2231.c        | @subpage email_rfc2231   |
 * | email/tags.c           | @subpage email_tags      |
 * | email/thread.c         | @subpage email_thread    |
 * | email/url.c            | @subpage email_url       |
 *
 * @image html libemail.svg
 */

#ifndef _EMAIL_EMAIL_H
#define _EMAIL_EMAIL_H

#include "address.h"
#include "attach.h"
#include "body.h"
#include "content.h"
#include "email_globals.h"
#include "envelope.h"
#include "from.h"
#include "header.h"
#include "idna2.h"
#include "mime.h"
#include "parameter.h"
#include "parse.h"
#include "rfc2047.h"
#include "rfc2231.h"
#include "tags.h"
#include "thread.h"
#include "url.h"

#endif /* _EMAIL_EMAIL_H */
