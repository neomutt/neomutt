/**
 * @file
 * Convenience wrapper for the send headers
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page lib_send SEND: Shared code for sending Emails

 * Shared code for sending Emails
 *
 * | File             | Description             |
 * | :--------------- | :---------------------- |
 * | send/body.c      | @subpage send_body      |
 * | send/config.c    | @subpage send_config    |
 * | send/header.c    | @subpage send_header    |
 * | send/multipart.c | @subpage send_multipart |
 * | send/send.c      | @subpage send_send      |
 * | send/sendlib.c   | @subpage send_sendlib   |
 * | send/sendmail.c  | @subpage send_sendmail  |
 * | send/smtp.c      | @subpage send_smtp      |
 */

#ifndef MUTT_SEND_LIB_H
#define MUTT_SEND_LIB_H

#include <stdbool.h>
// IWYU pragma: begin_exports
#include "body.h"
#include "header.h"
#include "multipart.h"
#include "send.h"
#include "sendlib.h"
#include "sendmail.h"
#include "smtp.h"
// IWYU pragma: end_exports

#endif /* MUTT_SEND_LIB_H */
