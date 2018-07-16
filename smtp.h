/**
 * @file
 * Send email to an SMTP server
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

#ifndef MUTT_SMTP_H
#define MUTT_SMTP_H

struct Address;

/* These Config Variables are only used in smtp.c */
extern char *SmtpAuthenticators;

#ifdef USE_SMTP
int mutt_smtp_send(const struct Address *from, const struct Address *to, const struct Address *cc, const struct Address *bcc, const char *msgfile, int eightbit);
#endif

#endif /* MUTT_SMTP_H */
