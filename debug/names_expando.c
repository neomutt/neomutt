/**
 * @file
 * Debug names for Expandos
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <assert.h>
#include "email/lib.h"
#include "core/lib.h"
#include "alias/gui.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "attach/lib.h"
#include "browser/lib.h"
#include "compmbox/lib.h"
#include "expando/lib.h"
#include "history/lib.h"
#include "index/lib.h"
#include "mixmaster/lib.h"
#include "compose/shared_data.h"
#include "ncrypt/pgp.h"
#include "ncrypt/pgplib.h"
#include "ncrypt/private.h"
#include "ncrypt/smime.h"
#include "pattern/private.h"
#include "sidebar/private.h"
#include "status.h"
#ifdef CRYPT_BACKEND_GPGME
#include "ncrypt/crypt_gpgme.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/private.h"
#endif

#define DEBUG_NAME(NAME)                                                       \
  case NAME:                                                                   \
    return #NAME

#define DEBUG_DEFAULT                                                          \
  default:                                                                     \
    return "UNKNOWN"

const char *name_expando_node_type(enum ExpandoNodeType type)
{
  switch (type)
  {
    DEBUG_NAME(ENT_EMPTY);
    DEBUG_NAME(ENT_TEXT);
    DEBUG_NAME(ENT_EXPANDO);
    DEBUG_NAME(ENT_PADDING);
    DEBUG_NAME(ENT_CONDITION);
    DEBUG_NAME(ENT_CONDBOOL);
    DEBUG_NAME(ENT_CONDDATE);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_pad_type(enum ExpandoPadType type)
{
  switch (type)
  {
    DEBUG_NAME(EPT_FILL_EOL);
    DEBUG_NAME(EPT_HARD_FILL);
    DEBUG_NAME(EPT_SOFT_FILL);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_domain(enum ExpandoDomain did)
{
  switch (did)
  {
    DEBUG_NAME(ED_ALIAS);
    DEBUG_NAME(ED_ATTACH);
    DEBUG_NAME(ED_AUTOCRYPT);
    DEBUG_NAME(ED_BODY);
    DEBUG_NAME(ED_COMPOSE);
    DEBUG_NAME(ED_COMPRESS);
    DEBUG_NAME(ED_EMAIL);
    DEBUG_NAME(ED_ENVELOPE);
    DEBUG_NAME(ED_FOLDER);
    DEBUG_NAME(ED_GLOBAL);
    DEBUG_NAME(ED_HISTORY);
    DEBUG_NAME(ED_INDEX);
    DEBUG_NAME(ED_MAILBOX);
    DEBUG_NAME(ED_MENU);
    DEBUG_NAME(ED_MIXMASTER);
    DEBUG_NAME(ED_NNTP);
    DEBUG_NAME(ED_PATTERN);
    DEBUG_NAME(ED_PGP);
    DEBUG_NAME(ED_PGP_CMD);
    DEBUG_NAME(ED_PGP_KEY);
    DEBUG_NAME(ED_SIDEBAR);
    DEBUG_NAME(ED_SMIME_CMD);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_alias(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_ALI_ADDRESS);
    DEBUG_NAME(ED_ALI_COMMENT);
    DEBUG_NAME(ED_ALI_FLAGS);
    DEBUG_NAME(ED_ALI_NAME);
    DEBUG_NAME(ED_ALI_NUMBER);
    DEBUG_NAME(ED_ALI_TAGGED);
    DEBUG_NAME(ED_ALI_TAGS);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_attach(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_ATT_CHARSET);
    DEBUG_NAME(ED_ATT_NUMBER);
    DEBUG_NAME(ED_ATT_TREE);
    DEBUG_DEFAULT;
  }
}

#ifdef USE_AUTOCRYPT
const char *name_expando_uid_autocrypt(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_AUT_ENABLED);
    DEBUG_NAME(ED_AUT_KEYID);
    DEBUG_NAME(ED_AUT_ADDRESS);
    DEBUG_NAME(ED_AUT_NUMBER);
    DEBUG_NAME(ED_AUT_PREFER_ENCRYPT);
    DEBUG_DEFAULT;
  }
}
#endif

const char *name_expando_uid_body(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_BOD_ATTACH_COUNT);
    DEBUG_NAME(ED_BOD_ATTACH_QUALIFIES);
    DEBUG_NAME(ED_BOD_CHARSET_CONVERT);
    DEBUG_NAME(ED_BOD_DELETED);
    DEBUG_NAME(ED_BOD_DESCRIPTION);
    DEBUG_NAME(ED_BOD_DISPOSITION);
    DEBUG_NAME(ED_BOD_FILE);
    DEBUG_NAME(ED_BOD_FILE_DISPOSITION);
    DEBUG_NAME(ED_BOD_FILE_SIZE);
    DEBUG_NAME(ED_BOD_MIME_ENCODING);
    DEBUG_NAME(ED_BOD_MIME_MAJOR);
    DEBUG_NAME(ED_BOD_MIME_MINOR);
    DEBUG_NAME(ED_BOD_TAGGED);
    DEBUG_NAME(ED_BOD_UNLINK);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_compose(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_COM_ATTACH_COUNT);
    DEBUG_NAME(ED_COM_ATTACH_SIZE);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_compress(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_CMP_FROM);
    DEBUG_NAME(ED_CMP_TO);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_email(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_EMA_ATTACHMENT_COUNT);
    DEBUG_NAME(ED_EMA_BODY_CHARACTERS);
    DEBUG_NAME(ED_EMA_COMBINED_FLAGS);
    DEBUG_NAME(ED_EMA_CRYPTO_FLAGS);
    DEBUG_NAME(ED_EMA_DATE_FORMAT);
    DEBUG_NAME(ED_EMA_DATE_FORMAT_LOCAL);
    DEBUG_NAME(ED_EMA_FROM_LIST);
    DEBUG_NAME(ED_EMA_INDEX_HOOK);
    DEBUG_NAME(ED_EMA_LINES);
    DEBUG_NAME(ED_EMA_LIST_OR_SAVE_FOLDER);
    DEBUG_NAME(ED_EMA_MESSAGE_FLAGS);
    DEBUG_NAME(ED_EMA_NUMBER);
    DEBUG_NAME(ED_EMA_SCORE);
    DEBUG_NAME(ED_EMA_SIZE);
    DEBUG_NAME(ED_EMA_STATUS_FLAGS);
    DEBUG_NAME(ED_EMA_STRF);
    DEBUG_NAME(ED_EMA_STRF_LOCAL);
    DEBUG_NAME(ED_EMA_STRF_RECV_LOCAL);
    DEBUG_NAME(ED_EMA_TAGS);
    DEBUG_NAME(ED_EMA_TAGS_TRANSFORMED);
    DEBUG_NAME(ED_EMA_THREAD_COUNT);
    DEBUG_NAME(ED_EMA_THREAD_HIDDEN_COUNT);
    DEBUG_NAME(ED_EMA_THREAD_NUMBER);
    DEBUG_NAME(ED_EMA_THREAD_TAGS);
    DEBUG_NAME(ED_EMA_TO_CHARS);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_envelope(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_ENV_CC_ALL);
    DEBUG_NAME(ED_ENV_FIRST_NAME);
    DEBUG_NAME(ED_ENV_FROM);
    DEBUG_NAME(ED_ENV_FROM_FULL);
    DEBUG_NAME(ED_ENV_INITIALS);
    DEBUG_NAME(ED_ENV_LIST_ADDRESS);
    DEBUG_NAME(ED_ENV_LIST_EMPTY);
    DEBUG_NAME(ED_ENV_MESSAGE_ID);
    DEBUG_NAME(ED_ENV_NAME);
    DEBUG_NAME(ED_ENV_NEWSGROUP);
    DEBUG_NAME(ED_ENV_ORGANIZATION);
    DEBUG_NAME(ED_ENV_REAL_NAME);
    DEBUG_NAME(ED_ENV_REPLY_TO);
    DEBUG_NAME(ED_ENV_SENDER);
    DEBUG_NAME(ED_ENV_SENDER_PLAIN);
    DEBUG_NAME(ED_ENV_SPAM);
    DEBUG_NAME(ED_ENV_SUBJECT);
    DEBUG_NAME(ED_ENV_THREAD_TREE);
    DEBUG_NAME(ED_ENV_THREAD_X_LABEL);
    DEBUG_NAME(ED_ENV_TO);
    DEBUG_NAME(ED_ENV_TO_ALL);
    DEBUG_NAME(ED_ENV_USERNAME);
    DEBUG_NAME(ED_ENV_USER_NAME);
    DEBUG_NAME(ED_ENV_X_COMMENT_TO);
    DEBUG_NAME(ED_ENV_X_LABEL);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_folder(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_FOL_DATE);
    DEBUG_NAME(ED_FOL_DATE_FORMAT);
    DEBUG_NAME(ED_FOL_DESCRIPTION);
    DEBUG_NAME(ED_FOL_FILENAME);
    DEBUG_NAME(ED_FOL_FILE_GROUP);
    DEBUG_NAME(ED_FOL_FILE_MODE);
    DEBUG_NAME(ED_FOL_FILE_OWNER);
    DEBUG_NAME(ED_FOL_FILE_SIZE);
    DEBUG_NAME(ED_FOL_FLAGS);
    DEBUG_NAME(ED_FOL_FLAGS2);
    DEBUG_NAME(ED_FOL_HARD_LINKS);
    DEBUG_NAME(ED_FOL_MESSAGE_COUNT);
    DEBUG_NAME(ED_FOL_NEWSGROUP);
    DEBUG_NAME(ED_FOL_NEW_COUNT);
    DEBUG_NAME(ED_FOL_NEW_MAIL);
    DEBUG_NAME(ED_FOL_NOTIFY);
    DEBUG_NAME(ED_FOL_NUMBER);
    DEBUG_NAME(ED_FOL_POLL);
    DEBUG_NAME(ED_FOL_STRF);
    DEBUG_NAME(ED_FOL_TAGGED);
    DEBUG_NAME(ED_FOL_UNREAD_COUNT);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_global(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_GLO_CERTIFICATE_PATH);
    DEBUG_NAME(ED_GLO_HOSTNAME);
    DEBUG_NAME(ED_GLO_SORT);
    DEBUG_NAME(ED_GLO_SORT_AUX);
    DEBUG_NAME(ED_GLO_USE_THREADS);
    DEBUG_NAME(ED_GLO_VERSION);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_history(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_HIS_MATCH);
    DEBUG_NAME(ED_HIS_NUMBER);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_index(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_IND_DELETED_COUNT);
    DEBUG_NAME(ED_IND_DESCRIPTION);
    DEBUG_NAME(ED_IND_FLAGGED_COUNT);
    DEBUG_NAME(ED_IND_LIMIT_COUNT);
    DEBUG_NAME(ED_IND_LIMIT_PATTERN);
    DEBUG_NAME(ED_IND_LIMIT_SIZE);
    DEBUG_NAME(ED_IND_MAILBOX_PATH);
    DEBUG_NAME(ED_IND_MAILBOX_SIZE);
    DEBUG_NAME(ED_IND_MESSAGE_COUNT);
    DEBUG_NAME(ED_IND_NEW_COUNT);
    DEBUG_NAME(ED_IND_OLD_COUNT);
    DEBUG_NAME(ED_IND_POSTPONED_COUNT);
    DEBUG_NAME(ED_IND_READONLY);
    DEBUG_NAME(ED_IND_READ_COUNT);
    DEBUG_NAME(ED_IND_TAGGED_COUNT);
    DEBUG_NAME(ED_IND_UNREAD_COUNT);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_mailbox(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_MBX_MAILBOX_NAME);
    DEBUG_NAME(ED_MBX_MESSAGE_COUNT);
    DEBUG_NAME(ED_MBX_PERCENTAGE);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_menu(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_MEN_PERCENTAGE);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_mixmaster(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_MIX_ADDRESS);
    DEBUG_NAME(ED_MIX_CAPABILITIES);
    DEBUG_NAME(ED_MIX_NUMBER);
    DEBUG_NAME(ED_MIX_SHORT_NAME);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_nntp(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_NTP_ACCOUNT);
    DEBUG_NAME(ED_NTP_PORT);
    DEBUG_NAME(ED_NTP_PORT_IF);
    DEBUG_NAME(ED_NTP_SCHEMA);
    DEBUG_NAME(ED_NTP_SERVER);
    DEBUG_NAME(ED_NTP_USERNAME);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_pattern(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_PAT_DESCRIPTION);
    DEBUG_NAME(ED_PAT_EXPRESION);
    DEBUG_NAME(ED_PAT_NUMBER);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_pgp(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_PGP_NUMBER);
    DEBUG_NAME(ED_PGP_TRUST);
    DEBUG_NAME(ED_PGP_USER_ID);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_pgp_cmd(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_PGC_FILE_MESSAGE);
    DEBUG_NAME(ED_PGC_FILE_SIGNATURE);
    DEBUG_NAME(ED_PGC_KEY_IDS);
    DEBUG_NAME(ED_PGC_NEED_PASS);
    DEBUG_NAME(ED_PGC_SIGN_AS);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_pgp_key(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_PGK_DATE);
    DEBUG_NAME(ED_PGK_KEY_ALGORITHM);
    DEBUG_NAME(ED_PGK_KEY_CAPABILITIES);
    DEBUG_NAME(ED_PGK_KEY_FLAGS);
    DEBUG_NAME(ED_PGK_KEY_ID);
    DEBUG_NAME(ED_PGK_KEY_LENGTH);
    DEBUG_NAME(ED_PGK_PKEY_ALGORITHM);
    DEBUG_NAME(ED_PGK_PKEY_CAPABILITIES);
    DEBUG_NAME(ED_PGK_PKEY_FLAGS);
    DEBUG_NAME(ED_PGK_PKEY_ID);
    DEBUG_NAME(ED_PGK_PKEY_LENGTH);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_sidebar(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_SID_DELETED_COUNT);
    DEBUG_NAME(ED_SID_DESCRIPTION);
    DEBUG_NAME(ED_SID_FLAGGED);
    DEBUG_NAME(ED_SID_FLAGGED_COUNT);
    DEBUG_NAME(ED_SID_LIMITED_COUNT);
    DEBUG_NAME(ED_SID_MESSAGE_COUNT);
    DEBUG_NAME(ED_SID_NAME);
    DEBUG_NAME(ED_SID_NEW_MAIL);
    DEBUG_NAME(ED_SID_NOTIFY);
    DEBUG_NAME(ED_SID_OLD_COUNT);
    DEBUG_NAME(ED_SID_POLL);
    DEBUG_NAME(ED_SID_READ_COUNT);
    DEBUG_NAME(ED_SID_TAGGED_COUNT);
    DEBUG_NAME(ED_SID_UNREAD_COUNT);
    DEBUG_NAME(ED_SID_UNSEEN_COUNT);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid_smime_cmd(int uid)
{
  switch (uid)
  {
    DEBUG_NAME(ED_SMI_ALGORITHM);
    DEBUG_NAME(ED_SMI_CERTIFICATE_IDS);
    DEBUG_NAME(ED_SMI_DIGEST_ALGORITHM);
    DEBUG_NAME(ED_SMI_INTERMEDIATE_IDS);
    DEBUG_NAME(ED_SMI_KEY);
    DEBUG_NAME(ED_SMI_MESSAGE_FILE);
    DEBUG_NAME(ED_SMI_SIGNATURE_FILE);
    DEBUG_DEFAULT;
  }
}

const char *name_expando_uid(enum ExpandoDomain did, int uid)
{
  switch (did)
  {
    case ED_ALIAS:
      return name_expando_uid_alias(uid);
    case ED_ATTACH:
      return name_expando_uid_attach(uid);
#ifdef USE_AUTOCRYPT
    case ED_AUTOCRYPT:
      return name_expando_uid_autocrypt(uid);
#endif
    case ED_BODY:
      return name_expando_uid_body(uid);
    case ED_COMPOSE:
      return name_expando_uid_compose(uid);
    case ED_COMPRESS:
      return name_expando_uid_compress(uid);
    case ED_EMAIL:
      return name_expando_uid_email(uid);
    case ED_ENVELOPE:
      return name_expando_uid_envelope(uid);
    case ED_FOLDER:
      return name_expando_uid_folder(uid);
    case ED_GLOBAL:
      return name_expando_uid_global(uid);
    case ED_HISTORY:
      return name_expando_uid_history(uid);
    case ED_INDEX:
      return name_expando_uid_index(uid);
    case ED_MAILBOX:
      return name_expando_uid_mailbox(uid);
    case ED_MENU:
      return name_expando_uid_menu(uid);
    case ED_MIXMASTER:
      return name_expando_uid_mixmaster(uid);
    case ED_NNTP:
      return name_expando_uid_nntp(uid);
    case ED_PATTERN:
      return name_expando_uid_pattern(uid);
    case ED_PGP:
      return name_expando_uid_pgp(uid);
    case ED_PGP_CMD:
      return name_expando_uid_pgp_cmd(uid);
    case ED_PGP_KEY:
      return name_expando_uid_pgp_key(uid);
    case ED_SIDEBAR:
      return name_expando_uid_sidebar(uid);
    case ED_SMIME_CMD:
      return name_expando_uid_smime_cmd(uid);
    default:
      assert(false);
  }
}

const char *name_format_justify(enum FormatJustify just)
{
  switch (just)
  {
    DEBUG_NAME(JUSTIFY_LEFT);
    DEBUG_NAME(JUSTIFY_CENTER);
    DEBUG_NAME(JUSTIFY_RIGHT);
    DEBUG_DEFAULT;
  }
}
