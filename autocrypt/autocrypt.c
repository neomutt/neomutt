/**
 * @file
 * XXX
 *
 * @authors
 * Copyright (C) 2019 Kevin J. McCarthy <kevin@8t8.us>
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
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "autocrypt_private.h"
#include "address/lib.h"
#include "email/lib.h"
#include "mutt.h"
#include "autocrypt.h"
#include "curs_lib.h"
#include "globals.h"
#include "mutt_curses.h"
#include "send.h"

static int autocrypt_dir_init(int can_create)
{
  int rv = 0;
  struct stat sb;
  struct Buffer *prompt = NULL;

  if (!stat(C_AutocryptDir, &sb))
    return 0;

  if (!can_create)
    return -1;

  prompt = mutt_buffer_pool_get();
  mutt_buffer_printf(prompt, _("%s does not exist. Create it?"), C_AutocryptDir);
  if (mutt_yesorno(mutt_b2s(prompt), MUTT_YES) == MUTT_YES)
  {
    if (mutt_file_mkdir(C_AutocryptDir, S_IRWXU) < 0)
    {
      mutt_error(_("Can't create %s: %s."), C_AutocryptDir, strerror(errno));
      rv = -1;
    }
  }

  mutt_buffer_pool_release(&prompt);
  return rv;
}

int mutt_autocrypt_init(int can_create)
{
  if (AutocryptDB)
    return 0;

  if (!C_Autocrypt || !C_AutocryptDir)
    return -1;

  if (autocrypt_dir_init(can_create))
    goto bail;

  if (mutt_autocrypt_gpgme_init())
    goto bail;

  if (mutt_autocrypt_db_init(can_create))
    goto bail;

  return 0;

bail:
  C_Autocrypt = false;
  mutt_autocrypt_db_close();
  return -1;
}

void mutt_autocrypt_cleanup(void)
{
  mutt_autocrypt_db_close();
}

/* Creates a brand new account the first time autocrypt is initialized */
int mutt_autocrypt_account_init(void)
{
  struct Address *addr = NULL;
  struct AutocryptAccount *account = NULL;
  bool done = false;
  int rc = -1;

  mutt_debug(LL_DEBUG1, "In mutt_autocrypt_account_init\n");
  if (mutt_yesorno(_("Create an initial autocrypt account?"), MUTT_YES) != MUTT_YES)
    return 0;

  struct Buffer *keyid = mutt_buffer_pool_get();
  struct Buffer *keydata = mutt_buffer_pool_get();

  if (C_From)
  {
    addr = mutt_addr_copy(C_From);
    if (!addr->personal && C_Realname)
      addr->personal = mutt_str_strdup(C_Realname);
  }

  struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
  mutt_addrlist_append(&al, addr);

  do
  {
    if (mutt_edit_address(&al, _("Autocrypt account address: "), 0) != 0)
      goto cleanup;

    addr = TAILQ_FIRST(&al);
    if (!addr || !addr->mailbox || TAILQ_NEXT(addr, entries))
    {
      /* L10N:
         Autocrypt prompts for an account email address, and requires
         a single address.
      */
      mutt_error(_("Please enter a single email address"));
      done = false;
    }
    else
      done = true;
  } while (!done);

  addr = TAILQ_FIRST(&al);
  if (mutt_autocrypt_db_account_get(addr, &account) < 0)
    goto cleanup;
  if (account)
  {
    mutt_error(_("That email address already has an autocrypt account"));
    goto cleanup;
  }

  if (mutt_autocrypt_gpgme_create_key(addr, keyid, keydata))
    goto cleanup;

  /* TODO: prompt for prefer_encrypt value? */
  if (mutt_autocrypt_db_account_insert(addr, mutt_b2s(keyid), mutt_b2s(keydata), 0))
    goto cleanup;

  rc = 0;

cleanup:
  if (rc == 0)
    mutt_message(_("Autocrypt account creation succeeded"));
  else
    mutt_error(_("Autocrypt account creation aborted"));

  mutt_autocrypt_db_account_free(&account);
  mutt_addrlist_clear(&al);
  mutt_buffer_pool_release(&keyid);
  mutt_buffer_pool_release(&keydata);
  return rc;
}

