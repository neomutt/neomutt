/**
 * @file
 * Pattern definitions
 *
 * @authors
 * Copyright (C) 1996-2000,2006-2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page pattern_flags Pattern definitions
 *
 * Pattern definitions
 */

#include "config.h"
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "mutt.h"
#include "lib.h"

// clang-format off
/**
 * Flags - Lookup table for all patterns
 */
const struct PatternFlags Flags[] = {
  { 'A', MUTT_ALL, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~A
    N_("all messages") },
  { 'b', MUTT_PAT_BODY, MUTT_PC_FULL_MSG | MUTT_PC_SEND_MODE_SEARCH, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~b
    N_("messages whose body matches EXPR") },
  { 'B', MUTT_PAT_WHOLE_MSG, MUTT_PC_FULL_MSG | MUTT_PC_SEND_MODE_SEARCH, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~B
    N_("messages whose body or headers match EXPR") },
  { 'c', MUTT_PAT_CC, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~c
    N_("messages whose CC header matches EXPR") },
  { 'C', MUTT_PAT_RECIPIENT, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~C
    N_("messages whose recipient matches EXPR") },
  { 'd', MUTT_PAT_DATE, 0, EAT_DATE,
    // L10N: Pattern Completion Menu description for ~d
    N_("messages sent in DATERANGE") },
  { 'D', MUTT_DELETED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~D
    N_("deleted messages") },
  { 'e', MUTT_PAT_SENDER, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~e
    N_("messages whose Sender header matches EXPR") },
  { 'E', MUTT_EXPIRED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~E
    N_("expired messages") },
  { 'f', MUTT_PAT_FROM, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~f
    N_("messages whose From header matches EXPR") },
  { 'F', MUTT_FLAG, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~F
    N_("flagged messages") },
  { 'g', MUTT_PAT_CRYPT_SIGN, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~g
    N_("cryptographically signed messages") },
  { 'G', MUTT_PAT_CRYPT_ENCRYPT, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~G
    N_("cryptographically encrypted messages") },
  { 'h', MUTT_PAT_HEADER, MUTT_PC_FULL_MSG | MUTT_PC_SEND_MODE_SEARCH, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~h
    N_("messages whose header matches EXPR") },
  { 'H', MUTT_PAT_HORMEL, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~H
    N_("messages whose spam tag matches EXPR") },
  { 'i', MUTT_PAT_ID, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~i
    N_("messages whose Message-ID matches EXPR") },
  { 'I', MUTT_PAT_ID_EXTERNAL, 0, EAT_QUERY,
    // L10N: Pattern Completion Menu description for ~I
    N_("messages whose Message-ID is included in the results returned from an external search program") },
  { 'k', MUTT_PAT_PGP_KEY, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~k
    N_("messages which contain PGP key") },
  { 'l', MUTT_PAT_LIST, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~l
    N_("messages addressed to known mailing lists") },
  { 'L', MUTT_PAT_ADDRESS, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~L
    N_("messages whose From/Sender/To/CC matches EXPR") },
  { 'm', MUTT_PAT_MESSAGE, 0, EAT_MESSAGE_RANGE,
    // L10N: Pattern Completion Menu description for ~m
    N_("messages whose number is in RANGE") },
  { 'M', MUTT_PAT_MIMETYPE, MUTT_PC_FULL_MSG, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~M
    N_("messages with a Content-Type matching EXPR") },
  { 'n', MUTT_PAT_SCORE, 0, EAT_RANGE,
    // L10N: Pattern Completion Menu description for ~n
    N_("messages whose score is in RANGE") },
  { 'N', MUTT_NEW, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~N
    N_("new messages") },
  { 'O', MUTT_OLD, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~O
    N_("old messages") },
  { 'p', MUTT_PAT_PERSONAL_RECIP, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~p
    N_("messages addressed to you") },
  { 'P', MUTT_PAT_PERSONAL_FROM, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~P
    N_("messages from you") },
  { 'Q', MUTT_REPLIED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~Q
    N_("messages which have been replied to") },
  { 'r', MUTT_PAT_DATE_RECEIVED, 0, EAT_DATE,
    // L10N: Pattern Completion Menu description for ~r
    N_("messages received in DATERANGE") },
  { 'R', MUTT_READ, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~R
    N_("already read messages") },
  { 's', MUTT_PAT_SUBJECT, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~s
    N_("messages whose Subject header matches EXPR") },
  { 'S', MUTT_SUPERSEDED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~S
    N_("superseded messages") },
  { 't', MUTT_PAT_TO, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~t
    N_("messages whose To header matches EXPR") },
  { 'T', MUTT_TAG, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~T
    N_("tagged messages") },
  { 'u', MUTT_PAT_SUBSCRIBED_LIST, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~u
    N_("messages addressed to subscribed mailing lists") },
  { 'U', MUTT_UNREAD, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~U
    N_("unread messages") },
  { 'v', MUTT_PAT_COLLAPSED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~v
    N_("messages in collapsed threads") },
  { 'V', MUTT_PAT_CRYPT_VERIFIED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~V
    N_("cryptographically verified messages") },
#ifdef USE_NNTP
  { 'w', MUTT_PAT_NEWSGROUPS, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~w
    N_("newsgroups matching EXPR") },
#endif
  { 'x', MUTT_PAT_REFERENCE, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~x
    N_("messages whose References header matches EXPR") },
  { 'X', MUTT_PAT_MIMEATTACH, 0, EAT_RANGE,
    // L10N: Pattern Completion Menu description for ~X
    N_("messages with RANGE attachments") },
  { 'y', MUTT_PAT_XLABEL, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~y
    N_("messages whose X-Label header matches EXPR") },
  { 'Y', MUTT_PAT_DRIVER_TAGS, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for ~Y
    N_("messages whose tags match EXPR") },
  { 'z', MUTT_PAT_SIZE, 0, EAT_RANGE,
    // L10N: Pattern Completion Menu description for ~z
    N_("messages whose size is in RANGE") },
  { '=', MUTT_PAT_DUPLICATED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~=
    N_("duplicated messages") },
  { '$', MUTT_PAT_UNREFERENCED, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~$
    N_("unreferenced messages") },
  { '#', MUTT_PAT_BROKEN, 0, EAT_NONE,
    // L10N: Pattern Completion Menu description for ~#
    N_("broken threads") },
  { '/', MUTT_PAT_SERVERSEARCH, 0, EAT_REGEX,
    // L10N: Pattern Completion Menu description for =/
    N_("IMAP custom server-side search for STRING") },
  { 0, 0, 0, EAT_NONE, NULL, },
};
// clang-format on


/**
 * lookup_tag - Lookup a pattern modifier
 * @param tag Letter, e.g. 'b' for pattern '~b'
 * @retval ptr Pattern data
 */
const struct PatternFlags *lookup_tag(char tag)
{
  for (int i = 0; Flags[i].tag; i++)
    if (Flags[i].tag == tag)
      return &Flags[i];
  return NULL;
}

/**
 * lookup_op - Lookup the Pattern Flags for an op
 * @param op Operation, e.g. #MUTT_PAT_SENDER
 * @retval ptr PatternFlags
 */
const struct PatternFlags *lookup_op(int op)
{
  for (int i = 0; Flags[i].tag; i++)
    if (Flags[i].op == op)
      return (&Flags[i]);
  return NULL;
}
