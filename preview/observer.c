#include "private.h"

#include "mutt/notify_type.h"
#include "gui/mutt_window.h"

int preview_neomutt_observer(struct NotifyCallback *nc)
{
  struct MuttWindow *win = nc->global_data;
  win->actions |= WA_RECALC;
  return 0;
}

int preview_dialog_observer(struct NotifyCallback *nc)
{
  struct MuttWindow *win = nc->global_data;
  win->actions |= WA_RECALC;

  if (nc->event_type == NT_USER_INDEX)
  {
    mutt_debug(LL_DEBUG1, "preview: receive a NT_USER_INDEX event");
  }

  return 0;
}

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
