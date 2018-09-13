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

#ifndef MUTT_REMAILER_H
#define MUTT_REMAILER_H

#include <stddef.h>

struct ListHead;
struct Email;

/* These Config Variables are only used in remailer.c */
extern char *MixEntryFormat;
extern char *Mixmaster;

/* Mixmaster's maximum chain length.  Don't change this. */

#define MAXMIXES 19

/**
 * struct Remailer - A Mixmaster remailer
 */
struct Remailer
{
  int num;
  char *shortname;
  char *addr;
  char *ver;
  int caps;
};

/**
 * struct MixChain - A Mixmaster chain
 */
struct MixChain
{
  size_t cl;
  int ch[MAXMIXES];
};

int mix_send_message(struct ListHead *chain, const char *tempfile);
int mix_check_message(struct Email *msg);
void mix_make_chain(struct ListHead *chainhead);

#endif /* MUTT_REMAILER_H */
