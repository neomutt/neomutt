# Sidebar

_Overview of mailboxes_

## Support

**Since:** NeoMutt 2016-09-10, NeoMutt 1.7.0

**Dependencies:** None

## Introduction

The Sidebar shows a list of all your mailboxes. The list can be turned on and off, it can be themed and the list style can be configured.

This part of the manual is a reference guide. If you want a simple introduction with examples see the [Sidebar Howto](/guide/gettingstarted.html#intro-sidebar). If you just want to get started, you could use the sample [Sidebar neomuttrc](#neomuttrc).

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `sidebar_component_depth` | number | `0` |
| `sidebar_delim_chars` | string | `/.` |
| `sidebar_divider_char` | string | `\|` |
| `sidebar_folder_indent` | boolean | `no` |
| `sidebar_format` | string | `%D%\* %n` |
| `sidebar_indent_string` | string | `(two spaces)` |
| `sidebar_new_mail_only` | boolean | `no` |
| `sidebar_next_new_wrap` | boolean | `no` |
| `sidebar_non_empty_mailbox_only` | boolean | `no` |
| `sidebar_on_right` | boolean | `no` |
| `sidebar_short_path` | boolean | `no` |
| `sidebar_sort` | enum | `unsorted` |
| `sidebar_visible` | boolean | `no` |
| `sidebar_width` | number | `20` |

For more details, and examples, about the `$sidebar_format`, see the [Sidebar Intro](/guide/gettingstarted.html#intro-sidebar-format).

## Functions

Sidebar adds the following functions to NeoMutt. By default, none of them are bound to keys.

| Menus | Function | Description |
| ----- | -------- | ----------- |
| index,pager | `<sidebar-next>` | Move the highlight to next mailbox |
| index,pager | `<sidebar-next-new>` | Move the highlight to next mailbox with new mail |
| index,pager | `<sidebar-open>` | Open highlighted mailbox |
| index,pager | `<sidebar-page-down>` | Scroll the Sidebar down 1 page |
| index,pager | `<sidebar-page-up>` | Scroll the Sidebar up 1 page |
| index,pager | `<sidebar-prev>` | Move the highlight to previous mailbox |
| index,pager | `<sidebar-prev-new>` | Move the highlight to previous mailbox with new mail |
| index,pager | `<sidebar-toggle-visible>` | Make the Sidebar (in)visible |

## Commands

* `sidebar_pin` _`mailbox`_ [ _`mailbox`_ ...]
* `sidebar_unpin` { _`*`_ | _`mailbox`_ ... }

This command specifies mailboxes that will always be displayed in the sidebar, even if `$sidebar_new_mail_only` is set and the mailbox does not contain new mail.

The "sidebar_unpin" command is used to remove a mailbox from the list of always displayed mailboxes. Use "sidebar_unpin \*" to remove all mailboxes.

## Colors

| Name | Default Color | Description |
| ---- | ------------- | ----------- |
| `sidebar_background` | default | The entire sidebar panel |
| `sidebar_divider` | default | The dividing line between the Sidebar and the Index/Pager panels |
| `sidebar_flagged` | default | Mailboxes containing flagged mail |
| `sidebar_highlight` | underline | Cursor to select a mailbox |
| `sidebar_indicator` | neomuttindicator | The mailbox open in the Index panel |
| `sidebar_new` | default | Mailboxes containing new mail |
| `sidebar_ordinary` | default | Mailboxes that have no new/flagged mails, etc. |
| `sidebar_spool_file` | default | Mailbox that receives incoming mail |
| `sidebar_unread` | default | Mailboxes containing unread mail |

If the `sidebar_indicator` color isn't set, then the default NeoMutt indicator color will be used (the color used in the index panel).

## Sort

| Sort | Description |
| ---- | ----------- |
| alpha | Alphabetically by path or label |
| count | Total number of messages |
| desc | Descriptive name of the mailbox |
| flagged | Number of flagged messages |
| name | Alphabetically by path or label |
| new | Number of unread messages |
| path | Alphabetically by path (ignores label) |
| unread | Number of unread messages |
| unsorted | Order of the mailboxes command |

## neomuttrc

```
# Example NeoMutt config file for the sidebar feature.

# --------------------------------------------------------------------------
# VARIABLES – shown with their default values
# --------------------------------------------------------------------------
# Should the Sidebar be shown?
set sidebar_visible = no
# How wide should the Sidebar be in screen columns?

# Note: Some characters, e.g. Chinese, take up two columns each.
set sidebar_width = 20
# Should the mailbox paths be abbreviated?
set sidebar_short_path = no
# Number of top-level mailbox path subdirectories to truncate for display
set sidebar_component_depth = 0
# When abbreviating mailbox path names, use any of these characters as path
# separators. Only the part after the last separators will be shown.
# For file folders '/' is good. For IMAP folders, often '.' is useful.
set sidebar_delim_chars = '/.'
# If the mailbox path is abbreviated, should it be indented?
set sidebar_folder_indent = no
# Indent mailbox paths with this string.
set sidebar_indent_string = '  '
# Make the Sidebar only display mailboxes that contain new, or flagged,
# mail.
set sidebar_new_mail_only = no
# Any mailboxes that are pinned will always be visible, even if the
# sidebar_new_mail_only option is enabled.
set sidebar_non_empty_mailbox_only = no
# Only show mailboxes that contain some mail
sidebar_pin '/home/user/mailbox1'
sidebar_pin '/home/user/mailbox2'
# When searching for mailboxes containing new mail, should the search wrap
# around when it reaches the end of the list?
set sidebar_next_new_wrap = no
# Show the Sidebar on the right-hand side of the screen
set sidebar_on_right = no
# The character to use as the divider between the Sidebar and the other NeoMutt
# panels.
set sidebar_divider_char = '|'
# Enable extended mailbox mode to calculate total, new, and flagged
# message counts for each mailbox.
set mail_check_stats
# Display the Sidebar mailboxes using this format string.
set sidebar_format = '%B%<F? [%F]>%* %<N?%N/>%S'
# Sort the mailboxes in the Sidebar using this method:
#       count    – total number of messages
#       flagged  – number of flagged messages
#       unread   – number of unread messages
#       path     – mailbox path
#       unsorted – do not sort the mailboxes
set sidebar_sort = 'unsorted'
# --------------------------------------------------------------------------
# FUNCTIONS – shown with an example mapping
# --------------------------------------------------------------------------
# Move the highlight to the previous mailbox
bind index,pager \Cp sidebar-prev
# Move the highlight to the next mailbox
bind index,pager \Cn sidebar-next
# Open the highlighted mailbox
bind index,pager \Co sidebar-open
# Move the highlight to the previous page
# This is useful if you have a LOT of mailboxes.
bind index,pager <F3> sidebar-page-up
# Move the highlight to the next page
# This is useful if you have a LOT of mailboxes.
bind index,pager <F4> sidebar-page-down
# Move the highlight to the previous mailbox containing new, or flagged,
# mail.
bind index,pager <F5> sidebar-prev-new
# Move the highlight to the next mailbox containing new, or flagged, mail.
bind index,pager <F6> sidebar-next-new
# Toggle the visibility of the Sidebar.
bind index,pager B sidebar-toggle-visible
# --------------------------------------------------------------------------
# COLORS – some unpleasant examples are given
# --------------------------------------------------------------------------
# Note: All color operations are of the form:
#       color OBJECT FOREGROUND BACKGROUND
# Color of the current, open, mailbox
# Note: This is a general NeoMutt option which colors all selected items.
color indicator cyan black
# Sidebar-specific color of the selected item
color sidebar_indicator cyan black
# Color of the highlighted, but not open, mailbox.
color sidebar_highlight black color8
# Color of the entire Sidebar panel
color sidebar_background default black
# Color of the divider separating the Sidebar from NeoMutt panels
color sidebar_divider color8 black
# Color to give mailboxes containing flagged mail
color sidebar_flagged red black
# Color to give mailboxes containing new mail
color sidebar_new green black
# Color to give mailboxes containing no new/flagged mail, etc.
color sidebar_ordinary color245 default
# Color to give the spool_file mailbox
color sidebar_spool_file color207 default
# Color to give mailboxes containing no unread mail
color sidebar_unread color136 default
# --------------------------------------------------------------------------

# vim: syntax=neomuttrc
```

## See Also

* [Regular Expressions](/guide/advancedusage.html#regex)
* [Patterns](/guide/advancedusage.html#patterns)
* [Color command](/guide/configuration.html#color)
* [notmuch feature](/feature/notmuch)

## Known Bugs

None

## Credits

Justin Hibbits, Thomer M. Gil, David Sterba, Evgeni Golov, Fabian Groffen, Jason DeTiberus, Stefan Assmann, Steve Kemp, Terry Chan, Tyler Earnest, Richard Russon
