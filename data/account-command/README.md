# Introduction

NeoMutt provides a general mechanism to retrieve password information rather
than storing these sensitive data in the config files using `imap_pass`,
`smtp_pass`, and other `*_pass` variables.
See [the feature page](https://neomutt.org/feature/account-cmd) for more info.

# Sample password managers

This directory contains sample managers / solutions.

- [Mac OSX Keychain (Python 3 required)](macos-keychain)
- [GPG-based JSON database](gpg-json)
