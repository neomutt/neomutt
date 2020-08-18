#pragma once

#include <stdbool.h>

#include "mutt/buffer.h"

struct NotifyCallback;
struct MuttWindow;
struct Mailbox;

/* observers.c */

int preview_insertion_observer  (struct NotifyCallback *nc);
int preview_neomutt_observer    (struct NotifyCallback *nc);
int preview_dialog_observer     (struct NotifyCallback *nc);

/* preview.c */

void preview_win_init       (struct MuttWindow *dlg);
void preview_win_shutdown   (struct MuttWindow *dlg);

/* draw.c */
void preview_draw(struct MuttWindow* win);

/*  config.c */
extern bool *C_PreviewEnabled;
extern short C_PreviewHeight;
extern short C_PreviewLines;
extern char* C_PreviewDividerCharH;

/* wdata.c */
struct PreviewWindowData
{
  struct Mailbox *mailbox;
  struct Email *current_email;
  struct Buffer buffer;
};

struct PreviewWindowData    *preview_wdata_new(void);
struct PreviewWindowData    *preview_wdata_get(struct MuttWindow *win);
void                        preview_wdata_free(struct MuttWindow *win, void **ptr);

/*  functions.c */
void compute_mail_preview(struct PreviewWindowData *data);
