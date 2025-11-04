# Forgotten Attachment

_Alert user when (s)he forgets to attach a file to an outgoing email._

## Support

**Since:** NeoMutt 2016-09-10

**Dependencies:** None

## Introduction

The "forgotten-attachment" feature provides a new setting for NeoMutt that alerts the user if the message body contains a certain keyword but there are no attachments added. This is meant to ensure that the user does not forget to attach a file after promising to do so in the mail. The attachment keyword will not be scanned in text matched by `$quote_regex`.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `abort_noattach_regex` | regular expression | `\\<(attach\|attached\|attachments?)\\>` |
| `abort_noattach` | quadoption | `no` |

## neomuttrc

```
# Example NeoMutt config file for the forgotten-attachment feature.

# The 'forgotten-attachment' feature provides a new setting for NeoMutt that
# alerts the user if the message body contains a certain regular expression but there are
# no attachments added. This is meant to ensure that the user does not forget
# to attach a file after promising to do so in the mail.

# Ask if the user wishes to abort sending if $abort_noattach_regex is found in the
# body, but no attachments have been added
# It can be set to:
#    "yes"     : always abort
#    "ask-yes" : ask whether to abort
#    "no"      : send the mail
set abort_noattach = no
# Search for the following regular expression in the body of the email
# English: attach, attached, attachment, attachments
set abort_noattach_regex = "\\<attach(|ed|ments?)\\>"
# Nederlands:
# set abort_noattach_regex = "\\<(bijvoegen|bijgevoegd|bijlage|bijlagen)\\>"
# Deutsch:
# set abort_noattach_regex = "\\<(anhängen|angehängt|anhang|anhänge|hängt an)\\>"
# Français:
# set abort_noattach_regex = "\\<(attaché|attachés|attache|attachons|joint|jointe|joints|jointes|joins|joignons)\\>"

# vim: syntax=neomuttrc
```

## See Also

* [The Attachment Menu](/guide/mimesupport.html#attach-menu)
* [The Attachment Menu key mappings](/guide/reference.html#attachment-map)

## Known Bugs

None

## Credits

Darshit Shah, Richard Russon, Johannes Weißl, Steven Ragnarök
