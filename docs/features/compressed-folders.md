# Compressed Folders

_Read from/write to compressed mailboxes_

## Support

**Since:** NeoMutt 2016-05-30

**Dependencies:** None

## Introduction

The Compressed Folder feature allows NeoMutt to read mailbox files that are compressed. But it isn't limited to compressed files. It works well with encrypted files, too. In fact, if you can create a program/script to convert to and from your format, then NeoMutt can read it.

The feature adds three hooks to NeoMutt:`open-hook`,`close-hook` and`append-hook`. They define commands to: uncompress a file; compress a file; append messages to an already compressed file.

There are some examples of both compressed and encrypted files, later. For now, the documentation will just concentrate on compressed files.

## Commands

`open-hook` _`regex`_ _`"shell-command"`_   
`close-hook` _`regex`_ _`"shell-command"`_   
`append-hook` _`regex`_ _`"shell-command"`_ 

The shell-command must contain two placeholders for filenames:`%f` and`%t`. These represent“from” and“to” filenames. These placeholders should be placed inside single-quotes to prevent unintended shell expansions.

If you need the exact string“%f” or“%t” in your command, simply double up the“%” character, e.g.“%%f” or“%%t”.

**Not all Hooks are Required**

| Open | Close | Append | Effect                                                                             | Useful if                                         |
| ---- | ----- | ------ | ---------------------------------------------------------------------------------- | ------------------------------------------------- |
| Open | —     | —      | Folder is readonly                                                                 | The folder is just a backup                       |
| Open | Close | —      | Folder is read/write, but the entire folder must be written if anything is changed | Your compression format doesn't support appending |
| Open | Close | Append | Folder is read/write and emails can be efficiently added to the end                | Your compression format supports appending        |
| Open | —     | Append | Folder is readonly, but can be appended to                                         | You want to store emails, but never change them   |

### Note

The command:

* should return a non-zero exit status on failure
* should not delete any files

### Read from compressed mailbox

`open-hook regex "shell-command"`

If NeoMutt is unable to open a file, it then looks for`open-hook` that matches the filename.

If your compression program doesn't have a well-defined extension, then you can use`.` as the regex.

**Example of`open-hook`**

```
open-hook '\.gz$' "gzip --stdout --decompress '%f' > '%t'"
```

* NeoMutt finds a file,“example.gz”, that it can't read
* NeoMutt has an`open-hook` whose regex matches the filename:`\.gz$`
* NeoMutt uses the command`gzip -cd` to create a temporary file that it _can_ read

### Write to a compressed mailbox

`close-hook regex "shell-command"`

When NeoMutt has finished with a compressed mail folder, it will look for a matching`close-hook` to recompress the file. This hook is[optional](#table-compress-optional "Table 12.1. Not all Hooks are Required").

### Note

If the folder has not been modified, the`close-hook` will not be called.

**Example of`close-hook`**

```
close-hook '\.gz$' "gzip --stdout '%t' > '%f'"
```

* NeoMutt has finished with a folder,“example.gz”, that it opened with`open-hook`
* The folder has been modified
* NeoMutt has a`close-hook` whose regex matches the filename:`\.gz$`
* NeoMutt uses the command`gzip -c` to create a new compressed file

### Note

The`close-hook` can also include extra options, e.g. compression level:`--best`

### Append to a compressed mailbox

`append-hook regex "shell-command"`

When NeoMutt wants to append an email to a compressed mail folder, it will look for a matching`append-hook`. This hook is[optional](#table-compress-optional "Table 12.1. Not all Hooks are Required").

Using the`append-hook` will save time, but NeoMutt won't be able to determine the type of the mail folder inside the compressed file.

NeoMutt will _assume_ the type to be that of the`$mbox_type` variable. NeoMutt also uses this type for temporary files.

NeoMutt will only use the`append-hook` for existing files. The`close-hook` will be used for empty, or missing files.

### Note

If your command writes to stdout, it is vital that you use`>>` in the“append-hook”. If not, data will be lost.

**Example of`append-hook`**

```
append-hook '\.gz$' "gzip --stdout '%t' >> '%f'"
```

* NeoMutt wants to append an email to a folder,“example.gz”, that it opened with`open-hook`
* NeoMutt has an`append-hook` whose regex matches the filename:`\.gz$`
* NeoMutt knows the mailbox type from the`$mbox` variable
* NeoMutt uses the command`gzip -c` to append to an existing compressed file

### Note

The`append-hook` can also include extra options, e.g. compression level:`--best`

### Empty Files

NeoMutt assumes that an empty file is not compressed. In this situation, unset[$save\_empty](/guide/reference.html#save-empty "3.413. save_empty"), so that the compressed file will be removed if you delete all of the messages.

### Security

Encrypted files are decrypted into temporary files which are stored in the[$tmp\_dir](/guide/reference.html#tmp-dir "3.522. tmp_dir") directory. This could be a security risk.

## neomuttrc

```
# Example NeoMutt config file for the compress feature.

# This feature adds three hooks to NeoMutt which allow it to
# work with compressed, or encrypted, mailboxes.

# The hooks are of the form:
#       open-hook   regex "shell-command"
#       close-hook  regex "shell-command"
#       append-hook regex "shell-command"
# The 'append-hook' is optional.

# Handler for gzip compressed mailboxes
open-hook   '\.gz$' "gzip --stdout --decompress '%f' >  '%t'"
close-hook  '\.gz$' "gzip --stdout              '%t' >  '%f'"
append-hook '\.gz$' "gzip --stdout              '%t' >> '%f'"
# Handler for bzip2 compressed mailboxes
open-hook   '\.bz2$' "bzip2 --stdout --decompress '%f' >  '%t'"
close-hook  '\.bz2$' "bzip2 --stdout              '%t' >  '%f'"
append-hook '\.bz2$' "bzip2 --stdout              '%t' >> '%f'"
# Handler for xz compressed mailboxes
open-hook   '\.xz$' "xz --stdout --decompress '%f' >  '%t'"
close-hook  '\.xz$' "xz --stdout              '%t' >  '%f'"
append-hook '\.xz$' "xz --stdout              '%t' >> '%f'"
# Handler for pgp encrypted mailboxes
# PGP does not support appending to an encrypted file
open-hook   '\.pgp$' "pgp -f < '%f' > '%t'"
close-hook  '\.pgp$' "pgp -fe YourPgpUserIdOrKeyId < '%t' > '%f'"
# Handler for gpg encrypted mailboxes
# gpg does not support appending to an encrypted file
open-hook   '\.gpg$' "gpg --decrypt < '%f' > '%t'"
close-hook  '\.gpg$' "gpg --encrypt --recipient YourGpgUserIdOrKeyId < '%t' > '%f'"

# vim: syntax=neomuttrc
```

## See Also

* [Regular Expressions](/guide/advancedusage.html#regex "2. Regular Expressions")
* [$tmp\_dir](/guide/reference.html#tmp-dir "3.522. tmp_dir")
* [$mbox\_type](/guide/reference.html#mbox-type "3.239. mbox_type")
* [$save\_empty](/guide/reference.html#save-empty "3.413. save_empty")
* [folder-hook](/guide/configuration.html#folder-hook "9. Setting Variables Based Upon Mailbox")

## Credits

Roland Rosenfeld, Alain Penders, Christoph“Myon” Berg, Evgeni Golov, Richard Russon
