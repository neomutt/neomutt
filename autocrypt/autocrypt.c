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
#include "ncrypt/ncrypt.h"
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

int mutt_autocrypt_process_gossip_header(struct Email *hdr, struct Envelope *prot_headers)
{
  struct Envelope *env;
  struct AutocryptHeader *ac_hdr;
  struct timeval now;
  struct AutocryptPeer *peer = NULL;
  struct AutocryptGossipHistory *gossip_hist = NULL;
  struct Address *peer_addr;
  struct Address ac_hdr_addr = { 0 };
  struct Buffer *keyid = NULL;
  int update_db = 0, insert_db = 0, insert_db_history = 0, import_gpg = 0;
  int rv = -1;

  if (!C_Autocrypt)
    return 0;

  if (mutt_autocrypt_init(0))
    return -1;

  if (!hdr || !hdr->env || !prot_headers)
    return 0;

  env = hdr->env;

  struct Address *from = TAILQ_FIRST(&env->from);
  if (!from)
    return 0;

  /* Ignore emails that appear to be more than a week in the future,
   * since they can block all future updates during that time. */
  gettimeofday(&now, NULL);
  if (hdr->date_sent > (now.tv_sec + 7 * 24 * 60 * 60))
    return 0;

  keyid = mutt_buffer_pool_get();

  struct AddressList recips = TAILQ_HEAD_INITIALIZER(recips);

  /* Normalize the recipient list for comparison */
  mutt_addrlist_copy(&recips, &env->to, false);
  mutt_addrlist_copy(&recips, &env->cc, false);
  mutt_addrlist_copy(&recips, &env->reply_to, false);
  mutt_autocrypt_db_normalize_addrlist(&recips);

  for (ac_hdr = prot_headers->autocrypt_gossip; ac_hdr; ac_hdr = ac_hdr->next)
  {
    if (ac_hdr->invalid)
      continue;

    /* normalize for comparison against recipient list */
    mutt_str_replace(&ac_hdr_addr.mailbox, ac_hdr->addr);
    ac_hdr_addr.is_intl = 1;
    ac_hdr_addr.intl_checked = 1;
    mutt_autocrypt_db_normalize_addr(&ac_hdr_addr);

    /* Check to make sure the address is in the recipient list.  Since the
     * addresses are normalized we use strcmp, not mutt_str_strcasecmp. */
    TAILQ_FOREACH(peer_addr, &recips, entries)
    {
      if (!mutt_str_strcmp(peer_addr->mailbox, ac_hdr_addr.mailbox))
        break;
    }

    if (!peer_addr)
      continue;

    if (mutt_autocrypt_db_peer_get(peer_addr, &peer) < 0)
      goto cleanup;

    if (peer)
    {
      if (hdr->date_sent <= peer->gossip_timestamp)
      {
        mutt_autocrypt_db_peer_free(&peer);
        continue;
      }

      update_db = 1;
      peer->gossip_timestamp = hdr->date_sent;
      /* This is slightly different from the autocrypt 1.1 spec.
       * Avoid setting an empty peer.gossip_keydata with a value that matches
       * the current peer.keydata. */
      if ((peer->gossip_keydata && mutt_str_strcmp(peer->gossip_keydata, ac_hdr->keydata)) ||
          (!peer->gossip_keydata && mutt_str_strcmp(peer->keydata, ac_hdr->keydata)))
      {
        import_gpg = 1;
        insert_db_history = 1;
        mutt_str_replace(&peer->gossip_keydata, ac_hdr->keydata);
      }
    }
    else
    {
      import_gpg = 1;
      insert_db = 1;
      insert_db_history = 1;
    }

    if (!peer)
    {
      peer = mutt_autocrypt_db_peer_new();
      peer->gossip_timestamp = hdr->date_sent;
      peer->gossip_keydata = mutt_str_strdup(ac_hdr->keydata);
    }

    if (import_gpg)
    {
      if (mutt_autocrypt_gpgme_import_key(peer->gossip_keydata, keyid))
        goto cleanup;
      mutt_str_replace(&peer->gossip_keyid, mutt_b2s(keyid));
    }

    if (insert_db && mutt_autocrypt_db_peer_insert(peer_addr, peer))
      goto cleanup;

    if (update_db && mutt_autocrypt_db_peer_update(peer_addr, peer))
      goto cleanup;

    if (insert_db_history)
    {
      gossip_hist = mutt_autocrypt_db_gossip_history_new();
      gossip_hist->sender_email_addr = mutt_str_strdup(from->mailbox);
      gossip_hist->email_msgid = mutt_str_strdup(env->message_id);
      gossip_hist->timestamp = hdr->date_sent;
      gossip_hist->gossip_keydata = mutt_str_strdup(peer->gossip_keydata);
      if (mutt_autocrypt_db_gossip_history_insert(peer_addr, gossip_hist))
        goto cleanup;
    }

    mutt_autocrypt_db_peer_free(&peer);
    mutt_autocrypt_db_gossip_history_free(&gossip_hist);
    mutt_buffer_reset(keyid);
    update_db = insert_db = insert_db_history = import_gpg = 0;
  }

  rv = 0;

cleanup:
  FREE(&ac_hdr_addr.mailbox);
  mutt_addrlist_clear(&recips);
  mutt_autocrypt_db_peer_free(&peer);
  mutt_autocrypt_db_gossip_history_free(&gossip_hist);
  mutt_buffer_pool_release(&keyid);

  return rv;
}

