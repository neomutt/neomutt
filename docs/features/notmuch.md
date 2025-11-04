# Notmuch

_Email search engine_

## Support

**Since:** NeoMutt 2016-03-17

**Dependencies:** Notmuch library

## Introduction

Notmuch is a mail indexer that provides fast search and tagging of email messages. NeoMutt can integrate with Notmuch to provide powerful search capabilities.

The Notmuch feature allows NeoMutt to use Notmuch as a virtual mailbox provider. This means you can search your email using Notmuch's query language and view the results in NeoMutt.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `nm_default_uri` | string | (empty) |
| `nm_record` | boolean | `no` |
| `nm_record_tags` | string | (empty) |
| `nm_unread_tag` | string | (empty) |
| `nm_replied_tag` | string | (empty) |
| `nm_flagged_tag` | string | (empty) |
| `nm_delayed_tag` | string | (empty) |
| `nm_deleted_tag` | string | (empty) |
| `nm_hidden_tag` | string | (empty) |
| `nm_exclude_tags` | string | (empty) |
| `nm_open_timeout` | number | `5` |
| `nm_db_limit` | number | `0` |

## Commands

* `notmuch` - Open a Notmuch virtual mailbox

## neomuttrc

```
# Example NeoMutt config file for the notmuch feature.

# Notmuch is a mail indexer that provides fast search and tagging of email messages.

# Configure Notmuch database
set nm_default_uri = "notmuch:///path/to/maildir"
# Set tags for various message states
set nm_unread_tag = "unread"
set nm_replied_tag = "replied"
set nm_flagged_tag = "flagged"

# vim: syntax=neomuttrc
```

## See Also

* [Notmuch](/guide/optionalfeatures.html#notmuch)

## Known Bugs

None

## Credits

David Bremner, Austin Ray, Richard Russon
