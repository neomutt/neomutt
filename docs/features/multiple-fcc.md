# Multiple FCC

_Save multiple copies of outgoing mail_

## Support

**Since:** NeoMutt 2016-08-08

**Dependencies:** None

## Introduction

The "multiple-fcc" feature allows you to save multiple copies of outgoing emails. This is useful if you want to archive your sent mail in different places or in different formats.

## Variables

The `$fcc` variable can be a comma-separated list of folders.

## neomuttrc

```
# Example NeoMutt config file for the multiple-fcc feature.

# The 'multiple-fcc' feature allows you to save multiple copies of outgoing emails.

# This is useful if you want to archive your sent mail in different places or in different formats.

# Save sent mail to multiple folders
set fcc = "~/Mail/sent,~/Mail/archive"

# vim: syntax=neomuttrc
```

## See Also

* [`$fcc`](/guide/reference.html#fcc)
* [Record](/guide/configuration.html#record)
* [Postpone](/guide/configuration.html#postpone)

## Known Bugs

None

## Credits

Richard Russon
