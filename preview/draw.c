#include "config.h"
#include "private.h"

#include "mutt/logging.h"
#include "mutt/queue.h"
#include "email/lib.h"
#include "gui/lib.h"

void preview_draw(struct MuttWindow *win)
{
  if (!win || !win->state.visible)
    return;

  struct PreviewWindowData *data = preview_wdata_get(win);
  if (data->current_email == NULL)
  {
    mutt_window_mvprintw(win, 5, 5, "No email selected");
    return;
  }

  struct Email *em = data->current_email;

  struct Address *from = TAILQ_FIRST(&em->env->sender);
  mutt_window_mvprintw(win, 5, 5, "Selected mail id: %s, from: %s, subject: %s",
                       em->env->message_id, from ? from->mailbox : "unknown",
                       em->env->subject);
}
