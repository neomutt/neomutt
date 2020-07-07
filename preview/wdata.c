#include "private.h"

#include "gui/mutt_window.h"

struct PreviewWindowData *preview_wdata_new(void)
{
  return calloc(1, sizeof(struct PreviewWindowData));
}

struct PreviewWindowData *preview_wdata_get(struct MuttWindow *win)
{
  if (!win || (win->type != WT_PREVIEW))
    return NULL;

  return win->wdata;
}

void preview_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct PreviewWindowData *wdata = *ptr;
  FREE(ptr);
}
