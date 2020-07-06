#include "lib.h"

#include "core/neomutt.h"
#include "gui/mutt_window.h"
#include "mutt_globals.h"

#include "private.h"
#include "mutt/logging.h"
#include "mutt/queue.h"

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

void preview_win_init(struct MuttWindow *dlg)
{
  dlg->orient = MUTT_WIN_ORIENT_HORIZONTAL;
  struct MuttWindow *index_panel = find_index_container(dlg);
  (void) index_panel;
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
