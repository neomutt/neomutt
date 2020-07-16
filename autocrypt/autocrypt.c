/**
 * @file
 * Autocrypt end-to-end encryption
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

/**
 * @page autocrypt_autocrypt Autocrypt end-to-end encryption
 *
 * Autocrypt end-to-end encryption
 */

#include "config.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "gui/lib.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "options.h"
#include "autocrypt/lib.h"
#include "hcache/lib.h"
#include "ncrypt/lib.h"
#include "send/lib.h"

/**
 * autocrypt_dir_init - Initialise an Autocrypt directory
 * @param can_create If true, the directory may be created
 * @retval  0 Success
 * @retval -1 Error
 */
static int autocrypt_dir_init(bool can_create)
{
  int rc = 0;
  struct stat sb;

  if (stat(C_AutocryptDir, &sb) == 0)
    return 0;

  if (!can_create)
    return -1;

  struct Buffer *prompt = mutt_buffer_pool_get();
  /* L10N: s is a directory.  NeoMutt is looking for a directory it needs
     for some reason (e.g. autocrypt, header cache, bcache), but it
     doesn't exist.  The prompt is asking whether to create the directory */
  mutt_buffer_printf(prompt, _("%s does not exist. Create it?"), C_AutocryptDir);
  if (mutt_yesorno(mutt_b2s(prompt), MUTT_YES) == MUTT_YES)
  {
    if (mutt_file_mkdir(C_AutocryptDir, S_IRWXU) < 0)
    {
      /* L10N: mkdir() on the directory %s failed.  The second %s is the
         error message returned by libc */
      mutt_error(_("Can't create %s: %s"), C_AutocryptDir, strerror(errno));
      rc = -1;
    }
  }

  mutt_buffer_pool_release(&prompt);
  return rc;
}

/**
 * mutt_autocrypt_init - Initialise Autocrypt
 * @param can_create If true, directories may be created
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_init(bool can_create)
{
  if (AutocryptDB)
    return 0;

  if (!C_Autocrypt || !C_AutocryptDir)
    return -1;

  OptIgnoreMacroEvents = true;
  /* The init process can display menus at various points
   *(e.g. browser, pgp key selection).  This allows the screen to be
   * autocleared after each menu, so the subsequent prompts can be
   * read. */
  OptMenuPopClearScreen = true;

  if (autocrypt_dir_init(can_create))
    goto bail;

  if (mutt_autocrypt_gpgme_init())
    goto bail;

  if (mutt_autocrypt_db_init(can_create))
    goto bail;

  OptIgnoreMacroEvents = false;
  OptMenuPopClearScreen = false;

  return 0;

bail:
  OptIgnoreMacroEvents = false;
  OptMenuPopClearScreen = false;
  C_Autocrypt = false;
  mutt_autocrypt_db_close();
  return -1;
}

/**
 * mutt_autocrypt_cleanup - Shutdown Autocrypt
 */
void mutt_autocrypt_cleanup(void)
{
  mutt_autocrypt_db_close();
}

/**
 * mutt_autocrypt_account_init - Create a new Autocrypt account
 * @param prompt Prompt the user
 * @retval  0 Success
 * @retval -1 Error
 *
 * This is used the first time autocrypt is initialized,
 * and in the account menu.
 */
