/**
 * @file
 * Common code for command tests
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "browser/lib.h"
#include "complete/lib.h"
#include "key/lib.h"
#include "send/lib.h"
#include "external.h"

struct ConnAccount;
struct stat;

bool MonitorCurMboxChanged = false;
bool OptAutocryptGpgme = false;
bool OptDontHandlePgpKeys = false;
bool OptNeedRescore = false;
bool OptNeedResort = false;
bool OptNews = false;
bool OptNewsSend = false;
bool OptPgpCheckTrust = false;
bool OptSortSubthreads = false;

char *CurrentFolder = NULL;

struct ListHead TempAttachmentsList = STAILQ_HEAD_INITIALIZER(TempAttachmentsList);
struct ListHead UserHeader = STAILQ_HEAD_INITIALIZER(UserHeader);

const struct CompleteOps CompleteAliasOps = {};
const struct CompleteOps CompleteFileOps = {};

const struct MenuFuncOp OpCompose[] = {
  { NULL, OP_NULL },
};

const char *GitVer = "";

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
  return false;
}

bool message_is_tagged(struct Email *e)
{
  return false;
}

int mutt_body_copy(FILE *fp, struct Body **b_dst, struct Body *b_src)
{
  return 0;
}

bool mutt_can_decode(struct Body *b)
{
  return false;
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

bool mutt_edit_content_type(struct Email *e, struct Body *b, FILE *fp)
{
  return false;
}

void mutt_edit_headers(const char *editor, const char *body, struct Email *e,
                       struct Buffer *fcc)
{
}

bool mutt_prefer_as_attachment(struct Body *b)
{
  return false;
}

bool mutt_rfc3676_is_format_flowed(struct Body *b)
{
  return false;
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

int mutt_save_message_mbox(struct Mailbox *m_src, struct Email *e, enum MessageSaveOpt save_opt,
                           enum MessageTransformOpt transform_opt, struct Mailbox *m_dst)
{
  return 0;
}

int mx_ac_remove(struct Mailbox *m, bool keep_account)
{
  return 0;
}

void mx_fastclose_mailbox(struct Mailbox *m, bool keep_account)
{
}

enum MxStatus mx_mbox_close(struct Mailbox *m)
{
  return MX_STATUS_ERROR;
}

struct Mailbox *mx_mbox_find2(const char *path)
{
  return NULL;
}

bool mx_mbox_open(struct Mailbox *m, OpenMailboxFlags flags)
{
  return false;
}

int mx_msg_commit(struct Mailbox *m, struct Message *msg)
{
  return 0;
}

int mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *b,
                             enum MuttWriteHeaderMode mode, bool privacy,
                             bool hide_protected_subject, struct ConfigSubset *sub)
{
  return 0;
}

int mutt_write_mime_body(struct Body *b, FILE *fp, struct ConfigSubset *sub)
{
  return 0;
}

int mutt_write_mime_header(struct Body *b, FILE *fp, struct ConfigSubset *sub)
{
  return 0;
}

int mutt_write_one_header(FILE *fp, const char *tag, const char *value,
                          const char *pfx, int wraplen, CopyHeaderFlags chflags,
                          struct ConfigSubset *sub)
{
  return 0;
}

bool feature_enabled(const char *name)
{
  return true;
}

int mutt_monitor_add(struct Mailbox *m)
{
  return 0;
}

int mutt_monitor_remove(struct Mailbox *m)
{
  return 0;
}

bool mx_ac_add(struct Account *a, struct Mailbox *m)
{
  return false;
}

struct Account *mx_ac_find(struct Mailbox *m)
{
  return NULL;
}

struct Mailbox *mx_mbox_find(struct Account *a, const char *path)
{
  return NULL;
}

int mx_path_canon2(struct Mailbox *m, const char *folder)
{
  if (m)
    m->type = MUTT_MAILDIR;
  return 0;
}

bool print_version(FILE *fp, bool use_ansi)
{
  return false;
}

const struct MxOps *mx_get_ops(enum MailboxType type)
{
  return NULL;
}

int mx_access(const char *path, int flags)
{
  return 0;
}

int debug_level_validator(const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  return 1;
}

enum MailboxType mx_path_probe(const char *path)
{
  return MUTT_MAILDIR;
}

int sort_validator(const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  return CSR_SUCCESS;
}

const struct EnumDef UseThreadsTypeDef = {
  "use_threads_type",
  4,
  NULL,
};
