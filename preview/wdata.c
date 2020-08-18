#include "private.h"

#include "mutt/buffer.h"
#include "gui/mutt_window.h"

struct PreviewWindowData *preview_wdata_new(void)
{
  struct PreviewWindowData *data = calloc(1, sizeof(struct PreviewWindowData));
  // could probably be better;
  *data = (struct PreviewWindowData){
    .current_email = NULL,
    .mailbox = NULL,
    .buffer = mutt_buffer_make(C_PreviewLines * 1024),
  };

  return data;
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
  struct PreviewWindowData *wdata = *ptr;
  mutt_buffer_dealloc(&wdata->buffer);

  FREE(ptr);
}
