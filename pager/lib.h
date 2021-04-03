/**
 * @file
 * GUI display a file/email/help in a viewport with paging
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
 * @page lib_pager PAGER: GUI display contents of an email or help
 *
 * GUI Display contents of an email or help
 *
 * | File               | Description              |
 * | :----------------- | :----------------------- |
 * | pager/config.c     | @subpage pager_config    |
 * | pager/pager.c      | @subpage pager_pager     |
 */

#ifndef MUTT_PAGER_LIB_H
#define MUTT_PAGER_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct Buffer;

typedef uint16_t PagerFlags;              ///< Flags for mutt_pager(), e.g. #MUTT_SHOWFLAT
#define MUTT_PAGER_NO_FLAGS         0     ///< No flags are set
#define MUTT_SHOWFLAT         (1 << 0)    ///< Show characters (used for displaying help)
#define MUTT_SHOWCOLOR        (1 << 1)    ///< Show characters in color otherwise don't show characters
#define MUTT_HIDE             (1 << 2)    ///< Don't show quoted text
#define MUTT_SEARCH           (1 << 3)    ///< Resolve search patterns
#define MUTT_TYPES            (1 << 4)    ///< Compute line's type
#define MUTT_SHOW             (MUTT_SHOWCOLOR | MUTT_SHOWFLAT)

/* exported flags for mutt_(do_)?pager */
#define MUTT_PAGER_NSKIP      (1 << 5)    ///< Preserve whitespace with smartwrap
#define MUTT_PAGER_MARKER     (1 << 6)    ///< Use markers if option is set
#define MUTT_PAGER_RETWINCH   (1 << 7)    ///< Need reformatting on SIGWINCH
#define MUTT_PAGER_ATTACHMENT (1 << 8)    ///< Attachments may exist
#define MUTT_PAGER_NOWRAP     (1 << 9)    ///< Format for term width, ignore $wrap
#define MUTT_PAGER_LOGS       (1 << 10)   ///< Logview mode
#define MUTT_PAGER_MESSAGE    (MUTT_SHOWCOLOR | MUTT_PAGER_MARKER)

#define MUTT_DISPLAYFLAGS (MUTT_SHOW | MUTT_PAGER_NSKIP | MUTT_PAGER_MARKER | MUTT_PAGER_LOGS)

// Pager mode.
// There are 10 code paths that lead to mutt_pager() invocation:
//
// 1. mutt_index_menu -> mutt_display_message -> mutt_pager
//
//    This path always results in mailbox and email set,
//    the rest is unset - Body, fp.
//    This invocation can be identified by IsEmail macro.
//    The intent is to display an email message
//
// 2. mutt_view_attachment -> mutt_do_pager -> mutt_pager
//
//    this path always results in email, body, ctx set
//    this invocation can be identified by one of the two macros
//    - IsAttach (viewing a regular attachment)
//    - IsMsgAttach (viewing nested email)
//
//    IsMsgAttach has extra->body->email set
//    extra->email != extra->body->email
//    the former is the message that contains the latter message as attachment
//
//    NB. extra->email->body->email seems to be always NULL
//
// 3. The following 8 invocations are similar, because they all call
//    mutt_do_page with info = NULL
//
//    And so it results in mailbox, body, fp set to NULL.
//    The intent is to show user some text that is not
//    directly related to viewing emails,
//    e.g. help, log messages,gpg key selection etc.
//
//    No macro identifies these invocations
//
//    mutt_index_menu       -> mutt_do_pager -> mutt_pager
//    mutt_help             -> mutt_do_pager -> mutt_pager
//    icmd_bind             -> mutt_do_pager -> mutt_pager
//    icmd_set              -> mutt_do_pager -> mutt_pager
//    icmd_version          -> mutt_do_pager -> mutt_pager
//    dlg_select_pgp_key    -> mutt_do_pager -> mutt_pager
//    verify_key            -> mutt_do_pager -> mutt_pager
//    mutt_invoke_sendmail  -> mutt_do_pager -> mutt_pager
//
//
// - IsAttach(pager) (pager && (pager)->body)
// - IsMsgAttach(pager)
//   (pager && (pager)->fp && (pager)->body && (pager)->body->email)
// - IsEmail(pager) (pager && (pager)->email && !(pager)->body)
// See nice infographic here:
// https://gist.github.com/flatcap/044ecbd2498c65ea9a85099ef317509a

/**
 * enum PagerMode - Determine the behaviour of the Pager
 */
enum PagerMode
{
  PAGER_MODE_UNKNOWN = 0, ///< A default and invalid mode, should never be used

  PAGER_MODE_EMAIL,       ///< Pager is invoked via 1st path. The mime part is selected automatically
  PAGER_MODE_ATTACH,      ///< Pager is invoked via 2nd path. A user-selected attachment (mime part or a nested email) will be shown
  PAGER_MODE_ATTACH_E,    ///< A special case of PAGER_MODE_ATTACH - attachment is a full-blown email message
  PAGER_MODE_HELP,        ///< Pager is invoked via 3rd path to show help.
  PAGER_MODE_OTHER,       ///< Pager is invoked via 3rd path. Non-email content is likely to be shown

  PAGER_MODE_MAX,         ///< Another invalid mode, should never be used
};

/**
 * struct PagerData - Data to be displayed by PagerView
 */
struct PagerData
{
  struct Context   *ctx;    ///< Current Mailbox context
  struct Email     *email;  ///< Current message
  struct Body      *body;   ///< Current attachment
  FILE             *fp;     ///< Source stream
  struct AttachCtx *actx;   ///< Attachment information
  const char       *fname;  ///< Name of the file to read
};

/**
 * struct PagerView - paged view into some data
 */
struct PagerView
{
  struct PagerData *pdata;   ///< Data that pager displays. NOTNULL
  enum PagerMode    mode;    ///< Pager mode
  PagerFlags        flags;   ///< Additional settings to tweak pager's function
  const char       *banner;  ///< Title to display in status bar

  struct MuttWindow *win_ibar;
  struct MuttWindow *win_index;
  struct MuttWindow *win_pbar;
  struct MuttWindow *win_pager;
};

int mutt_pager(struct PagerView *pview);
void mutt_buffer_strip_formatting(struct Buffer *dest, const char *src, bool strip_markers);

#endif /* MUTT_PAGER_LIB_H */
