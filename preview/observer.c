#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "index/lib.h"
#include "pager/lib.h"

static void handle_selection_change(struct NotifyCallback *nc)
{
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
  // struct PreviewWindowData *preview_data = preview_wdata_get(win);

  switch (nc->event_type)
  {
    default:
      return 0;
    case NT_WINDOW:
    {
      struct EventWindow *focused_window = nc->event_data;
      if (nc->event_subtype == NT_WINDOW_FOCUS &&
          focused_window->win->type == WT_PAGER && mutt_window_is_visible(win))
      {
        win->state.visible = false;
        win->parent->actions |= WA_REFLOW | WA_REPAINT;
        win->actions |= WA_REFLOW | WA_REPAINT;
        focused_window->win->parent->actions |= WA_REFLOW | WA_REPAINT;
        focused_window->win->actions |= WA_REFLOW | WA_REPAINT;
      }
      else if (nc->event_subtype == NT_WINDOW_STATE &&
               focused_window->win->type == WT_PAGER && !mutt_window_is_visible(win))
      {
        win->state.visible = !mutt_window_is_visible(focused_window->win);
        win->actions |= WA_RECALC | WA_REFLOW;
        win->parent->actions |= WA_REFLOW;
      }
      window_reflow(focused_window->win);
      break;
    }
    case NT_USER_INDEX:
      win->actions |= WA_RECALC;
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
    const bool c_preview_enabled = cs_subset_bool(NeoMutt->sub, PREVIEW_CONFIG_PREFIX "enabled");
    window_set_visible(win, c_preview_enabled);
  }
  else if (mutt_str_equal(ec->name, PREVIEW_CONFIG_PREFIX "height"))
  {
    const short c_preview_height = cs_subset_number(NeoMutt->sub, PREVIEW_CONFIG_PREFIX "height");
    win->req_rows = c_preview_height;
  }

  win->parent->actions |= WA_REFLOW;

  return 0;
}
