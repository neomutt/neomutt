#!/usr/bin/python3
#
# markdown2html.py — simple Markdown-to-HTML converter for use with NeoMutt
#
# NeoMutt recently learnt [how to compose `multipart/alternative`
# emails][1]. This script assumes a message has been composed using Markdown
# (with a lot of pandoc extensions enabled), and translates it to `text/html`
# for NeoMutt to tie into such a `multipart/alternative` message.
#
# [1]: https://gitlab.com/muttmua/mutt/commit/0e566a03725b4ad789aa6ac1d17cdf7bf4e7e354)
#
# Configuration:
#   neomuttrc:
#     set send_multipart_alternative=yes
#     set send_multipart_alternative_filter=/path/to/markdown2html.py
#
# Optionally, Custom CSS styles will be read from `~/.mutt/markdown2html.css`,
# if present.
#
# Requirements:
#   - python3
#   - PyPandoc (and pandoc installed, or downloaded)
#   - Pynliner
#
# Optional:
#   - Pygments, if installed, then syntax highlighting is enabled
#
# Latest version:
#   https://git.madduck.net/etc/mutt.git/blob_plain/HEAD:/.mutt/markdown2html
#
# Copyright © 2019 martin f. krafft <madduck@madduck.net>
# Released under the GPL-2+ licence, just like NeoMutt itself.
#

import pypandoc
import pynliner
import re
import os
import sys

try:
    from pygments.formatters import get_formatter_by_name
    formatter = get_formatter_by_name('html', style='default')
    DEFAULT_CSS = formatter.get_style_defs('.sourceCode')

except ImportError:
    DEFAULT_CSS = ""


DEFAULT_CSS += '''
.quote {
    padding: 0 0.5em;
    margin: 0;
    font-style: italic;
    border-left: 2px solid #ccc;
    color: #999;
    font-size: 80%;
}
.quotelead {
    font-style: italic;
    margin-bottom: -1em;
    color: #999;
    font-size: 80%;
}
.quotechar { display: none; }
.footnote-ref, .footnote-back { text-decoration: none;}
.signature {
    color: #999;
    font-family: monospace;
    white-space: pre;
    margin: 1em 0 0 0;
    font-size: 80%;
}
table, th, td {
    border-collapse: collapse;
    border: 1px solid #999;
}
th, td { padding: 0.5em; }
.header {
    background: #eee;
}
.even { background: #eee; }
'''

STYLESHEET = os.path.join(os.path.expanduser('~/.mutt'),
                          'markdown2html.css')
if os.path.exists(STYLESHEET):
    DEFAULT_CSS += open(STYLESHEET).read()

HTML_DOCUMENT = '''<!DOCTYPE html>
<html><head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes"/>
<title>HTML E-Mail</title>
</head><body class="email">
{htmlbody}
</body></html>'''


SIGNATURE_HTML = \
        '<div class="signature"><span class="leader">-- </span>{sig}</div>'


def _preprocess_markdown(mdwn):
    '''
    Preprocess Markdown for handling by the converter.
    '''
    # convert hard line breaks within paragraphs to 2 trailing spaces, which
    # is the markdown way of representing hard line breaks. Note how the
    # regexp will not match between paragraphs.
    ret = re.sub(r'(\S)\n(\s*\S)', r'\g<1>  \n\g<2>', mdwn, flags=re.MULTILINE)

    # Clients like Thunderbird need the leading '>' to be able to properly
    # create nested quotes, so we duplicate the symbol, the first instance
    # will tell pandoc to create a blockquote, while the second instance will
    # be a <span> containing the character, along with a class that causes CSS
    # to actually hide it from display. However, this does not work with the
    # text-mode HTML2text converters, and so it's left commented for now.
    #ret = re.sub(r'\n>', r'  \n>[>]{.quotechar}', ret, flags=re.MULTILINE)

    return ret