int mutt_autocrypt_account_init(bool prompt)
{
  struct Address *addr = NULL;
  struct AutocryptAccount *account = NULL;
  bool done = false;
  int rc = -1;
  bool prefer_encrypt = false;

  if (prompt)
  {
    /* L10N: The first time NeoMutt is started with $autocrypt set, it will
       create $autocrypt_dir and then prompt to create an autocrypt account
       with this message.  */
    if (mutt_yesorno(_("Create an initial autocrypt account?"), MUTT_YES) != MUTT_YES)
      return 0;
  }

  struct Buffer *keyid = mutt_buffer_pool_get();
  struct Buffer *keydata = mutt_buffer_pool_get();

  if (C_From)
  {
    addr = mutt_addr_copy(C_From);
    if (!addr->personal && C_Realname)
      addr->personal = mutt_str_dup(C_Realname);
  }

  struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
  mutt_addrlist_append(&al, addr);

  do
  {
    /* L10N: Autocrypt is asking for the email address to use for the
       autocrypt account.  This will generate a key and add a record
       to the database for use in autocrypt operations.  */
    if (mutt_edit_address(&al, _("Autocrypt account address: "), false) != 0)
      goto cleanup;

    addr = TAILQ_FIRST(&al);
    if (!addr || !addr->mailbox || TAILQ_NEXT(addr, entries))
    {
      /* L10N: Autocrypt prompts for an account email address, and requires
         a single address.  This is shown if they entered something invalid,
         nothing, or more than one address for some reason.  */
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
    /* L10N: When creating an autocrypt account, this message will be displayed
       if there is already an account in the database with the email address
       they just entered.  */
    mutt_error(_("That email address already has an autocrypt account"));
    goto cleanup;
  }

  if (mutt_autocrypt_gpgme_select_or_create_key(addr, keyid, keydata))
    goto cleanup;

  /* L10N: Autocrypt has a setting "prefer-encrypt".
     When the recommendation algorithm returns "available" and BOTH sender and
     recipient choose "prefer-encrypt", encryption will be automatically
     enabled.
     Otherwise the UI will show encryption is "available" but the user
     will be required to enable encryption manually.  */
  if (mutt_yesorno(_("Prefer encryption?"), MUTT_NO) == MUTT_YES)
    prefer_encrypt = true;

  if (mutt_autocrypt_db_account_insert(addr, mutt_b2s(keyid), mutt_b2s(keydata), prefer_encrypt))
    goto cleanup;

  rc = 0;

cleanup:
  if (rc == 0)
    /* L10N: Message displayed after an autocrypt account is successfully created.  */
    mutt_message(_("Autocrypt account creation succeeded"));
  else
    /* L10N: Error message displayed if creating an autocrypt account failed
       or was aborted by the user.  */
    mutt_error(_("Autocrypt account creation aborted"));

  mutt_autocrypt_db_account_free(&account);
  mutt_addrlist_clear(&al);
  mutt_buffer_pool_release(&keyid);
  mutt_buffer_pool_release(&keydata);
  return rc;
}

/**
 * mutt_autocrypt_process_autocrypt_header - Parse an Autocrypt email header
 * @param e   Email
 * @param env Envelope
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_process_autocrypt_header(struct Email *e, struct Envelope *env)
{
  struct AutocryptHeader *valid_ac_hdr = NULL;
  struct AutocryptPeer *peer = NULL;
  struct AutocryptPeerHistory *peerhist = NULL;
  struct Buffer *keyid = NULL;
  bool update_db = false, insert_db = false, insert_db_history = false, import_gpg = false;
  int rc = -1;

  if (!C_Autocrypt)
    return 0;

  if (mutt_autocrypt_init(false))
    return -1;

  if (!e || !e->content || !env)
    return 0;

  /* 1.1 spec says to skip emails with more than one From header */
  struct Address *from = TAILQ_FIRST(&env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return 0;

  /* 1.1 spec also says to skip multipart/report emails */
  if ((e->content->type == TYPE_MULTIPART) && mutt_istr_equal(e->content->subtype, "report"))
  {
    return 0;
  }

  /* Ignore emails that appear to be more than a week in the future,
   * since they can block all future updates during that time. */
  if (e->date_sent > (mutt_date_epoch() + (7 * 24 * 60 * 60)))
    return 0;

  for (struct AutocryptHeader *ac_hdr = env->autocrypt; ac_hdr; ac_hdr = ac_hdr->next)
  {
    if (ac_hdr->invalid)
      continue;

    /* NOTE: this assumes the processing is occurring right after
     * mutt_parse_rfc822_line() and the from ADDR is still in the same
     * form (intl) as the autocrypt header addr field */
    if (!mutt_istr_equal(from->mailbox, ac_hdr->addr))
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
      rc = 0;
      goto cleanup;
    }

    if (e->date_sent > peer->last_seen)
    {
      update_db = true;
      peer->last_seen = e->date_sent;
    }

    if (valid_ac_hdr)
    {
      update_db = true;
      peer->autocrypt_timestamp = e->date_sent;
      peer->prefer_encrypt = valid_ac_hdr->prefer_encrypt;
      if (!mutt_str_equal(peer->keydata, valid_ac_hdr->keydata))
      {
        import_gpg = true;
        insert_db_history = true;
        mutt_str_replace(&peer->keydata, valid_ac_hdr->keydata);
      }
    }
  }
  else if (valid_ac_hdr)
  {
    import_gpg = true;
    insert_db = true;
    insert_db_history = true;
  }

  if (!(import_gpg || insert_db || update_db))
  {
    rc = 0;
    goto cleanup;
  }

  if (!peer)
  {
    peer = mutt_autocrypt_db_peer_new();
    peer->last_seen = e->date_sent;
    peer->autocrypt_timestamp = e->date_sent;
    peer->keydata = mutt_str_dup(valid_ac_hdr->keydata);
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

  if (update_db && mutt_autocrypt_db_peer_update(peer))
    goto cleanup;

  if (insert_db_history)
  {
    peerhist = mutt_autocrypt_db_peer_history_new();
    peerhist->email_msgid = mutt_str_dup(env->message_id);
    peerhist->timestamp = e->date_sent;
    peerhist->keydata = mutt_str_dup(peer->keydata);
    if (mutt_autocrypt_db_peer_history_insert(from, peerhist))
      goto cleanup;
  }

  rc = 0;

cleanup:
  mutt_autocrypt_db_peer_free(&peer);
  mutt_autocrypt_db_peer_history_free(&peerhist);
  mutt_buffer_pool_release(&keyid);

  return rc;
}

/**
 * mutt_autocrypt_process_gossip_header - Parse an Autocrypt email gossip header
 * @param e            Email
 * @param prot_headers Envelope with protected headers
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_process_gossip_header(struct Email *e, struct Envelope *prot_headers)
{
  struct AutocryptPeer *peer = NULL;
  struct AutocryptGossipHistory *gossip_hist = NULL;
  struct Address *peer_addr = NULL;
  struct Address ac_hdr_addr = { 0 };
  bool update_db = false, insert_db = false, insert_db_history = false, import_gpg = false;
  int rc = -1;

  if (!C_Autocrypt)
    return 0;

  if (mutt_autocrypt_init(false))
    return -1;

  if (!e || !e->env || !prot_headers)
    return 0;

  struct Envelope *env = e->env;

  struct Address *from = TAILQ_FIRST(&env->from);
  if (!from)
    return 0;

  /* Ignore emails that appear to be more than a week in the future,
   * since they can block all future updates during that time. */
  if (e->date_sent > (mutt_date_epoch() + (7 * 24 * 60 * 60)))
    return 0;

  struct Buffer *keyid = mutt_buffer_pool_get();

  struct AddressList recips = TAILQ_HEAD_INITIALIZER(recips);

  /* Normalize the recipient list for comparison */
  mutt_addrlist_copy(&recips, &env->to, false);
  mutt_addrlist_copy(&recips, &env->cc, false);
  mutt_addrlist_copy(&recips, &env->reply_to, false);
  mutt_autocrypt_db_normalize_addrlist(&recips);

  for (struct AutocryptHeader *ac_hdr = prot_headers->autocrypt_gossip; ac_hdr;
       ac_hdr = ac_hdr->next)
  {
    if (ac_hdr->invalid)
      continue;

    /* normalize for comparison against recipient list */
    mutt_str_replace(&ac_hdr_addr.mailbox, ac_hdr->addr);
    ac_hdr_addr.is_intl = true;
    ac_hdr_addr.intl_checked = true;
    mutt_autocrypt_db_normalize_addr(&ac_hdr_addr);

    /* Check to make sure the address is in the recipient list. */
    TAILQ_FOREACH(peer_addr, &recips, entries)
    {
      if (mutt_str_equal(peer_addr->mailbox, ac_hdr_addr.mailbox))
        break;
    }

    if (!peer_addr)
      continue;

    if (mutt_autocrypt_db_peer_get(peer_addr, &peer) < 0)
      goto cleanup;

    if (peer)
    {
      if (e->date_sent <= peer->gossip_timestamp)
      {
        mutt_autocrypt_db_peer_free(&peer);
        continue;
      }

      update_db = true;
      peer->gossip_timestamp = e->date_sent;
      /* This is slightly different from the autocrypt 1.1 spec.
       * Avoid setting an empty peer.gossip_keydata with a value that matches
       * the current peer.keydata. */
      if ((peer->gossip_keydata && !mutt_str_equal(peer->gossip_keydata, ac_hdr->keydata)) ||
          (!peer->gossip_keydata && !mutt_str_equal(peer->keydata, ac_hdr->keydata)))
      {
        import_gpg = true;
        insert_db_history = true;
        mutt_str_replace(&peer->gossip_keydata, ac_hdr->keydata);
      }
    }
    else
    {
      import_gpg = true;
      insert_db = true;
      insert_db_history = true;
    }

    if (!peer)
    {
      peer = mutt_autocrypt_db_peer_new();
      peer->gossip_timestamp = e->date_sent;
      peer->gossip_keydata = mutt_str_dup(ac_hdr->keydata);
    }

    if (import_gpg)
    {
      if (mutt_autocrypt_gpgme_import_key(peer->gossip_keydata, keyid))
        goto cleanup;
      mutt_str_replace(&peer->gossip_keyid, mutt_b2s(keyid));
    }

    if (insert_db && mutt_autocrypt_db_peer_insert(peer_addr, peer))
      goto cleanup;

    if (update_db && mutt_autocrypt_db_peer_update(peer))
      goto cleanup;

    if (insert_db_history)
    {
      gossip_hist = mutt_autocrypt_db_gossip_history_new();
      gossip_hist->sender_email_addr = mutt_str_dup(from->mailbox);
      gossip_hist->email_msgid = mutt_str_dup(env->message_id);
      gossip_hist->timestamp = e->date_sent;
      gossip_hist->gossip_keydata = mutt_str_dup(peer->gossip_keydata);
      if (mutt_autocrypt_db_gossip_history_insert(peer_addr, gossip_hist))
        goto cleanup;
    }

    mutt_autocrypt_db_peer_free(&peer);
    mutt_autocrypt_db_gossip_history_free(&gossip_hist);
    mutt_buffer_reset(keyid);
    update_db = false;
    insert_db = false;
    insert_db_history = false;
    import_gpg = false;
  }

  rc = 0;

cleanup:
  FREE(&ac_hdr_addr.mailbox);
  mutt_addrlist_clear(&recips);
  mutt_autocrypt_db_peer_free(&peer);
  mutt_autocrypt_db_gossip_history_free(&gossip_hist);
  mutt_buffer_pool_release(&keyid);

  return rc;
}

