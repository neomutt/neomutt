/**
 * @file
 * Notification API
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page notify Notification API
 *
 * Notification API
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "notify.h"
#include "memory.h"
#include "queue.h"

/**
 * struct Notify - Notification API
 */
struct Notify
{
  void *obj;
  enum NotifyType obj_type;
  struct Notify *parent;
  struct ObserverList observers;
};

/**
 * notify_new - Create a new notifications handler
 * @param object Owner of the object
 * @param type   Object type, e.g. #NT_ACCOUNT
 * @retval ptr New notification handler
 */
struct Notify *notify_new(void *object, enum NotifyType type)
{
  struct Notify *notify = mutt_mem_calloc(1, sizeof(*notify));

  notify->obj = object;
  notify->obj_type = type;
  STAILQ_INIT(&notify->observers);

  return notify;
}

/**
 * notify_free - Free a notification handler
 * @param ptr Notification handler to free
 */
void notify_free(struct Notify **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Notify *notify = *ptr;
  // NOTIFY observers

  notify_observer_remove(notify, NULL, 0);

  FREE(ptr);
}

/**
 * notify_set_parent - Set the parent notification handler
 * @param notify Notification handler to alter
 * @param parent Parent notification handler
 *
 * Notifications are passed up the tree of handlers.
 */
void notify_set_parent(struct Notify *notify, struct Notify *parent)
{
  if (!notify || !parent)
    return;

  notify->parent = parent;
}

/**
 * send - Send out a notification message
 * @param source  Source of the event, e.g. #Account
 * @param current Current handler, e.g. #NeoMutt
 * @param type    Type of event, e.g. #NT_ACCOUNT
 * @param subtype Subtype, e.g. NT_ACCOUNT_NEW
 * @param data    Private data associated with the event type
 * @retval true If successfully sent
 *
 * Notifications are sent to all matching observers, then propagated up the
 * handler tree.  For example a "new email" notification would be sent to the
 * Mailbox that owned it, the Account (owning the Mailbox) and finally the
 * NeoMutt object.
 */
static bool send(struct Notify *source, struct Notify *current, int type,
                 int subtype, intptr_t data)
{
  if (!source || !current)
    return false;

  // mutt_debug(LL_NOTIFY, "send: %d, %ld\n", type, data);
  struct ObserverNode *np = NULL;
  struct ObserverNode *tmp = NULL;
  // We use the `_SAFE` version in case an event causes an observer to be deleted
  STAILQ_FOREACH_SAFE(np, &current->observers, entries, tmp)
  {
    struct Observer *o = np->observer;

    struct NotifyCallback nc = { source->obj, source->obj_type, type,   subtype,
                                 data,        o->flags,         o->data };
    o->callback(&nc);
  }

  if (current->parent)
    return send(source, current->parent, type, subtype, data);
  return true;
}

/**
 * notify_send - Send out a notification message
 * @param notify  Notification handler
 * @param type    Type of event, e.g. #NT_ACCOUNT
 * @param subtype Subtype, e.g. NT_ACCOUNT_NEW
 * @param data    Private data associated with the event type
 * @retval true If successfully sent
 *
 * See send() for more details.
 */
bool notify_send(struct Notify *notify, int type, int subtype, intptr_t data)
{
  return send(notify, notify, type, subtype, data);
}

/**
 * notify_observer_add - Add an observer to an object
 * @param notify   Notification handler
 * @param type     Type of event to listen for, e.g. #NT_ACCOUNT
 * @param subtype  Subtype, e.g. NT_ACCOUNT_NEW
 * @param callback Function to call on a matching event, see ::observer_t
 * @param data     Private data associated with the event type
 * @retval true If successful
 *
 * New observers are added to the front of the list, giving them higher
 * priority than existing observers.
 */
bool notify_observer_add(struct Notify *notify, enum NotifyType type,
                         int subtype, observer_t callback, intptr_t data)
{
  if (!notify || !callback)
    return false;

  struct ObserverNode *np = NULL;
  STAILQ_FOREACH(np, &notify->observers, entries)
  {
    if (np->observer->callback == callback)
      return true;
  }

  struct Observer *o = mutt_mem_calloc(1, sizeof(*o));
  o->type = type;
  o->flags = subtype;
  o->callback = callback;
  o->data = data;

  np = mutt_mem_calloc(1, sizeof(*np));
  np->observer = o;
  STAILQ_INSERT_HEAD(&notify->observers, np, entries);

  return true;
}

/**
 * notify_observer_remove - Remove an observer from an object
 * @param notify   Notification handler
 * @param callback Function to call on a matching event, see ::observer_t
 * @param data     Private data to match specific callback
 * @retval true If successful
 *
 * If callback is NULL, all the observers will be removed.
 */
bool notify_observer_remove(struct Notify *notify, observer_t callback, intptr_t data)
{
  if (!notify)
    return false;

  bool result = false;
  struct ObserverNode *np = NULL;
  struct ObserverNode *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &notify->observers, entries, tmp)
  {
    if (!callback || ((np->observer->callback == callback) && (np->observer->data == data)))
    {
      STAILQ_REMOVE(&notify->observers, np, ObserverNode, entries);
      FREE(&np->observer);
      FREE(&np);
      result = true;
      if (callback)
        break;
    }
  }

  return result;
}
