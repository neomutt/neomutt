# KeybaseMutt

A work in progress. KeybaseMutt exists to let its users utilize Keybase within mutt. (Unfortunately, not natively (yet).)

## Quick start

Run the install script. It will set up the directory's and paths for you. Do what it says or suffer the consequences!

Then just throw these into your .muttrc

`set editor = 'echo %s > ~/.mutt/keybaseMutt/.tmp; vim %s'`
`macro compose K "<enter-command>unset wait_key<enter><shell-escape>python ~/.mutt/keybaseMutt/keybase.py<enter><enter-command>set wait_key<enter>`

Done!

## How to use KeybaseMutt

Write an email. In the screen right before you send your email (called the pager mode), press "K".

![](pagerMode.png)
(Press 'K' when you're here)

You can now use four Keybase commands in this "shell". (I thought it best to limit the user to four commands for security reasons. Running unusual commands would overwrite the email, forcing the user to rewrite the email again.)

The commands are:
- `keybase encrypt [user]`
- `keybase pgp encrypt [user]`
- `keybase sign`
- `keybase pgp sign`
