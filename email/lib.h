/**
 * @file
 * Structs that make up an email
 *
 * @authors
 * Copyright (C) 2018-2026 Richard Russon <rich@flatcap.org>
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
 * @page lib_email Email code
 *
 * Structs that make up an email
 *
 * | File                   | Description               |
 * | :--------------------- | :------------------------ |
 * | email/body.c           | @subpage email_body       |
 * | email/config.c         | @subpage email_config     |
 * | email/copy_body.c      | @subpage email_copy_body  |
 * | email/copy_email.c     | @subpage email_copy_email |
 * | email/email.c          | @subpage email_email      |
 * | email/enriched.c       | @subpage email_enriched   |
 * | email/envelope.c       | @subpage email_envelope   |
 * | email/from.c           | @subpage email_from       |
 * | email/globals.c        | @subpage email_globals    |
 * | email/group.c          | @subpage email_group      |
 * | email/handler.c        | @subpage email_handler    |
 * | email/header.c         | @subpage email_header     |
 * | email/ignore.c         | @subpage email_ignore     |
 * | email/mailcap.c        | @subpage email_mailcap    |
 * | email/mime.c           | @subpage email_mime       |
 * | email/module.c         | @subpage email_module     |
 * | email/parameter.c      | @subpage email_parameter  |
 * | email/parse.c          | @subpage email_parse      |
 * | email/rfc2047.c        | @subpage email_rfc2047    |
 * | email/rfc2231.c        | @subpage email_rfc2231    |
 * | email/rfc3676.c        | @subpage email_rfc3676    |
 * | email/score.c          | @subpage email_score      |
 * | email/sort.c           | @subpage email_sort       |
 * | email/spam.c           | @subpage email_spam       |
 * | email/tags.c           | @subpage email_tags       |
 * | email/thread.c         | @subpage email_thread     |
 * | email/url.c            | @subpage email_url        |
 *
 * @image html libemail.svg
 */

#ifndef MUTT_EMAIL_LIB_H
#define MUTT_EMAIL_LIB_H

// IWYU pragma: begin_keep
#include "body.h"
#include "content.h"
#include "copy_body.h"
#include "copy_email.h"
#include "email.h"
#include "enriched.h"
#include "envelope.h"
#include "from.h"
#include "globals2.h"
#include "group.h"
#include "handler.h"
#include "header.h"
#include "ignore.h"
#include "mailcap.h"
#include "mime.h"
#include "parameter.h"
#include "parse.h"
#include "rfc2047.h"
#include "rfc2231.h"
#include "rfc3676.h"
#include "score.h"
#include "sort.h"
#include "spam.h"
#include "tags.h"
#include "thread.h"
#include "url.h"
// IWYU pragma: end_keep

#endif /* MUTT_EMAIL_LIB_H */
