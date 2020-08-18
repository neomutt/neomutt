#include "config.h"
#include <string.h>
#include "private.h"
#include "mutt/logging.h"
#include "mutt/queue.h"
#include "core/neomutt.h"
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

static void debug_window_tree(struct MuttWindow *node, int indent)
{
  char *s_indent = malloc(4 * indent + 1);
  memset(s_indent, ' ', 4 * indent);
  s_indent[4 * indent] = 0;
  {
    struct MuttWindow *win;
    TAILQ_FOREACH(win, &node->children, entries)
    {
      mutt_message("%sWindow: %d\n", s_indent, win->type);
      debug_window_tree(win, indent + 1);
    }
  }
  free(s_indent);
}

static int preview_recalc(struct MuttWindow *win)
{
  mutt_debug(LL_DEBUG1, "PREVIEW RECALC\n");
  win->actions |= WA_REPAINT;
  return -1;
}

static int preview_repaint(struct MuttWindow *win)
{
  mutt_debug(LL_DEBUG1, "PREVIEW REPAINT\n");
  preview_draw(win);
  return -1;
}

void preview_win_init(struct MuttWindow *dlg)
{
  dlg->orient = MUTT_WIN_ORIENT_HORIZONTAL;

  struct MuttWindow *index_container = find_index_container(dlg);
  struct MuttWindow *bar = TAILQ_LAST(&index_container->children, MuttWindowList);
  mutt_window_remove_child(index_container, bar);

  struct MuttWindow *preview_window =
      mutt_window_new(WT_PREVIEW, MUTT_WIN_ORIENT_HORIZONTAL, MUTT_WIN_SIZE_FIXED,
                      MUTT_WIN_SIZE_UNLIMITED, C_PreviewHeight);
  {
    preview_window->state.visible = C_PreviewEnabled && C_PreviewHeight > 0;
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
  }

  debug_window_tree(dlg, 0);
}

void preview_win_shutdown(struct MuttWindow *dlg)
{
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