int mutt_autocrypt_process_autocrypt_header(struct Email *e, struct Envelope *env)
{
  struct AutocryptHeader *ac_hdr, *valid_ac_hdr = NULL;
  struct timeval now;
  struct AutocryptPeer *peer = NULL;
  struct AutocryptPeerHistory *peerhist = NULL;
  struct Buffer *keyid = NULL;
  int update_db = 0, insert_db = 0, insert_db_history = 0, import_gpg = 0;
  int rv = -1;

  if (!C_Autocrypt)
    return 0;

  if (mutt_autocrypt_init(0))
    return -1;

  if (!e || !e->content || !env)
    return 0;

  /* 1.1 spec says to skip emails with more than one From header */
  struct Address *from = TAILQ_FIRST(&env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return 0;

  /* 1.1 spec also says to skip multipart/report emails */
  if ((e->content->type == TYPE_MULTIPART) &&
      !(mutt_str_strcasecmp(e->content->subtype, "report")))
    return 0;

  /* Ignore emails that appear to be more than a week in the future,
   * since they can block all future updates during that time. */
  gettimeofday(&now, NULL);
  if (e->date_sent > (now.tv_sec + 7 * 24 * 60 * 60))
    return 0;

  for (ac_hdr = env->autocrypt; ac_hdr; ac_hdr = ac_hdr->next)
  {
    if (ac_hdr->invalid)
      continue;

    /* NOTE: this assumes the processing is occurring right after
     * mutt_parse_rfc822_line() and the from ADDR is still in the same
     * form (intl) as the autocrypt header addr field */
    if (mutt_str_strcasecmp(from->mailbox, ac_hdr->addr))
      continue;

    /* 1.1 spec says ignore all, if more than one valid header is found. */
    if (valid_ac_hdr)
    {
      valid_ac_hdr = NULL;
      break;
    }
    valid_ac_hdr = ac_hdr;
  }

  if (mutt_autocrypt_db_peer_get(from, &peer) < 0)
    goto cleanup;

  if (peer)
  {
    if (e->date_sent <= peer->autocrypt_timestamp)
    {
      rv = 0;
      goto cleanup;
    }

    if (e->date_sent > peer->last_seen)
    {
      update_db = 1;
      peer->last_seen = e->date_sent;
    }

    if (valid_ac_hdr)
    {
      update_db = 1;
      peer->autocrypt_timestamp = e->date_sent;
      peer->prefer_encrypt = valid_ac_hdr->prefer_encrypt;
      if (mutt_str_strcmp(peer->keydata, valid_ac_hdr->keydata))
      {
        import_gpg = 1;
        insert_db_history = 1;
        mutt_str_replace(&peer->keydata, valid_ac_hdr->keydata);
      }
    }
  }
  else if (valid_ac_hdr)
  {
    import_gpg = 1;
    insert_db = 1;
    insert_db_history = 1;
  }

  if (!(import_gpg || insert_db || update_db))
  {
    rv = 0;
    goto cleanup;
  }

  if (!peer)
  {
    peer = mutt_autocrypt_db_peer_new();
    peer->last_seen = e->date_sent;
    peer->autocrypt_timestamp = e->date_sent;
    peer->keydata = mutt_str_strdup(valid_ac_hdr->keydata);
    peer->prefer_encrypt = valid_ac_hdr->prefer_encrypt;
  }

  if (import_gpg)
  {
    keyid = mutt_buffer_pool_get();
    if (mutt_autocrypt_gpgme_import_key(peer->keydata, keyid))
      goto cleanup;
    mutt_str_replace(&peer->keyid, mutt_b2s(keyid));
  }

  if (insert_db && mutt_autocrypt_db_peer_insert(from, peer))
    goto cleanup;

  if (update_db && mutt_autocrypt_db_peer_update(from, peer))
    goto cleanup;

  if (insert_db_history)
  {
    peerhist = mutt_autocrypt_db_peer_history_new();
    peerhist->email_msgid = mutt_str_strdup(env->message_id);
    peerhist->timestamp = e->date_sent;
    peerhist->keydata = mutt_str_strdup(peer->keydata);
    if (mutt_autocrypt_db_peer_history_insert(from, peerhist))
      goto cleanup;
  }

  rv = 0;

cleanup:
  mutt_autocrypt_db_peer_free(&peer);
  mutt_autocrypt_db_peer_history_free(&peerhist);
  mutt_buffer_pool_release(&keyid);

  return rv;
}
