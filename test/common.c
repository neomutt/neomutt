/**
 * @file
 * Common code for file tests
 *
 * @authors
 * Copyright (C) 2020-2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "browser/lib.h"
#include "color/lib.h"
#include "complete/lib.h"
#include "send/lib.h"
#include "external.h"
#include "mx.h"

struct AttachCtx;
struct Notify;
struct PagerView;

bool StartupComplete = true;

char *HomeDir = NULL;
char *ShortHostname = "example";
bool MonitorContextChanged = false;
char *LastFolder = NULL;

bool OptResortInit = false;

#define TEST_DIR "NEOMUTT_TEST_DIR"

const struct CompleteOps CompleteMailboxOps = {};

struct ListHead AlternativeOrderList = STAILQ_HEAD_INITIALIZER(AlternativeOrderList);
struct ListHead AutoViewList = STAILQ_HEAD_INITIALIZER(AutoViewList);
struct ListHead HeaderOrderList = STAILQ_HEAD_INITIALIZER(HeaderOrderList);
struct ListHead MimeLookupList = STAILQ_HEAD_INITIALIZER(MimeLookupList);

extern const struct Module ModuleTest;
static const struct Module *Modules[] = { &ModuleTest, NULL };

const char *get_test_dir(void)
{
  return mutt_str_getenv(TEST_DIR);
}

static void init_tmp_dir(struct NeoMutt *n)
{
  char buf[PATH_MAX] = { 0 };

  snprintf(buf, sizeof(buf), "%s/tmp", mutt_str_getenv(TEST_DIR));

  struct HashElem *he = cs_get_elem(n->sub->cs, "tmp_dir");
  cs_he_initial_set(n->sub->cs, he, buf, NULL);
  cs_str_reset(n->sub->cs, "tmp_dir", NULL);
}

void test_gen_path(struct Buffer *buf, const char *fmt)
{
  buf_printf(buf, NONULL(fmt), NONULL(get_test_dir()));
}

static void init_home_dir(struct NeoMutt *n)
{
  struct passwd *pw = getpwuid(getuid());
  TEST_CHECK(pw != NULL);

  mutt_str_replace(&n->username, pw->pw_name);
  mutt_str_replace(&n->home_dir, pw->pw_dir);
}

bool test_neomutt_create(void)
{
  NeoMutt = neomutt_new();
  TEST_CHECK(NeoMutt != NULL);

  char **tmp_env = MUTT_MEM_CALLOC(2, char *);
  neomutt_init(NeoMutt, tmp_env, Modules);
  FREE(&tmp_env);

  init_tmp_dir(NeoMutt);
  init_home_dir(NeoMutt);

  return NeoMutt;
}

void test_neomutt_destroy(void)
{
  if (!NeoMutt)
    return;

  neomutt_cleanup(NeoMutt);
  neomutt_free(&NeoMutt);
}

void test_init(void)
{
  setenv("TZ", "UTC", 1); // Default to UTC

  const char *path = get_test_dir();
  bool success = false;

  TEST_CASE("Common setup");
  if (!TEST_CHECK(path != NULL))
  {
    TEST_MSG("Environment variable '%s' isn't set", TEST_DIR);
    goto done;
  }

  size_t len = strlen(path);
  if (!TEST_CHECK(path[len - 1] != '/'))
  {
    TEST_MSG("Environment variable '%s' mustn't end with a '/'", TEST_DIR);
    goto done;
  }

  struct stat st = { 0 };

  if (!TEST_CHECK(stat(path, &st) == 0))
  {
    TEST_MSG("Test dir '%s' doesn't exist", path);
    goto done;
  }

  if (!TEST_CHECK(S_ISDIR(st.st_mode) == true))
  {
    TEST_MSG("Test dir '%s' isn't a directory", path);
    goto done;
  }

  test_neomutt_create();

  if (!TEST_CHECK((setlocale(LC_ALL, "C.UTF-8") != NULL) ||
                  (setlocale(LC_ALL, "en_US.UTF-8") != NULL)))
  {
    TEST_MSG("Can't set locale to C.UTF-8 or en_US.UTF-8");
    goto done;
  }

  regex_colors_init();
  success = true;
done:
  if (!success)
  {
    TEST_MSG("See: https://github.com/neomutt/neomutt-test-files#test-files");
    exit(1);
  }
}

void test_fini(void)
{
  regex_colors_cleanup();
  config_cache_cleanup();
  test_neomutt_destroy();
  buf_pool_cleanup();
}

struct MuttWindow *add_panel_pager(struct MuttWindow *parent, bool status_on_top)
{
  return NULL;
}

int mutt_do_pager(struct PagerView *pview, struct Email *e)
{
  return 0;
}

struct HashTable *mutt_make_id_hash(struct Mailbox *m)
{
  return NULL;
}

void mx_alloc_memory(struct Mailbox *m, int req_size)
{
}

struct Mailbox *mx_path_resolve(const char *path)
{
  return NULL;
}

bool mx_mbox_ac_link(struct Mailbox *m)
{
  return false;
}

void mutt_set_header_color(struct Mailbox *m, struct Email *e)
{
}

struct Message *mx_msg_open_new(struct Mailbox *m, const struct Email *e, MsgOpenFlags flags)
{
  return NULL;
}

int mutt_copy_message(FILE *fp_out, struct Email *e, struct Message *msg,
                      CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen)
{
  return 0;
}

/**
 * log_disp_null - Discard log lines - Implements ::log_dispatcher_t - @ingroup logging_api
 */
int log_disp_null(time_t stamp, const char *file, int line, const char *function,
                  enum LogLevel level, const char *format, ...)
{
  return 0;
}

