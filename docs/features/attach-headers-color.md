# Attach Headers Color

_Color attachment headers using regex, just like mail bodies_

## Support

**Since:** NeoMutt 2016-09-10

**Dependencies:** None

## Introduction

This feature allows specifying regexes to color attachment headers just like the mail body would. The headers are the parts colored by the `attachment` color. Coloring them is useful to highlight the results of GPGME's signature checks or simply the mimetype or size of the attachment. Only the part matched by the regex is colored.

## Usage

The `attach_headers` color should be used just like the `body` color.

```
color attach_headers foreground background pattern
```

## neomuttrc

```
# Example NeoMutt config file for the attach-headers-color feature.

# Color if the attachment is autoviewed
color   attach_headers     brightgreen     default    "Autoview"
# Color only the brackets around the headers
color   attach_headers     brightyellow    default    "^\\[--"
color   attach_headers     brightyellow    default    "--]$"
# Color the mime type and the size
color   attach_headers     green           default    "Type: [a-z]+/[a-z0-9\-]+"
color   attach_headers     green           default    "Size: [0-9\.]+[KM]"
# Color GPGME signature checks
color   attach_headers     brightgreen     default    "Good signature from.*"
color   attach_headers     brightred       default    "Bad signature from.*"
color   attach_headers     brightred       default    "BAD signature from.*"
color   attach_headers     brightred       default    "Note: This key has expired!"
color   attach_headers     brightmagenta   default    "Problem signature from.*"
color   attach_headers     brightmagenta   default    "WARNING: This key is not certified with a trusted signature!"
color   attach_headers     brightmagenta   default    "         There is no indication that the signature belongs to the owner."
color   attach_headers     brightmagenta   default    "can't handle these multiple signatures"
color   attach_headers     brightmagenta   default    "signature verification suppressed"
color   attach_headers     brightmagenta   default    "invalid node with packet of type"

# vim: syntax=neomuttrc
```

## See Also

* [Color command](/guide/configuration.html#color "11. Using Color and Mono Video Attributes")
* [Regular Expressions](/guide/advancedusage.html#regex "2. Regular Expressions")

## Known Bugs

None

## Credits

Guillaume Brogi