def _identify_quotes_for_later(mdwn):
    '''
    Email quoting such as:

    ```
    On 1970-01-01, you said:
    > The Flat Earth Society has members all around the globe.
    ```

    isn't really properly handled by Markdown, so let's do our best to
    identify the individual elements, and mark them, using a syntax similar to
    what pandoc uses already in some cases. As pandoc won't actually use these
    data (yet?), we call `self._reformat_quotes` later to use these markers
    to slap the appropriate classes on the HTML tags.
    '''

    def generate_lines_with_context(mdwn):
        '''
        Iterates the input string line-wise, returning a triplet of
        previous, current, and next line, the first and last of which
        will be None on the first and last line of the input data
        respectively.
        '''
        prev = cur = nxt = None
        lines = iter(mdwn.splitlines())
        cur = next(lines)
        for nxt in lines:
            yield prev, cur, nxt
            prev = cur
            cur = nxt
        yield prev, cur, None

    ret = []
    for prev, cur, nxt in generate_lines_with_context(mdwn):

        # The lead-in to a quote is a single line immediately preceding the
        # quote, and ending with ':'. Note that there could be multiple of
        # these:
        if nxt is not None and re.match(r'^\s*[^>].+:\s*$', cur) and nxt.startswith('>'):
            ret.append(f'{{.quotelead}}{cur.strip()}')
            # pandoc needs an empty line before the blockquote, so
            # we enter one for the purpose of HTML rendition:
            ret.append('')
            continue

        # The first blockquote after such a lead-in gets marked as the
        # "initial" quote:
        elif prev is not None and re.match(r'^\s*[^>].+:\s*$', prev) and cur.startswith('>'):
            ret.append(re.sub(r'^(\s*>\s*)+(.+)',
                              r'\g<1>{.quoteinitial}\g<2>',
                              cur, flags=re.MULTILINE))

        # All other occurrences of blockquotes get the "subsequent" marker:
        elif cur.startswith('>') and prev is not None and not prev.startswith('>'):
            ret.append(re.sub(r'^((?:\s*>\s*)+)(.+)',
                              r'\g<1>{.quotesubsequent}\g<2>',
                              cur, flags=re.MULTILINE))

        else: # pass through everything else.
            ret.append(cur)

    return '\n'.join(ret)


def _reformat_quotes(html):
    '''
    Earlier in the pipeline, we marked email quoting, using markers, which we
    now need to turn into HTML classes, so that we can use CSS to style them.
    '''
    ret = html.replace('{.quotelead}', '<p class="quotelead">')
    ret = re.sub(r'<blockquote>\n((?:<blockquote>\n)*)<p>(?:\{\.quote(\w+)\})',
                 r'<blockquote class="quote \g<2>">\n\g<1><p>', ret, flags=re.MULTILINE)
    return ret



def _convert_with_pandoc(mdwn, inputfmt='markdown', outputfmt='html5',
                         ext_enabled=None, ext_disabled=None,
                         standalone=True, title="HTML E-Mail"):
    '''
    Invoke pandoc to do the actual conversion of Markdown to HTML5.
    '''
    if not ext_enabled:
        ext_enabled = [ 'backtick_code_blocks',
                       'line_blocks',
                       'fancy_lists',
                       'startnum',
                       'definition_lists',
                       'example_lists',
                       'table_captions',
                       'simple_tables',
                       'multiline_tables',
                       'grid_tables',
                       'pipe_tables',
                       'all_symbols_escapable',
                       'intraword_underscores',
                       'strikeout',
                       'superscript',
                       'subscript',
                       'fenced_divs',
                       'bracketed_spans',
                       'footnotes',
                       'inline_notes',
                       'emoji',
                       'tex_math_double_backslash',
                       'autolink_bare_uris'
                      ]
    if not ext_disabled:
        ext_disabled = [ 'tex_math_single_backslash',
                         'tex_math_dollars',
                         'smart',
                         'raw_html'
                       ]

    enabled = '+'.join(ext_enabled)
    disabled = '-'.join(ext_disabled)
    inputfmt = f'{inputfmt}+{enabled}-{disabled}'

    args = []
    if standalone:
        args.append('--standalone')
    if title:
        args.append(f'--metadata=pagetitle:"{title}"')

    return pypandoc.convert_text(mdwn, format=inputfmt, to=outputfmt,
                                 extra_args=args)


def _apply_styling(html):
    '''
    Inline all styles defined and used into the individual HTML tags.
    '''
    return pynliner.Pynliner().from_string(html).with_cssString(DEFAULT_CSS).run()


def _postprocess_html(html):
    '''
    Postprocess the generated and styled HTML.
    '''
    return html


def convert_markdown_to_html(mdwn):
    '''
    Converts the input Markdown to HTML, handling separately the body, as well
    as an optional signature.
    '''
    parts = re.split(r'^-- $', mdwn, 1, flags=re.MULTILINE)
    body = parts[0]
    if len(parts) == 2:
        sig = parts[1]
    else:
        sig = None

    html=''
    if body:
        body = _preprocess_markdown(body)
        body = _identify_quotes_for_later(body)
        html = _convert_with_pandoc(body, standalone=False)
        html = _reformat_quotes(html)

    if sig:
        sig = _preprocess_markdown(sig)
        html += SIGNATURE_HTML.format(sig='<br/>'.join(sig.splitlines()))

    html = HTML_DOCUMENT.format(htmlbody=html)
    html = _apply_styling(html)
    html = _postprocess_html(html)

    return html


def main():
    '''
    Convert text on stdin to HTML, and print it to stdout, like mutt would
    expect.
    '''
    html = convert_markdown_to_html(sys.stdin.read())
    if html:
        # mutt expects the content type in the first line, so:
        print(f'text/html\n\n{html}')


if __name__ == '__main__':
    main()
