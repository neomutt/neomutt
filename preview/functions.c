#include "mutt/buffer.h"
#include "mutt/file.h"
#include "mutt/memory.h"
#include "handler.h"
#include "mutt_parse.h"
#include "mx.h"
#include "pager/lib.h"
#include "preview/private.h"
#include "recvattach.h"
#include "state.h"

#include "email/attach.h"
#include "email/lib.h"

static struct Body *find_first_decodable_body(struct AttachCtx *actx)
{
  for (int i = 0; i < actx->idxlen; i++)
  {
    struct Body *body = actx->idx[i]->body;
    if (mutt_can_decode(body))
    {
      return body;
    }
  }

  mutt_debug(LL_DEBUG1, "preview: did not find a decodable body");
  return NULL;
}

static void filter_and_add(struct Buffer *buffer, char *source)
{
  while (*source)
  {
    char sc = *source;
    // XXX: how to handle locale here? Do we have a global conf one?
    // Also no idea how non roman alphabet should be handled…
    if (isspace(sc))
    {
      mutt_buffer_addch(buffer, ' ');
      ++source;
      while (isspace(*source))
        ++source;
      continue;
    }

    int idx = 0;
    while (source[idx] != '\0' && !isspace(source[idx]))
    {
      ++idx;
    }

    bool add_space = false;
    if (source[idx] != '\0')
    {
      source[idx] = '\0';
      add_space = true;
    }

    mutt_buffer_strip_formatting(buffer, source, false, true);
    if (add_space)
    {
      source[idx] = ' ';
    }

    source += idx;
  }
}

void compute_mail_preview(struct PreviewWindowData *data)
{
  struct Mailbox *m = data->mailbox;
  struct Email *e = data->current_email;

  if (!e)
  {
    mutt_debug(LL_DEBUG1, "preview: no mail selected");
    return;
  }

  mutt_parse_mime_message(m, e);
  struct Message *msg = mx_msg_open(m, e->msgno);

  mutt_buffer_reset(&data->buffer);

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

  char *line = NULL;
  struct Body *body = find_first_decodable_body(actx);
  if (!body)
  {
    goto cleanup;
  }

  mutt_body_handler(body, &s);

  rewind(s.fp_out);

  size_t sz_line = 1024;
  line = mutt_mem_malloc(sz_line);
  for (int i = 0; i < C_PreviewLines; ++i)
  {
    line = mutt_file_read_line(line, &sz_line, s.fp_out, NULL, 0);
    if (!line)
      break;

    filter_and_add(&data->buffer, line);
  }

cleanup:
  if (line)
  {
    FREE(&line);
  }
  mx_msg_close(m, &msg);
  fclose(s.fp_out);
  mutt_actx_free(&actx);
}
