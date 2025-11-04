# NNTP

_Talk to a Usenet news server_

## Support

**Since:** NeoMutt 2016-05-30

**Dependencies:** None

## Introduction

The "nntp" feature allows NeoMutt to read and post to Usenet newsgroups using the Network News Transfer Protocol (NNTP).

NNTP is the protocol used by Usenet news servers to distribute news articles. NeoMutt can connect to an NNTP server and browse newsgroups, read articles, and post new articles.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `nntp_user` | string | (empty) |
| `nntp_pass` | string | (empty) |
| `nntp_listgroup` | boolean | `yes` |
| `nntp_data` | string | (empty) |
| `nntp_context` | number | `1000` |
| `nntp_poll` | number | `60` |
| `nntp_reconnect` | quadoption | `ask` |
| `nntp_authenticators` | string | (empty) |

## neomuttrc

```
# Example NeoMutt config file for the nntp feature.

# The 'nntp' feature allows NeoMutt to read and post to Usenet newsgroups.

# Connect to an NNTP server
set folder = "news://news.example.com"
# Set username and password if required
set nntp_user = "username"
set nntp_pass = "password"

# vim: syntax=neomuttrc
```

## See Also

* [NNTP](/guide/optionalfeatures.html#nntp)

## Known Bugs

None

## Credits

Andrey Butov, Richard Russon