/**
 * mutt_autocrypt_ui_recommendation - Get the recommended action for an Email
 * @param[in]  e       Email
 * @param[out] keylist List of Autocrypt key ids
 * @retval num Recommendation, e.g. #AUTOCRYPT_REC_AVAILABLE
 *
 * If the recommendataion is > NO and keylist is not NULL, keylist will be
 * populated with the autocrypt keyids.
 */
enum AutocryptRec mutt_autocrypt_ui_recommendation(struct Email *e, char **keylist)
{
  enum AutocryptRec rc = AUTOCRYPT_REC_OFF;
  struct AutocryptAccount *account = NULL;
  struct AutocryptPeer *peer = NULL;
  struct Address *recip = NULL;
  bool all_encrypt = true, has_discourage = false;
  const char *matching_key = NULL;
  struct AddressList recips = TAILQ_HEAD_INITIALIZER(recips);
  struct Buffer *keylist_buf = NULL;

  if (!C_Autocrypt || mutt_autocrypt_init(false) || !e)
  {
    if (keylist)
    {
      /* L10N: Error displayed if the user tries to force sending an Autocrypt
         email when the engine is not available.  */
      mutt_message(_("Autocrypt is not available"));
    }
    return AUTOCRYPT_REC_OFF;
  }

  struct Address *from = TAILQ_FIRST(&e->env->from);
  if (!from || TAILQ_NEXT(from, entries))
  {
    if (keylist)
      mutt_message(_("Autocrypt is not available"));
    return AUTOCRYPT_REC_OFF;
  }

