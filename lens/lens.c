#include <stddef.h>
#include <string.h>
#include "mutt/hash.h"
#include "email/envelope.h"
#include "core/neomutt.h"
#include "lib.h"
#include "queue.h"

static int mailbox_to_lens(struct Mailbox *mailbox, struct MailboxLens *lens)
{
  // TODO: Dumb shallow-ref
}

static int lens_add_email(struct LensEmailList el, struct Email *e)
{
  if (!e)
    return -1;

  struct LensEmail *en = mutt_mem_calloc(1, sizeof(*en));
  en->email = e;
  // TODO: en->mailbox
  STAILQ_INSERT_TAIL(&el, en, entries);

  return 0;
}

static int merge_mailboxes(struct Mailbox *primary, struct MailboxList ml,
                           struct LensMailbox *merged)
{
  struct MailboxNode *np = NULL;
  int alloc = 0;
  if (!primary)
    return -1;
  mailbox_to_lens(primary, merged);

  // Prep: Find the number of elements in all our hash tables
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (!np->mailbox)
      continue;

    alloc += np->mailbox->id_hash->num_elems;
  }

  // Step 1: Copy out id_hash into id_hash_aux, converting every key into a LensEmail
  // TODO

  // Step 2: Merge all the hash tables
  struct HashTable *table = mutt_hash_new(alloc, MUTT_HASH_NO_FLAGS);
  STAILQ_FOREACH(np, &ml, entries)
  {
    mutt_hash_merge(table, np->mailbox->id_hash);
  }

  // Now the real work: match in-reply-to against the msg-ids in table
  struct LensEmail *le = NULL;
  char *ref = NULL;
  struct Email *match = NULL;
  STAILQ_FOREACH(le, &merged->emails, entries)
  {
    STAILQ_FOREACH(ref, &le->email->env->in_reply_to, entries)
    {
      // TODO: This step should directly return a LensEmail
      if ((match = mutt_hash_find(table, ref)))
        lens_add_email(merged->emails, match);
    }
  }

  mutt_hash_free(&table);
  return 0;
}

struct LensMailbox *mutt_lens_mailbox(struct Mailbox *primary, struct MailboxList ml)
{
  struct LensMailbox *lens = NULL;
  mutt_buffer_allocate(lens);
  merge_mailboxes(primary, ml, lens);
  return lens;
}
