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
 * @page lib_pager Pager
 *
 * Display contents of an email or help
 *
 * | File                 | Description                 |
 * | :------------------- | :-------------------------- |
 * | pager/config.c       | @subpage pager_config       |
 * | pager/display.c      | @subpage pager_display      |
 * | pager/dlg_pager.c    | @subpage pager_dlg_pager    |
 * | pager/do_pager.c     | @subpage pager_dopager      |
 * | pager/functions.c    | @subpage pager_functions    |
 * | pager/message.c      | @subpage pager_message      |
 * | pager/pager.c        | @subpage pager_pager        |
 * | pager/pbar.c         | @subpage pager_pbar         |
 * | pager/ppanel.c       | @subpage pager_ppanel       |
 * | pager/private_data.c | @subpage pager_private_data |
 */

#ifndef MUTT_PAGER_LIB_H
#define MUTT_PAGER_LIB_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"

struct Email;
struct IndexSharedData;
struct ConfigSubset;
struct Mailbox;
struct MuttWindow;
struct PagerPrivateData;

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
#define MUTT_PAGER_BOTTOM     (1 << 11)   ///< Start at the bottom
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
//    this path always results in email, body, mailboxview set
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
 * enum PagerLoopMode - What the Pager Event Loop should do next
 */
enum PagerLoopMode
{
  PAGER_LOOP_CONTINUE = -7,  ///< Stay in the Pager Event Loop
  PAGER_LOOP_QUIT     = -6,  ///< Quit the Pager
  PAGER_LOOP_RELOAD   = -5,  ///< Reload the Pager from scratch
};

/**
 * struct PagerData - Data to be displayed by PagerView
 */
struct PagerData
{
  struct Body      *body;   ///< Current attachment
  FILE             *fp;     ///< Source stream
  struct AttachCtx *actx;   ///< Attachment information
  const char       *fname;  ///< Name of the file to read
};

/**
 * struct PagerView - Paged view into some data
 */
struct PagerView
{
  struct PagerData *pdata; ///< Data that pager displays. NOTNULL
  enum PagerMode mode;     ///< Pager mode
  PagerFlags flags;        ///< Additional settings to tweak pager's function
  const char *banner;      ///< Title to display in status bar

  struct MuttWindow *win_index; ///< Index Window
  struct MuttWindow *win_pbar;  ///< Pager Bar Window
  struct MuttWindow *win_pager; ///< Pager Window
};

// Observers of #NT_PAGER will be passed a #PagerPrivateData.
typedef uint8_t NotifyPager;         ///< Flags, e.g. #NT_PAGER_DELETE
#define NT_PAGER_NO_FLAGS        0   ///< No flags are set
#define NT_PAGER_DELETE    (1 << 0)  ///< Pager Private Data is about to be freed
#define NT_PAGER_VIEW      (1 << 1)  ///< Pager View has changed

typedef uint8_t PagerRedrawFlags;       ///< Flags, e.g. #PAGER_REDRAW_PAGER
#define PAGER_REDRAW_NO_FLAGS        0  ///< No flags are set
#define PAGER_REDRAW_PAGER     (1 << 1) ///< Redraw the pager
#define PAGER_REDRAW_FLOW      (1 << 2) ///< Reflow the pager

extern int BrailleRow;
extern int BrailleCol;

int mutt_pager(struct PagerView *pview);
int mutt_do_pager(struct PagerView *pview, struct Email *e);
void buf_strip_formatting(struct Buffer *dest, const char *src, bool strip_markers);
struct MuttWindow *ppanel_new(bool status_on_top, struct IndexSharedData *shared);
struct MuttWindow *pager_window_new(struct IndexSharedData *shared, struct PagerPrivateData *priv);
int mutt_display_message(struct MuttWindow *win_index, struct IndexSharedData *shared);
int external_pager(struct Mailbox *m, struct Email *e, const char *command);
void pager_queue_redraw(struct PagerPrivateData *priv, PagerRedrawFlags redraw);
bool mutt_is_quote_line(char *buf, regmatch_t *pmatch);
const char *pager_get_pager(struct ConfigSubset *sub);

void mutt_clear_pager_position(void);

struct TextSyntax;
struct Line;
void dump_text_syntax(struct TextSyntax *ts, int num);
void dump_line(int i, struct Line *line);
void dump_pager(struct PagerPrivateData *priv);

#endif /* MUTT_PAGER_LIB_H */