  if (e->security & APPLICATION_SMIME)
  {
    if (keylist)
      mutt_message(_("Autocrypt is not available"));
    return AUTOCRYPT_REC_OFF;
  }

  if ((mutt_autocrypt_db_account_get(from, &account) <= 0) || !account->enabled)
  {
    if (keylist)
    {
      /* L10N: Error displayed if the user tries to force sending an Autocrypt
         email when the account does not exist or is not enabled.
         %s is the From email address used to look up the Autocrypt account.
      */
      mutt_message(_("Autocrypt is not enabled for %s"), NONULL(from->mailbox));
    }
    goto cleanup;
  }

  keylist_buf = mutt_buffer_pool_get();
  mutt_buffer_addstr(keylist_buf, account->keyid);

  mutt_addrlist_copy(&recips, &e->env->to, false);
  mutt_addrlist_copy(&recips, &e->env->cc, false);
  mutt_addrlist_copy(&recips, &e->env->bcc, false);

  rc = AUTOCRYPT_REC_NO;
  if (TAILQ_EMPTY(&recips))
    goto cleanup;

  TAILQ_FOREACH(recip, &recips, entries)
  {
    if (mutt_autocrypt_db_peer_get(recip, &peer) <= 0)
    {
      if (keylist)
      {
        /* L10N: s is an email address.  Autocrypt is scanning for the keyids
           to use to encrypt, but it can't find a valid keyid for this address.
           The message is printed and they are returned to the compose menu.  */
        mutt_message(_("No (valid) autocrypt key found for %s"), recip->mailbox);
      }
      goto cleanup;
    }

    if (mutt_autocrypt_gpgme_is_valid_key(peer->keyid))
    {
      matching_key = peer->keyid;

      if (!(peer->last_seen && peer->autocrypt_timestamp) ||
          (peer->last_seen - peer->autocrypt_timestamp > (35 * 24 * 60 * 60)))
      {
        has_discourage = true;
        all_encrypt = false;
      }

      if (!account->prefer_encrypt || !peer->prefer_encrypt)
        all_encrypt = false;
    }
    else if (mutt_autocrypt_gpgme_is_valid_key(peer->gossip_keyid))
    {
      matching_key = peer->gossip_keyid;

      has_discourage = true;
      all_encrypt = false;
    }
    else
    {
      if (keylist)
        mutt_message(_("No (valid) autocrypt key found for %s"), recip->mailbox);
      goto cleanup;
    }

    if (!mutt_buffer_is_empty(keylist_buf))
      mutt_buffer_addch(keylist_buf, ' ');
    mutt_buffer_addstr(keylist_buf, matching_key);

    mutt_autocrypt_db_peer_free(&peer);
  }

