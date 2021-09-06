/**
 * @file
 * Compose Private Data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMPOSE_PRIVATE_H
#define MUTT_COMPOSE_PRIVATE_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>

extern int HeaderPadding[];
extern int MaxHeaderWidth;
extern const char *const Prompts[];

typedef uint8_t NotifyCompose;         ///< Flags, e.g. #NT_COMPOSE_ATTACH
#define NT_COMPOSE_NO_FLAGS        0   ///< No flags are set
#define NT_COMPOSE_ATTACH    (1 << 0)  ///< Attachments have changed
#define NT_COMPOSE_ENVELOPE  (1 << 1)  ///< Envelope has changed

/**
 * enum HeaderField - Ordered list of headers for the compose screen
 *
 * The position of various fields on the compose screen.
 */
enum HeaderField
{
  HDR_FROM,    ///< "From:" field
  HDR_TO,      ///< "To:" field
  HDR_CC,      ///< "Cc:" field
  HDR_BCC,     ///< "Bcc:" field
  HDR_SUBJECT, ///< "Subject:" field
  HDR_REPLYTO, ///< "Reply-To:" field
  HDR_FCC,     ///< "Fcc:" (save folder) field
#ifdef MIXMASTER
  HDR_MIX, ///< "Mix:" field (Mixmaster chain)
#endif
  HDR_CRYPT,     ///< "Security:" field (encryption/signing info)
  HDR_CRYPTINFO, ///< "Sign as:" field (encryption/signing info)
#ifdef USE_AUTOCRYPT
  HDR_AUTOCRYPT, ///< "Autocrypt:" and "Recommendation:" fields
#endif
#ifdef USE_NNTP
  HDR_NEWSGROUPS, ///< "Newsgroups:" field
  HDR_FOLLOWUPTO, ///< "Followup-To:" field
  HDR_XCOMMENTTO, ///< "X-Comment-To:" field
#endif
  HDR_CUSTOM_HEADERS, ///< "Headers:" field
  HDR_ATTACH_TITLE,   ///< The "-- Attachments" line
};

struct AttachCtx;
struct Buffer;
struct ComposeAttachData;
struct ComposeSharedData;
struct ConfigSubset;
struct Menu;
struct MuttWindow;

struct MuttWindow *compose_env_new(struct MuttWindow *parent, struct ComposeSharedData *shared, struct Buffer *fcc);
struct MuttWindow *attach_new(struct MuttWindow *parent, struct ComposeSharedData *shared);
unsigned long cum_attachs_size(struct ConfigSubset *sub, struct ComposeAttachData *adata);
int num_attachments(struct ComposeAttachData *adata);
void update_menu(struct AttachCtx *actx, struct Menu *menu, bool init);

#endif /* MUTT_COMPOSE_PRIVATE_H */
