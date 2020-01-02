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
#include "notify.h"
#include "memory.h"
#include "queue.h"

/**
 * notify_new - Create a new notifications handler
 * @retval ptr New notification handler
 */
struct Notify *notify_new(void)
{
  struct Notify *notify = mutt_mem_calloc(1, sizeof(*notify));

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

  notify_observer_remove(notify, NULL, NULL);

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
 * @param source        Source of the event, e.g. #Account
 * @param current       Current handler, e.g. #NeoMutt
 * @param event_type    Type of event, e.g. #NT_ACCOUNT
 * @param event_subtype Subtype, e.g. #NT_ACCOUNT_ADD
 * @param event_data    Private data associated with the event type
 * @retval true If successfully sent
 *
 * Notifications are sent to all observers of the object, then propagated up
 * the handler tree.  For example a "new email" notification would be sent to
 * the Mailbox that owns it, the Account (owning the Mailbox) and finally the
 * NeoMutt object.
 */
static bool send(struct Notify *source, struct Notify *current,
                 enum NotifyType event_type, int event_subtype, void *event_data)
{
  if (!source || !current)
    return false;

  // mutt_debug(LL_NOTIFY, "send: %d, %ld\n", event_type, event_data);
  struct ObserverNode *np = NULL;
  struct ObserverNode *tmp = NULL;
  // We use the `_SAFE` version in case an event causes an observer to be deleted
  STAILQ_FOREACH_SAFE(np, &current->observers, entries, tmp)
  {
    struct Observer *o = np->observer;

    struct NotifyCallback nc = { event_type, event_subtype, event_data, o->global_data };
    o->callback(&nc);
  }

  if (current->parent)
    return send(source, current->parent, event_type, event_subtype, event_data);
  return true;
}

/**
 * notify_send - Send out a notification message
 * @param notify        Notification handler
 * @param event_type    Type of event, e.g. #NT_ACCOUNT
 * @param event_subtype Subtype, e.g. #NT_ACCOUNT_ADD
 * @param event_data    Private data associated with the event
 * @retval true If successfully sent
 *
 * See send() for more details.
 */
bool notify_send(struct Notify *notify, enum NotifyType event_type,
                 int event_subtype, void *event_data)
{
  return send(notify, notify, event_type, event_subtype, event_data);
}

/**
 * notify_observer_add - Add an observer to an object
 * @param notify      Notification handler
 * @param callback    Function to call on a matching event, see ::observer_t
 * @param global_data Private data associated with the observer
 * @retval true If successful
 *
 * New observers are added to the front of the list, giving them higher
 * priority than existing observers.
 */
bool notify_observer_add(struct Notify *notify, observer_t callback, void *global_data)
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
  o->callback = callback;
  o->global_data = global_data;

  np = mutt_mem_calloc(1, sizeof(*np));
  np->observer = o;
  STAILQ_INSERT_HEAD(&notify->observers, np, entries);

  return true;
}

/**
 * notify_observer_remove - Remove an observer from an object
 * @param notify      Notification handler
 * @param callback    Function to call on a matching event, see ::observer_t
 * @param global_data Private data to match specific callback
 * @retval true If successful
 *
 * If callback is NULL, all the observers will be removed.
 */
bool notify_observer_remove(struct Notify *notify, observer_t callback, void *global_data)
{
  if (!notify)
    return false;

  bool result = false;
  struct ObserverNode *np = NULL;
  struct ObserverNode *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &notify->observers, entries, tmp)
  {
    if (!callback || ((np->observer->callback == callback) &&
                      (np->observer->global_data == global_data)))
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
