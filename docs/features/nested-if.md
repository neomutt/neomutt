# Nested If

_Allow complex nested conditions in format strings_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "nested-if" feature allows you to use nested conditional expressions in format strings. This gives you more control over the display of information.

The conditional expression syntax is:

```
%<condition?true-string&false-string>
```

The condition can be another conditional expression, allowing nesting.

## Variables

This feature has no config of its own. It adds functionality to format strings used in variables like `$index_format`.

## neomuttrc

```
# Example NeoMutt config file for the nested-if feature.

# The 'nested-if' feature allows you to use nested conditional expressions in format strings.

# This gives you more control over the display of information.

# The conditional expression syntax is:
#   %<condition?true-string&false-string>
# The condition can be another conditional expression, allowing nesting.

# Simple conditional
# %<l?%4l&%4c>  # if lines > 0, show lines, else show chars

# Nested conditional
# %<l?%<L?%4L&%4l>&%4c>  # if lines > 0, if Lines > 0, show Lines, else show lines, else show chars

# vim: syntax=neomuttrc
```

## See Also

* [`$index_format`](/guide/reference.html#index-format)
* [`$status_format`](/guide/reference.html#status-format)
* [Format Strings](/guide/reference.html#formatstrings)

## Known Bugs

None

## Credits

Richard Russon