/* Returns the recommendation.  If the recommendataion is > NO and
 * keylist is not NULL, keylist will be populated with the autocrypt
 * keyids
 */
enum AutocryptRec mutt_autocrypt_ui_recommendation(struct Email *hdr, char **keylist)
{
  enum AutocryptRec rv = AUTOCRYPT_REC_OFF;
  struct AutocryptAccount *account = NULL;
  struct AutocryptPeer *peer = NULL;
  struct Address *recip = NULL;
  int all_encrypt = 1, has_discourage = 0;
  struct Buffer *keylist_buf = NULL;
  const char *matching_key;

  if (!C_Autocrypt || mutt_autocrypt_init(0) || !hdr)
    return AUTOCRYPT_REC_OFF;

  struct Address *from = TAILQ_FIRST(&hdr->env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return AUTOCRYPT_REC_OFF;

  if (hdr->security & APPLICATION_SMIME)
    return AUTOCRYPT_REC_OFF;

  if (mutt_autocrypt_db_account_get(from, &account) <= 0)
    goto cleanup;

  keylist_buf = mutt_buffer_pool_get();
  mutt_buffer_addstr(keylist_buf, account->keyid);

  struct AddressList recips = TAILQ_HEAD_INITIALIZER(recips);

  mutt_addrlist_copy(&recips, &hdr->env->to, false);
  mutt_addrlist_copy(&recips, &hdr->env->cc, false);
  mutt_addrlist_copy(&recips, &hdr->env->bcc, false);

  rv = AUTOCRYPT_REC_NO;
  if (TAILQ_EMPTY(&recips))
    goto cleanup;

  TAILQ_FOREACH(recip, &recips, entries)
  {
    if (mutt_autocrypt_db_peer_get(recip, &peer) <= 0)
    {
      if (keylist)
        /* L10N:
           %s is an email address.  Autocrypt is scanning for the keyids
           to use to encrypt, but it can't find a valid keyid for this address.
           The message is printed and they are returned to the compose menu.
         */
        mutt_message(_("No (valid) autocrypt key found for %s."), recip->mailbox);
      goto cleanup;
    }

    if (mutt_autocrypt_gpgme_is_valid_key(peer->keyid))
    {
      matching_key = peer->keyid;

      if (!(peer->last_seen && peer->autocrypt_timestamp) ||
          (peer->last_seen - peer->autocrypt_timestamp > 35 * 24 * 60 * 60))
      {
        has_discourage = 1;
        all_encrypt = 0;
      }

      if (!account->prefer_encrypt || !peer->prefer_encrypt)
        all_encrypt = 0;
    }
    else if (mutt_autocrypt_gpgme_is_valid_key(peer->gossip_keyid))
    {
      matching_key = peer->gossip_keyid;

      has_discourage = 1;
      all_encrypt = 0;
    }
    else
    {
      if (keylist)
        mutt_message(_("No (valid) autocrypt key found for %s."), recip->mailbox);
      goto cleanup;
    }

    if (mutt_buffer_len(keylist_buf))
      mutt_buffer_addch(keylist_buf, ' ');
    mutt_buffer_addstr(keylist_buf, matching_key);

    mutt_autocrypt_db_peer_free(&peer);
  }

  if (all_encrypt)
    rv = AUTOCRYPT_REC_YES;
  else if (has_discourage)
    rv = AUTOCRYPT_REC_DISCOURAGE;
  else
    rv = AUTOCRYPT_REC_AVAILABLE;

  if (keylist)
    mutt_str_replace(keylist, mutt_b2s(keylist_buf));

cleanup:
  mutt_autocrypt_db_account_free(&account);
  mutt_addrlist_clear(&recips);
  mutt_autocrypt_db_peer_free(&peer);
  mutt_buffer_pool_release(&keylist_buf);
  return rv;
}

int mutt_autocrypt_set_sign_as_default_key(struct Email *hdr)
{
  int rv = -1;
  struct AutocryptAccount *account = NULL;

  if (!C_Autocrypt || mutt_autocrypt_init(0) || !hdr)
    return -1;

  struct Address *from = TAILQ_FIRST(&hdr->env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return -1;

  if (mutt_autocrypt_db_account_get(from, &account) <= 0)
    goto cleanup;
  if (!account->keyid)
    goto cleanup;

  mutt_str_replace(&AutocryptSignAs, account->keyid);
  mutt_str_replace(&AutocryptDefaultKey, account->keyid);

  rv = 0;

cleanup:
  mutt_autocrypt_db_account_free(&account);
  return rv;
}

static void write_autocrypt_header_line(FILE *fp, const char *addr,
                                        int prefer_encrypt, const char *keydata)
{
  int count = 0;

  fprintf(fp, "addr=%s; ", addr);
  if (prefer_encrypt)
    fputs("prefer-encrypt=mutual; ", fp);
  fputs("keydata=\n", fp);

  while (*keydata)
  {
    count = 0;
    fputs("\t", fp);
    while (*keydata && count < 75)
    {
      fputc(*keydata, fp);
      count++;
      keydata++;
    }
    fputs("\n", fp);
  }
}

int mutt_autocrypt_write_autocrypt_header(struct Envelope *env, FILE *fp)
{
  int rv = -1;
  struct AutocryptAccount *account = NULL;

  if (!C_Autocrypt || mutt_autocrypt_init(0) || !env)
    return -1;

  struct Address *from = TAILQ_FIRST(&env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return -1;

  if (mutt_autocrypt_db_account_get(from, &account) <= 0)
    goto cleanup;
  if (!account->keydata)
    goto cleanup;

  fputs("Autocrypt: ", fp);
  write_autocrypt_header_line(fp, account->email_addr, account->prefer_encrypt,
                              account->keydata);

  rv = 0;

cleanup:
  mutt_autocrypt_db_account_free(&account);
  return rv;
}

int mutt_autocrypt_write_gossip_headers(struct Envelope *env, FILE *fp)
{
  struct AutocryptHeader *gossip;

  if (!C_Autocrypt || mutt_autocrypt_init(0) || !env)
    return -1;

  for (gossip = env->autocrypt_gossip; gossip; gossip = gossip->next)
  {
    fputs("Autocrypt-Gossip: ", fp);
    write_autocrypt_header_line(fp, gossip->addr, 0, gossip->keydata);
  }

  return 0;
}

int mutt_autocrypt_generate_gossip_list(struct Email *hdr)
{
  int rv = -1;
  struct AutocryptPeer *peer = NULL;
  struct AutocryptAccount *account = NULL;
  struct Address *recip = NULL;
  struct AutocryptHeader *gossip;
  const char *keydata, *addr;
  struct Envelope *mime_headers;

  if (!C_Autocrypt || mutt_autocrypt_init(0) || !hdr)
    return -1;

  mime_headers = hdr->content->mime_headers;
  if (!mime_headers)
    mime_headers = hdr->content->mime_headers = mutt_env_new();
  mutt_free_autocrypthdr(&mime_headers->autocrypt_gossip);

  struct AddressList recips = TAILQ_HEAD_INITIALIZER(recips);

  mutt_addrlist_copy(&recips, &hdr->env->to, false);
  mutt_addrlist_copy(&recips, &hdr->env->cc, false);

  TAILQ_FOREACH(recip, &recips, entries)
  {
    /* At this point, we just accept missing keys and include what
     * we can. */
    if (mutt_autocrypt_db_peer_get(recip, &peer) <= 0)
      continue;

    keydata = NULL;
    if (mutt_autocrypt_gpgme_is_valid_key(peer->keyid))
      keydata = peer->keydata;
    else if (mutt_autocrypt_gpgme_is_valid_key(peer->gossip_keyid))
      keydata = peer->gossip_keydata;

    if (keydata)
    {
      gossip = mutt_new_autocrypthdr();
      gossip->addr = mutt_str_strdup(peer->email_addr);
      gossip->keydata = mutt_str_strdup(keydata);
      gossip->next = mime_headers->autocrypt_gossip;
      mime_headers->autocrypt_gossip = gossip;
    }

    mutt_autocrypt_db_peer_free(&peer);
  }

  TAILQ_FOREACH(recip, &hdr->env->reply_to, entries)
  {
    addr = keydata = NULL;
    if (mutt_autocrypt_db_account_get(recip, &account) > 0)
    {
      addr = account->email_addr;
      keydata = account->keydata;
    }
    else if (mutt_autocrypt_db_peer_get(recip, &peer) > 0)
    {
      addr = peer->email_addr;
      if (mutt_autocrypt_gpgme_is_valid_key(peer->keyid))
        keydata = peer->keydata;
      else if (mutt_autocrypt_gpgme_is_valid_key(peer->gossip_keyid))
        keydata = peer->gossip_keydata;
    }

    if (keydata)
    {
      gossip = mutt_new_autocrypthdr();
      gossip->addr = mutt_str_strdup(addr);
      gossip->keydata = mutt_str_strdup(keydata);
      gossip->next = mime_headers->autocrypt_gossip;
      mime_headers->autocrypt_gossip = gossip;
    }
    mutt_autocrypt_db_account_free(&account);
    mutt_autocrypt_db_peer_free(&peer);
  }

  mutt_addrlist_clear(&recips);
  mutt_autocrypt_db_account_free(&account);
  mutt_autocrypt_db_peer_free(&peer);
  return rv;
}
