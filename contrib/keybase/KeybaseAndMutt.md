To use Keybase in Mutt, the first thing that must be done (before even the macro) is setting your editor. This is important because we can replace the editor with any command we could think of. Mine looks like this.

`set editor = 'echo %s > ~/.mutt/keybaseMutt/.tmp; vim %s'`

It is crucially important, without this line nothing will work.

What is really happening in that line is this. The echo command is echoing the contents of %s into a file. Typically, an editor will use %s to open the temporary file to let you create an email. We've just used it to work with a python script later on.

Next, set a macro in the .muttrc

`macro compose K "<enter-command>unset wait_key<enter><shell-escape>python ~/.mutt/keybaseMutt/keybase.py<enter><enter-command>set wait_key<enter>`

You will need to place the python script in your ~/.mutt/keybaseMutt directory, creating it if it doesn't exist. If you wish to place the script elsewhere, change the macro to point to the script.

The python script will take certain Keybase commands and run them. (Right now it will only let you encrypt or sign a message.)

The output of the script will be written directly into the email. (Thank the devs for using /tmp/mutt* files.) As you may have guessed, the you will only be able to send inline encrypted messages. No MIME attachments.

Creating encrypted email is now quite easy. All you need to do is press "K" and type something to the effect of `keybase encrypt [user]` or `keybase pgp encrypt [user]`.

The script will direct all output into the temporary file and replace your message with an encrypted version.

Signing is done the same way.

Decryption and verification is a bit more tricky. Unfortunately, mutt doesn't use /tmp/mutt* files for emails that you receive, and I don't understand mutt well enough to find the email on the drive.

To work around that, I've created several scripts to decrypt or verify messages. While you are reading an email that's been encrypted, use the pipe command in mutt ("|").

You will be able to pipe the contents of the email into one of the scripts to decrypt or verify the message.

How you do that is up to you, if you want to feed it the explicit location of the script, that will work fine. (Something like this `/home/[user]/path/to/script.sh`.)

Or, if you know how to create paths (or if you put it in the `/bin` directory), you only need to feed it the name of the script (`script.sh`). Look it up if you don't know how. Its fun!

There will be four separate scripts: verify.sh, decrypt.sh, pgpverify.sh, pgpdecrypt.sh

Unfortunately, I wasn't able to find a way to condense them into one script. (Mainly because I'm a little short on time. That may change later.)

Each script should be decently self-explanatory. The verify and decrypt scripts will work on keybase messages, while the pgpverify and pgpdecrypt scripts will work on keybase pgp messages.
