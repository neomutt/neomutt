#include "private.h"

#include "mutt/notify_type.h"
#include "mutt/observer.h"
#include "mutt/string2.h"
#include "config/subset.h"
#include "gui/color.h"
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

  if (data->mailbox == preview_data->mailbox &&
      data->current_email == preview_data->current_email)
  {
    return;
  }

  preview_data->mailbox = data->mailbox;
  preview_data->current_email = data->current_email;
  compute_mail_preview(preview_data);
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

/**
 * preview_color_observer - Color config has changed - Implements ::observer_t
 */
int preview_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  switch ((enum ColorId) nc->event_data)
  {
    default:
      return 0;
    case MT_COLOR_PREVIEW_TEXT:
    case MT_COLOR_PREVIEW_DIVIDER:
      break;
  }

  win->parent->actions |= WA_REPAINT;
  return 0;
}

/**
 * preview_config_observer - Config has changed - Implements ::observer_t
 */
int preview_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->event_data || !nc->global_data)
    return -1;

  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_str_startswith(ec->name, PREVIEW_CONFIG_PREFIX) &&
      !mutt_str_equal(ec->name, "ascii_chars"))
  {
    return 0;
  }

  mutt_debug(LL_NOTIFY, "config: %s\n", ec->name);

  struct MuttWindow *win = nc->global_data;

  if (mutt_str_equal(ec->name, PREVIEW_CONFIG_PREFIX "visible"))
  {
    window_set_visible(win, C_PreviewEnabled);
  }
  else if (mutt_str_equal(ec->name, PREVIEW_CONFIG_PREFIX "height"))
  {
    win->req_rows = C_PreviewHeight;
  }

  win->parent->actions |= WA_REFLOW;

  return 0;
}