  if (all_encrypt)
    rc = AUTOCRYPT_REC_YES;
  else if (has_discourage)
    rc = AUTOCRYPT_REC_DISCOURAGE;
  else
    rc = AUTOCRYPT_REC_AVAILABLE;

  if (keylist)
    mutt_str_replace(keylist, mutt_b2s(keylist_buf));

cleanup:
  mutt_autocrypt_db_account_free(&account);
  mutt_addrlist_clear(&recips);
  mutt_autocrypt_db_peer_free(&peer);
  mutt_buffer_pool_release(&keylist_buf);
  return rc;
}

/**
 * mutt_autocrypt_set_sign_as_default_key - Set the Autocrypt default key for signing
 * @param e Email
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_set_sign_as_default_key(struct Email *e)
{
  int rc = -1;
  struct AutocryptAccount *account = NULL;

  if (!C_Autocrypt || mutt_autocrypt_init(false) || !e)
    return -1;

  struct Address *from = TAILQ_FIRST(&e->env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return -1;

  if (mutt_autocrypt_db_account_get(from, &account) <= 0)
    goto cleanup;
  if (!account->keyid)
    goto cleanup;
  if (!account->enabled)
    goto cleanup;

  mutt_str_replace(&AutocryptSignAs, account->keyid);
  mutt_str_replace(&AutocryptDefaultKey, account->keyid);

  rc = 0;

cleanup:
  mutt_autocrypt_db_account_free(&account);
  return rc;
}

/**
 * write_autocrypt_header_line - Write an Autocrypt header to a file
 * @param fp             File to write to
 * @param addr           Email address
 * @param prefer_encrypt Whether encryption is preferred
 * @param keydata        Raw Autocrypt data
 */
