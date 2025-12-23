/**
 * @file
 * Common code for file tests
 *
 * @authors
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "complete/lib.h"
#include "send/lib.h"
#include "copy.h"
#include "external.h"
#include "mailcap.h"
#include "mutt_thread.h"
#include "mx.h"
#include "protos.h"

struct AddressList;
struct AttachCtx;
struct ConnAccount;
struct Email;
struct Envelope;
struct MuttWindow;
struct PagerView;

bool StartupComplete = true;

char *HomeDir = NULL;
char *ShortHostname = "example";
bool MonitorContextChanged = false;
char *LastFolder = NULL;

const struct CompleteOps CompleteAliasOps = { 0 };
char *CurrentFolder = NULL;
char *GitVer = NULL;

bool MonitorCurMboxChanged = false;
bool OptAutocryptGpgme = false;
bool OptDontHandlePgpKeys = false;
bool OptNeedRescore = false;
bool OptNeedResort = false;
bool OptNews = false;
bool OptNewsSend = false;
bool OptPgpCheckTrust = false;
bool OptResortInit = false;
bool OptSortSubthreads = false;

struct ListHead TempAttachmentsList = STAILQ_HEAD_INITIALIZER(TempAttachmentsList);
struct ListHead UserHeader = STAILQ_HEAD_INITIALIZER(UserHeader);

const struct EnumDef UseThreadsTypeDef = { "use_threads_type", 4, NULL };

#define TEST_DIR "NEOMUTT_TEST_DIR"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "assumed_charset", DT_SLIST|D_SLIST_SEP_COLON|D_SLIST_ALLOW_EMPTY, 0, 0, NULL, },
  { "charset", DT_STRING|D_NOT_EMPTY|D_CHARSET_SINGLE, IP "utf-8", 0, NULL, },
  { "config_charset", DT_STRING, 0, 0, NULL, },
  { "debug_level", DT_NUMBER, 0, 0, NULL },
  { "folder", DT_STRING|D_STRING_MAILBOX, IP "~/Mail", 0, NULL, },
  { "maildir_field_delimiter", DT_STRING, IP ":", 0, NULL, },
  { "record", DT_STRING|D_STRING_MAILBOX, IP "~/sent", 0, NULL, },
  { "sleep_time", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 0, 0, NULL, },
  { "tmp_dir", DT_PATH|D_PATH_DIR|D_NOT_EMPTY, IP TMPDIR, 0, NULL, },
  { NULL },
  // clang-format on
};

struct ListHead AlternativeOrderList = STAILQ_HEAD_INITIALIZER(AlternativeOrderList);
struct ListHead AutoViewList = STAILQ_HEAD_INITIALIZER(AutoViewList);
struct ListHead HeaderOrderList = STAILQ_HEAD_INITIALIZER(HeaderOrderList);
struct ListHead MimeLookupList = STAILQ_HEAD_INITIALIZER(MimeLookupList);

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

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

  NeoMutt->username = mutt_str_dup(pw->pw_name);
  NeoMutt->home_dir = mutt_str_dup(pw->pw_dir);
}

bool test_neomutt_create(void)
{
  struct ConfigSet *cs = cs_new(50);
  CONFIG_INIT_TYPE(cs, Address);
  CONFIG_INIT_TYPE(cs, Bool);
  CONFIG_INIT_TYPE(cs, Enum);
  CONFIG_INIT_TYPE(cs, Expando);
  CONFIG_INIT_TYPE(cs, Long);
  CONFIG_INIT_TYPE(cs, Mbtable);
  CONFIG_INIT_TYPE(cs, MyVar);
  CONFIG_INIT_TYPE(cs, Number);
  CONFIG_INIT_TYPE(cs, Path);
  CONFIG_INIT_TYPE(cs, Quad);
  CONFIG_INIT_TYPE(cs, Regex);
  CONFIG_INIT_TYPE(cs, Slist);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, String);

  NeoMutt = neomutt_new(cs);
  TEST_CHECK(NeoMutt != NULL);
  NeoMutt->env = MUTT_MEM_CALLOC(2, char *);

  TEST_CHECK(cs_register_variables(cs, Vars));

  init_tmp_dir(NeoMutt);
  init_home_dir(NeoMutt);

  return NeoMutt;
}

void test_neomutt_destroy(void)
{
  if (!NeoMutt)
    return;

  struct ConfigSet *cs = NeoMutt->sub->cs;
  neomutt_free(&NeoMutt);
  cs_free(&cs);
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

  if (!TEST_CHECK((setlocale(LC_ALL, "C.UTF-8") != NULL) ||
                  (setlocale(LC_ALL, "en_US.UTF-8") != NULL)))
  {
    TEST_MSG("Can't set locale to C.UTF-8 or en_US.UTF-8");
    goto done;
  }

  test_neomutt_create();
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

bool check_for_mailing_list(struct AddressList *al, const char *pfx, char *buf, int buflen)
{
  return false;
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

bool check_for_mailing_list_addr(struct AddressList *al, char *buf, int buflen)
{
  return true;
}

int debug_level_validator(const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  return 0;
}

int ea_add_tagged(struct EmailArray *ea, struct MailboxView *mv, struct Email *e, bool use_tagged)
{
  return 0;
}

bool feature_enabled(const char *name)
{
  return true;
}

bool first_mailing_list(char *buf, size_t buflen, struct AddressList *al)
{
  return true;
}

const char *get_use_threads_str(enum UseThreads value)
{
  return NULL;
}

void index_bounce_message(struct Mailbox *m, struct EmailArray *ea)
{
}

void mailbox_restore_timestamp(const char *path, struct stat *st)
{
}

void mailcap_entry_free(struct MailcapEntry **ptr)
{
}

struct MailcapEntry *mailcap_entry_new(void)
{
  return NULL;
}

int mailcap_expand_command(struct Body *b, const char *filename,
                           const char *type, struct Buffer *command)
{
  return 0;
}

void mailcap_expand_filename(const char *nametemplate, const char *oldfile,
                             struct Buffer *newfile)
{
}

bool mailcap_lookup(struct Body *b, char *type, size_t typelen,
                    struct MailcapEntry *entry, enum MailcapLookup opt)
{
  return true;
}

bool message_is_tagged(struct Email *e)
{
  return true;
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

int mutt_body_copy(FILE *fp, struct Body **b_dst, struct Body *b_src)
{
  return 0;
}

bool mutt_can_decode(struct Body *b)
{
  return true;
}

bool mutt_check_traditional_pgp(struct Mailbox *m, struct EmailArray *ea)
{
  return true;
}

void mutt_clear_threads(struct ThreadsContext *tctx)
{
}

struct Connection *mutt_conn_find(const struct ConnAccount *cac)
{
  return NULL;
}

struct Connection *mutt_conn_new(const struct ConnAccount *cac)
{
  return NULL;
}

int mutt_copy_hdr(FILE *fp_in, FILE *fp_out, LOFF_T off_start, LOFF_T off_end,
                  CopyHeaderFlags chflags, const char *prefix, int wraplen)
{
  return 0;
}

int mutt_copy_message_fp(FILE *fp_out, FILE *fp_in, struct Email *e,
                         CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen)
{
  return 0;
}

void mutt_decode_attachment(const struct Body *b, struct State *state)
{
}

void mutt_decode_base64(struct State *state, size_t len, bool istext, iconv_t cd)
{
}

void mutt_display_address(struct Envelope *env)
{
}

void mutt_draw_tree(struct ThreadsContext *tctx)
{
}

bool mutt_edit_content_type(struct Email *e, struct Body *b, FILE *fp)
{
  return true;
}

void mutt_edit_headers(const char *editor, const char *body, struct Email *e,
                       struct Buffer *fcc)
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

int mutt_monitor_add(struct Mailbox *m)
{
  return 0;
}

int mutt_monitor_remove(struct Mailbox *m)
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

bool mutt_prefer_as_attachment(struct Body *b)
{
  return true;
}

void mutt_print_message(struct Mailbox *m, struct EmailArray *ea)
{
}

bool mutt_rfc3676_is_format_flowed(struct Body *b)
{
  return true;
}

void mutt_rfc3676_space_stuff(struct Email *e)
{
}

void mutt_rfc3676_space_stuff_attachment(struct Body *b, const char *filename)
{
}

void mutt_rfc3676_space_unstuff(struct Email *e)
{
}

void mutt_rfc3676_space_unstuff_attachment(struct Body *b, const char *filename)
{
}

int mutt_save_message(struct Mailbox *m, struct EmailArray *ea,
                      enum MessageSaveOpt save_opt, enum MessageTransformOpt transform_opt)
{
  return 0;
}

int mutt_save_message_mbox(struct Mailbox *m_src, struct Email *e, enum MessageSaveOpt save_opt,
                           enum MessageTransformOpt transform_opt, struct Mailbox *m_dst)
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

int mx_access(const char *path, int flags)
{
  return 0;
}

bool mx_ac_add(struct Account *a, struct Mailbox *m)
{
  return true;
}

struct Account *mx_ac_find(struct Mailbox *m)
{
  return NULL;
}

int mx_ac_remove(struct Mailbox *m, bool keep_account)
{
  return 0;
}

void mx_fastclose_mailbox(struct Mailbox *m, bool keep_account)
{
}

const struct MxOps *mx_get_ops(enum MailboxType type)
{
  return NULL;
}

enum MxStatus mx_mbox_check(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

enum MxStatus mx_mbox_close(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

struct Mailbox *mx_mbox_find(struct Account *a, const char *path)
{
  return NULL;
}

struct Mailbox *mx_mbox_find2(const char *path)
{
  return NULL;
}

bool mx_mbox_open(struct Mailbox *m, OpenMailboxFlags flags)
{
  return true;
}

enum MxStatus mx_mbox_sync(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

int mx_msg_commit(struct Mailbox *m, struct Message *msg)
{
  return 0;
}

int mx_path_canon(struct Buffer *path, const char *folder, enum MailboxType *type)
{
  return 0;
}

int mx_path_canon2(struct Mailbox *m, const char *folder)
{
  return 0;
}

enum MailboxType mx_path_probe(const char *path)
{
  return MUTT_MAILDIR;
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

bool print_version(FILE *fp, bool use_ansi)
{
  return true;
}

int sort_validator(const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  return 0;
}
