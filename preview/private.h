#pragma once

struct NotifyCallback;
struct MuttWindow;

/* observers.c */

int preview_insertion_observer  (struct NotifyCallback *nc);
int preview_neomutt_observer    (struct NotifyCallback *nc);
int preview_dialog_observer     (struct NotifyCallback *nc);

/* preview.c */

void preview_win_init       (struct MuttWindow *dlg);
void preview_win_shutdown   (struct MuttWindow *dlg);

/* draw.c */
void preview_draw(struct MuttWindow* win);

/* wdata.c */

struct PreviewWindowData
{
  struct Email *current_email;
};

struct PreviewWindowData    *preview_wdata_new(void);
struct PreviewWindowData    *preview_wdata_get(struct MuttWindow *win);
void                        preview_wdata_free(struct MuttWindow *win, void **ptr);
