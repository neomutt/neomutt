# This is the NeoMutt Project

[![Stars](https://img.shields.io/github/stars/neomutt/neomutt.svg?style=social&label=Stars)](https://github.com/neomutt/neomutt "Give us a Star")
[![Twitter](https://img.shields.io/twitter/follow/NeoMutt_Org.svg?style=social&label=Follow)](https://twitter.com/NeoMutt_Org "Follow us on Twitter")
[![Contributors](https://img.shields.io/badge/Contributors-186-orange.svg)](https://github.com/neomutt/neomutt/blob/master/AUTHORS.md "All of NeoMutt's Contributors")
[![Release](https://img.shields.io/github/release/neomutt/neomutt.svg)](https://github.com/neomutt/neomutt/releases/latest "Latest Release Notes")
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://github.com/neomutt/neomutt/blob/master/LICENCE.md "Copyright Statement")
[![Code build](https://img.shields.io/travis/com/neomutt/neomutt/master?label=code)](https://travis-ci.com/neomutt/neomutt "Latest Automatic Code Build")
[![Coverity Scan](https://img.shields.io/coverity/scan/8495.svg)](https://scan.coverity.com/projects/neomutt-neomutt "Latest Code Static Analysis")
[![Website build](https://img.shields.io/travis/neomutt/neomutt.github.io.svg?label=website)](https://travis-ci.com/neomutt/neomutt.github.io "Latest Website Test")

## What is NeoMutt?

* NeoMutt is a project of projects.
* A place to gather all the patches against Mutt.
* A place for all the developers to gather.

Hopefully this will build the community and reduce duplicated effort.

NeoMutt was created when Richard Russon (@FlatCap) took all the old Mutt patches,
sorted through them, fixed them up and documented them.

## What Features does NeoMutt have?

| Name                     | Description
| ------------------------ | ------------------------------------------------------
| Attach Headers Color     | Color attachment headers using regex, just like mail bodies
| Compose to Sender        | Send new mail to the sender of the current mail
| Compressed Folders       | Read from/write to compressed mailboxes
| Conditional Dates        | Use rules to choose date format
| Custom Mailbox Tags      | Implements Notmuch tags and Imap keywords
| Encrypt-to-Self          | Save a self-encrypted copy of emails
| Fmemopen                 | Replace some temporary files with memory buffers
| Forgotten Attachment     | Alert user when (s)he forgets to attach a file to an outgoing email.
| Global Hooks             | Define actions to run globally within NeoMutt
| Header Cache Compression | Options for compressing the header cache files
| Ifdef                    | Conditional config options
| Index Color              | Custom rules for theming the email index
| Initials Expando         | Expando for author's initials
| Kyoto Cabinet            | Kyoto Cabinet backend for the header cache
| Limit Current Thread     | Focus on one Email Thread
| LMDB                     | LMDB backend for the header cache
| Multiple FCC             | Save multiple copies of outgoing mail
| Nested If                | Allow complex nested conditions in format strings
| New Mail                 | Execute a command upon the receipt of new mail.
| NNTP                     | Talk to a Usenet news server
| Notmuch                  | Email search engine
| Progress Bar             | Show a visual progress bar on slow operations
| Quasi-Delete             | Mark emails that should be hidden, but not deleted
| Reply With X-Orig-To     | Direct reply to email using X-Original-To header
| Sensible Browser         | Make the file browser behave
| Sidebar                  | Panel containing list of Mailboxes
| Skip Quoted              | Leave some context visible
| Status Color             | Custom rules for theming the status bar
| TLS-SNI                  | Negotiate with a server for a TLS/SSL certificate
| Trash Folder             | Automatically move deleted emails to a trash bin

## Contributed Scripts and Config

| Name                   | Description
| ---------------------- | ---------------------------------------------
| Header Cache Benchmark | Script to test the speed of the header cache
| Keybase                | Keybase Integration
| Useful programs        | List of useful programs interacting with NeoMutt
| Vi Keys                | Easy and clean Vi-keys for NeoMutt
| Vim Syntax             | Vim Syntax File

## How to Install NeoMutt?

NeoMutt may be packaged for your distribution, and otherwise it can be built from
source. Please refer to the instructions on the
[distro page](https://neomutt.org/distro.html).

## Where is NeoMutt?

- Source Code:     https://github.com/neomutt/neomutt
- Releases:        https://github.com/neomutt/neomutt/releases/latest
- Questions/Bugs:  https://github.com/neomutt/neomutt/issues
- Website:         https://neomutt.org
- IRC:             [irc://irc.freenode.net/neomutt](https://webchat.freenode.net/ "IRC Web Client") - please be patient.
  We're a small group, so our answer might take some time.
- Mailinglists:    [neomutt-users](mailto:neomutt-users-request@neomutt.org?subject=subscribe)
  and [neomutt-devel](mailto:neomutt-devel-request@neomutt.org?subject=subscribe)
- Development:     https://neomutt.org/dev.html
- Contributors:    [Everyone who has helped NeoMutt](AUTHORS.md)

## Copyright

NeoMutt is released under version 2 of the GPL, the GNU General Public License.
See [LICENSE.md](LICENSE.md).

The principal authors of NeoMutt are:

- Copyright (C) 1996-2016 Michael R. Elkins `<me@.s.hmc.edu>`
- Copyright (C) 1996-2002 Brandon Long `<blong@fiction.net>`
- Copyright (C) 1997-2009 Thomas Roessler `<roessler@does-not-exist.org>`
- Copyright (C) 1998-2005 Werner Koch `<wk@isil.d.shuttle.de>`
- Copyright (C) 1999-2017 Brendan Cully `<brendan@kublai.com>`
- Copyright (C) 1999-2002 Tommi Komulainen `<Tommi.Komulainen@iki.fi>`
- Copyright (C) 2000-2004 Edmund Grimley Evans `<edmundo@rano.org>`
- Copyright (C) 2000-2019 David Champion `<dgc.mutt@c13.us>`
- Copyright (C) 2006-2009 Rocco Rutte `<pdmef@gmx.net>`
- Copyright (C) 2014-2019 Kevin J. McCarthy `<kevin@8t8.us>`
- Copyright (C) 2015-2019 Richard Russon `<rich@flatcap.org>`

