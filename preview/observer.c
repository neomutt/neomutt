#include "private.h"

#include "mutt/notify_type.h"
#include "gui/mutt_window.h"

/**
 * preview_insertion_observer - Listen for new Dialogs - Implements ::observer_t
 */
int preview_insertion_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || (nc->event_subtype != NT_WINDOW_DIALOG))
    return 0;

  struct EventWindow *ew = nc->event_data;
  if (ew->win->type != WT_DLG_INDEX)
    return 0;

  if (ew->flags & WN_VISIBLE)
    preview_win_init(ew->win);
  else if (ew->flags & WN_HIDDEN)
    preview_win_shutdown(ew->win);

  return 0;
}
