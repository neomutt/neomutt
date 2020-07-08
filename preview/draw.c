#include "config.h"
#include "private.h"
#include "mutt/logging.h"
#include "gui/lib.h"

void preview_draw(struct MuttWindow *win)
{
  if (!win || !win->state.visible)
    return;

  mutt_window_mvprintw(win, 5, 5, "Hello World");
}