static void write_autocrypt_header_line(FILE *fp, const char *addr,
                                        bool prefer_encrypt, const char *keydata)
{
  fprintf(fp, "addr=%s; ", addr);
  if (prefer_encrypt)
    fputs("prefer-encrypt=mutual; ", fp);
  fputs("keydata=\n", fp);

  while (*keydata)
  {
    int count = 0;
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

/**
 * mutt_autocrypt_write_autocrypt_header - Write the Autocrypt header to a file
 * @param env Envelope
 * @param fp  File to write to
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_write_autocrypt_header(struct Envelope *env, FILE *fp)
{
  int rc = -1;
  struct AutocryptAccount *account = NULL;

  if (!C_Autocrypt || mutt_autocrypt_init(false) || !env)
    return -1;

  struct Address *from = TAILQ_FIRST(&env->from);
  if (!from || TAILQ_NEXT(from, entries))
    return -1;

  if (mutt_autocrypt_db_account_get(from, &account) <= 0)
    goto cleanup;
  if (!account->keydata)
    goto cleanup;
  if (!account->enabled)
    goto cleanup;

  fputs("Autocrypt: ", fp);
  write_autocrypt_header_line(fp, account->email_addr, account->prefer_encrypt,
                              account->keydata);

  rc = 0;

cleanup:
  mutt_autocrypt_db_account_free(&account);
  return rc;
}

/**
 * mutt_autocrypt_write_gossip_headers - Write the Autocrypt gossip headers to a file
 * @param env Envelope
 * @param fp  File to write to
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_write_gossip_headers(struct Envelope *env, FILE *fp)
{
  if (!C_Autocrypt || mutt_autocrypt_init(false) || !env)
    return -1;

  for (struct AutocryptHeader *gossip = env->autocrypt_gossip; gossip;
       gossip = gossip->next)
  {
    fputs("Autocrypt-Gossip: ", fp);
    write_autocrypt_header_line(fp, gossip->addr, 0, gossip->keydata);
  }

  return 0;
}

/**
 * mutt_autocrypt_generate_gossip_list - Create the gossip list headers
 * @param e Email
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_generate_gossip_list(struct Email *e)
{
  int rc = -1;
  struct AutocryptPeer *peer = NULL;
  struct AutocryptAccount *account = NULL;
  struct Address *recip = NULL;

  if (!C_Autocrypt || mutt_autocrypt_init(false) || !e)
    return -1;

  struct Envelope *mime_headers = e->content->mime_headers;
  if (!mime_headers)
    mime_headers = e->content->mime_headers = mutt_env_new();
  mutt_autocrypthdr_free(&mime_headers->autocrypt_gossip);

  struct AddressList recips = TAILQ_HEAD_INITIALIZER(recips);

  mutt_addrlist_copy(&recips, &e->env->to, false);
  mutt_addrlist_copy(&recips, &e->env->cc, false);

  TAILQ_FOREACH(recip, &recips, entries)
  {
    /* At this point, we just accept missing keys and include what we can. */
    if (mutt_autocrypt_db_peer_get(recip, &peer) <= 0)
      continue;

    const char *keydata = NULL;
    if (mutt_autocrypt_gpgme_is_valid_key(peer->keyid))
      keydata = peer->keydata;
    else if (mutt_autocrypt_gpgme_is_valid_key(peer->gossip_keyid))
      keydata = peer->gossip_keydata;

    if (keydata)
    {
      struct AutocryptHeader *gossip = mutt_autocrypthdr_new();
      gossip->addr = mutt_str_dup(peer->email_addr);
      gossip->keydata = mutt_str_dup(keydata);
      gossip->next = mime_headers->autocrypt_gossip;
      mime_headers->autocrypt_gossip = gossip;
    }

    mutt_autocrypt_db_peer_free(&peer);
  }

  TAILQ_FOREACH(recip, &e->env->reply_to, entries)
  {
    const char *addr = NULL;
    const char *keydata = NULL;
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
      struct AutocryptHeader *gossip = mutt_autocrypthdr_new();
      gossip->addr = mutt_str_dup(addr);
      gossip->keydata = mutt_str_dup(keydata);
      gossip->next = mime_headers->autocrypt_gossip;
      mime_headers->autocrypt_gossip = gossip;
    }
    mutt_autocrypt_db_account_free(&account);
    mutt_autocrypt_db_peer_free(&peer);
  }

  mutt_addrlist_clear(&recips);
  mutt_autocrypt_db_account_free(&account);
  mutt_autocrypt_db_peer_free(&peer);
  return rc;
}

