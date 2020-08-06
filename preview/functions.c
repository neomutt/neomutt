#include "mutt/file.h"
#include "mutt/memory.h"
#include "handler.h"
#include "mutt_parse.h"
#include "mx.h"
#include "preview/private.h"
#include "recvattach.h"
#include "state.h"

#include "email/attach.h"
#include "email/lib.h"

void compute_mail_preview(struct PreviewWindowData *data)
{
  struct Mailbox *m = data->mailbox;
  struct Email *e = data->current_email;
  mutt_parse_mime_message(m, e);
  struct Message *msg = mx_msg_open(m, e->msgno);

  if (!msg)
  {
    mutt_debug(LL_DEBUG1, "preview: could not open mail");
    return;
  }

  struct State s = { 0 };
  s.flags |= MUTT_DISPLAY | MUTT_VERIFY | MUTT_WEED | MUTT_CHARCONV;
  s.fp_in = msg->fp;
  s.fp_out = mutt_file_mkstemp();

  struct AttachCtx *actx = mutt_mem_calloc(1, sizeof(*actx));
  actx->email = e;
  actx->fp_root = msg->fp;

  mutt_generate_recvattach_list(actx, actx->email, actx->email->body,
                                actx->fp_root, -1, 0, 0);
  struct Body *body;
  for (int i = 0; i < actx->idxlen; i++)
  {
    body = actx->idx[i]->body;
    if (mutt_can_decode(body))
    {
      break;
    }
    body = NULL;
  }

  if (body == NULL)
  {
    // XXX: cleanup
    mutt_debug(LL_DEBUG1, "preview: no displayable mail's part displayble");
    return;
  }

  mutt_body_handler(body, &s);

  rewind(s.fp_out);

  size_t sz = 1024;
  char *line = mutt_mem_malloc(sz);

  while (true)
  {
    char *ln = mutt_file_read_line(line, &sz, s.fp_out, NULL, 0);
    if (!ln)
      break;
    mutt_debug(LL_DEBUG1, "preview: body is: %s\n", ln);
  }
}
