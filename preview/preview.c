#include "config.h"
#include <string.h>
#include "private.h"
#include "mutt/logging.h"
#include "mutt/notify_type.h"
#include "mutt/queue.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/mutt_window.h"
#include "lib.h"
#include "mutt_globals.h"

static struct MuttWindow *find_index_container(struct MuttWindow *root)
{
  bool has_sidebar = false;

  {
    struct MuttWindow *win = NULL;
    TAILQ_FOREACH(win, &root->children, entries)
    {
      if (win->type == WT_SIDEBAR)
      {
        has_sidebar = true;
        break;
      }
    }
  }

  struct MuttWindow *container;
  if (has_sidebar)
  {
    struct MuttWindow *a = TAILQ_FIRST(&root->children);
    struct MuttWindow *b = TAILQ_NEXT(a, entries);
    container = a->type == WT_SIDEBAR /* if true, sidebar is on the right */ ? b : a;
  }
  else
  {
    container = root;
  }

  return TAILQ_FIRST(&container->children); // Index container is the first
}

static int preview_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  return -1;
}

static int preview_repaint(struct MuttWindow *win)
{
  preview_draw(win);
  return -1;
}

void preview_win_init(struct MuttWindow *dlg)
{
  dlg->orient = MUTT_WIN_ORIENT_HORIZONTAL;

  struct MuttWindow *index_container = find_index_container(dlg);
  struct MuttWindow *bar = TAILQ_LAST(&index_container->children, MuttWindowList);
  mutt_window_remove_child(index_container, bar);

  const short c_preview_height = cs_subset_number(NeoMutt->sub, PREVIEW_CONFIG_PREFIX "height");
  const bool c_preview_enabled = cs_subset_bool(NeoMutt->sub, PREVIEW_CONFIG_PREFIX "enabled");
  struct MuttWindow *preview_window =
      mutt_window_new(WT_PREVIEW, MUTT_WIN_ORIENT_HORIZONTAL, MUTT_WIN_SIZE_FIXED,
                      MUTT_WIN_SIZE_UNLIMITED, c_preview_height);
  {
    preview_window->state.visible = c_preview_enabled && c_preview_height > 0;
    preview_window->wdata = preview_wdata_new();
    preview_window->wdata_free = preview_wdata_free;
    preview_window->recalc = preview_recalc;
    preview_window->repaint = preview_repaint;
  }

  mutt_window_add_child(index_container, preview_window);
  mutt_window_add_child(index_container, bar);

  { // notification registering
    notify_observer_add(NeoMutt->notify, NT_WINDOW, preview_neomutt_observer, preview_window);
    notify_observer_add(dlg->notify, NT_USER_INDEX, preview_dialog_observer, preview_window);
    notify_observer_add(NeoMutt->notify, NT_CONFIG, preview_config_observer, preview_window);
    notify_observer_add(NeoMutt->notify, NT_COLOR, preview_color_observer, preview_window);
  }
}

void preview_win_shutdown(struct MuttWindow *dlg)
{
  struct MuttWindow *preview_window = mutt_window_find(dlg, WT_PREVIEW);

  notify_observer_remove(NeoMutt->notify, preview_color_observer, preview_window);
  notify_observer_remove(NeoMutt->notify, preview_config_observer, preview_window);
  notify_observer_remove(dlg->notify, preview_dialog_observer, preview_window);
  notify_observer_remove(NeoMutt->notify, preview_neomutt_observer, preview_window);
}

void preview_init(void)
{
  notify_observer_add(NeoMutt->notify, NT_WINDOW, preview_insertion_observer, NULL);
}

void preview_shutdown(void)
{
  if (NeoMutt)
    notify_observer_remove(NeoMutt->notify, preview_insertion_observer, NULL);
}
