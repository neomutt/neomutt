#include "private.h"

#include "mutt/notify_type.h"
#include "mutt/observer.h"
#include "gui/mutt_window.h"
#include "index.h"

static void handle_selection_change(struct NotifyCallback *nc)
{
  mutt_debug(LL_DEBUG1, "preview: receive a NT_USER_INDEX event");
  if (nc->event_subtype != NT_USER_EMAIL_SELECTED)
  {
    return;
  }

  struct MuttWindow *win = nc->global_data;
  struct IndexEvent *data = nc->event_data;

  struct PreviewWindowData *preview_data = preview_wdata_get(win);

  if (data->current_email == preview_data->current_email)
  {
    return;
  }

  preview_data->current_email = data->current_email;
  win->actions |= WA_RECALC;
}

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

  switch (nc->event_type)
  {
    default:
      return 0;
    case NT_USER_INDEX:
      handle_selection_change(nc);
      break;
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