typedef uint8_t MuttThreadFlags;
int mutt_traverse_thread(struct Email *e, MuttThreadFlags flag)
{
  return 0;
}

enum UseThreads mutt_thread_style(void)
{
  return UT_THREADS;
}

#ifdef USE_DEBUG_BACKTRACE
void show_backtrace(void)
{
}
#endif

const char *mutt_make_version(void)
{
  return "VERSION";
}

void attach_bounce_message(struct Mailbox *m, FILE *fp, struct AttachCtx *actx,
                           struct Body *b)
{
}

int ea_add_tagged(struct EmailArray *ea, struct MailboxView *mv, struct Email *e, bool use_tagged)
{
  return 0;
}

const char *get_use_threads_str(enum UseThreads value)
{
  return NULL;
}

void index_bounce_message(struct Mailbox *m, struct EmailArray *ea)
{
}

int mutt_aside_thread(struct Email *e, bool forwards, bool subthreads)
{
  return 0;
}

void mutt_attach_forward(FILE *fp, struct Email *e, struct AttachCtx *actx,
                         struct Body *b, SendFlags flags)
{
}

void mutt_attach_mail_sender(struct AttachCtx *actx, struct Body *b)
{
}

void mutt_attach_reply(FILE *fp, struct Mailbox *m, struct Email *e,
                       struct AttachCtx *actx, struct Body *b, SendFlags flags)
{
}

void mutt_attach_resend(FILE *fp, struct Mailbox *m, struct AttachCtx *actx, struct Body *b)
{
}

bool mutt_check_traditional_pgp(struct Mailbox *m, struct EmailArray *ea)
{
  return true;
}

void mutt_clear_threads(struct ThreadsContext *tctx)
{
}

void mutt_display_address(struct Envelope *env)
{
}

void mutt_draw_tree(struct ThreadsContext *tctx)
{
}

void mutt_emails_set_flag(struct Mailbox *m, struct EmailArray *ea,
                          enum MessageType flag, bool bf)
{
}

int mutt_ev_message(struct Mailbox *m, struct EmailArray *ea, enum EvMessage action)
{
  return 0;
}

void mutt_label_hash_add(struct Mailbox *m, struct Email *e)
{
}

void mutt_label_hash_remove(struct Mailbox *m, struct Email *e)
{
}

int mutt_label_message(struct MailboxView *mv, struct EmailArray *ea)
{
  return 0;
}

bool mutt_limit_current_thread(struct MailboxView *mv, struct Email *e)
{
  return true;
}

bool mutt_link_threads(struct Email *parent, struct EmailArray *children, struct Mailbox *m)
{
  return true;
}

int mutt_mailbox_check(struct Mailbox *m_cur, CheckStatsFlags flags)
{
  return 0;
}

bool mutt_mailbox_list(void)
{
  return true;
}

struct Mailbox *mutt_mailbox_next_unread(struct Mailbox *m_cur, struct Buffer *s)
{
  return NULL;
}

bool mutt_mailbox_notify(struct Mailbox *m_cur)
{
  return true;
}

int mutt_messages_in_thread(struct Mailbox *m, struct Email *e, enum MessageInThread mit)
{
  return 0;
}

int mutt_parent_message(struct Email *e, bool find_root)
{
  return 0;
}

void mutt_pipe_message(struct Mailbox *m, struct EmailArray *ea)
{
}

void mutt_print_message(struct Mailbox *m, struct EmailArray *ea)
{
}

int mutt_save_message(struct Mailbox *m, struct EmailArray *ea,
                      enum MessageSaveOpt save_opt, enum MessageTransformOpt transform_opt)
{
  return 0;
}

bool mutt_select_sort(bool reverse)
{
  return true;
}

off_t mutt_set_vnum(struct Mailbox *m)
{
  return 0;
}

void mutt_sort_threads(struct ThreadsContext *tctx, bool init)
{
}

bool mutt_thread_can_collapse(struct Email *e)
{
  return true;
}

void mutt_thread_collapse(struct ThreadsContext *tctx, bool collapse)
{
}

void mutt_thread_collapse_collapsed(struct ThreadsContext *tctx)
{
}

int mutt_thread_set_flag(struct Mailbox *m, struct Email *e,
                         enum MessageType flag, bool bf, bool subthread)
{
  return 0;
}

void mview_free(struct MailboxView **ptr)
{
}

bool mview_has_limit(const struct MailboxView *mv)
{
  return true;
}

struct MailboxView *mview_new(struct Mailbox *m, struct Notify *parent)
{
  return NULL;
}

int mw_change_flag(struct Mailbox *m, struct EmailArray *ea, bool bf)
{
  return 0;
}

enum MxStatus mx_mbox_check(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

enum MxStatus mx_mbox_sync(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

int mx_path_canon(struct Buffer *path, const char *folder, enum MailboxType *type)
{
  return 0;
}

int mx_save_hcache(struct Mailbox *m, struct Email *e)
{
  return 0;
}

int mx_tags_commit(struct Mailbox *m, struct Email *e, const char *tags)
{
  return 0;
}

int mx_tags_edit(struct Mailbox *m, const char *tags, struct Buffer *buf)
{
  return 0;
}

bool mx_tags_is_supported(struct Mailbox *m)
{
  return true;
}

int mx_toggle_write(struct Mailbox *m)
{
  return 0;
}

enum MailboxType mx_type(struct Mailbox *m)
{
  return MUTT_MAILDIR;
}

int mutt_comp_valid_command(const char *cmd)
{
  return 1;
}