/**
 * mutt_autocrypt_scan_mailboxes - Scan mailboxes for Autocrypt headers
 *
 * This is invoked during the first autocrypt initialization,
 * to scan one or more mailboxes for autocrypt headers.
 *
 * Due to the implementation, header-cached headers are not scanned,
 * so this routine just opens up the mailboxes with $header_cache
 * temporarily disabled.
 */
void mutt_autocrypt_scan_mailboxes(void)
{
#ifdef USE_HCACHE
  char *old_hdrcache = C_HeaderCache;
  C_HeaderCache = NULL;
#endif

  struct Buffer *folderbuf = mutt_buffer_pool_get();

  /* L10N: The first time autocrypt is enabled, NeoMutt will ask to scan
     through one or more mailboxes for Autocrypt: headers.  Those headers are
     then captured in the database as peer records and used for encryption.
     If this is answered yes, they will be prompted for a mailbox.  */
  enum QuadOption scan =
      mutt_yesorno(_("Scan a mailbox for autocrypt headers?"), MUTT_YES);
  while (scan == MUTT_YES)
  {
    // L10N: The prompt for a mailbox to scan for Autocrypt: headers
    if ((!mutt_buffer_enter_fname(_("Scan mailbox"), folderbuf, true)) &&
        (!mutt_buffer_is_empty(folderbuf)))
    {
      mutt_buffer_expand_path_regex(folderbuf, false);
      struct Mailbox *m = mx_path_resolve(mutt_b2s(folderbuf));
      /* NOTE: I am purposely *not* executing folder hooks here,
       * as they can do all sorts of things like push into the getch() buffer.
       * Authentication should be in account-hooks. */
      struct Context *ctx = mx_mbox_open(m, MUTT_READONLY);
      mx_mbox_close(&ctx);
      mutt_buffer_reset(folderbuf);
    }

    /* L10N: This is the second prompt to see if the user would like
       to scan more than one mailbox for Autocrypt headers.
       I'm purposely being extra verbose; asking first then prompting
       for a mailbox.  This is because this is a one-time operation
       and I don't want them to accidentally ctrl-g and abort it.  */
    scan = mutt_yesorno(_("Scan another mailbox for autocrypt headers?"), MUTT_YES);
  }

#ifdef USE_HCACHE
  C_HeaderCache = old_hdrcache;
#endif
  mutt_buffer_pool_release(&folderbuf);
}
