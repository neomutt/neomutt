# This is the NeoMutt Project

[![Stars](https://img.shields.io/github/stars/neomutt/neomutt.svg?style=social&label=Stars)](https://github.com/neomutt/neomutt "Give us a Star")
[![Twitter](https://img.shields.io/twitter/follow/NeoMutt_Org.svg?style=social&label=Follow)](https://twitter.com/NeoMutt_Org "Follow us on Twitter")
[![Contributors](https://img.shields.io/badge/Contributors-115-orange.svg)](#contributors)
[![Release](https://img.shields.io/github/release/neomutt/neomutt.svg)](https://github.com/neomutt/neomutt/releases/latest "Latest Release Notes")
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://github.com/neomutt/neomutt/blob/master/COPYRIGHT)
[![Code build](https://img.shields.io/travis/neomutt/neomutt.svg?label=code)](https://travis-ci.org/neomutt/neomutt)
[![Coverity Scan](https://img.shields.io/coverity/scan/8495.svg)](https://scan.coverity.com/projects/neomutt-neomutt)
[![Website build](https://img.shields.io/travis/neomutt/neomutt.github.io.svg?label=website)](https://travis-ci.org/neomutt/neomutt.github.io)

## What is NeoMutt?

* NeoMutt is a project of projects.
* A place to gather all the patches against Mutt.
* A place for all the developers to gather.

Hopefully this will build the community and reduce duplicated effort.

NeoMutt was created when Richard Russon (FlatCap) took all the old Mutt patches,
sorted through them, fixed them up and documented them.

## What Features does NeoMutt have?

| Name                 | Description
| -------------------- | ------------------------------------------------------
| Attach Headers Color | Color attachment headers using regex, just like mail bodies
| Compose to Sender    | Send new mail to the sender of the current mail
| Compressed Folders   | Read from/write to compressed mailboxes
| Conditional Dates    | Use rules to choose date format
| Encrypt-to-Self      | Save a self-encrypted copy of emails
| Fmemopen             | Replace some temporary files with memory buffers
| Forgotten Attachment | Alert user when (s)he forgets to attach a file to an outgoing email.
| Global Hooks         | Define actions to run globally within NeoMutt
| Ifdef                | Conditional config options
| Index Color          | Custom rules for theming the email index
| Initials Expando     | Expando for author's initials
| Kyoto Cabinet        | Kyoto Cabinet backend for the header cache
| Limit Current Thread | Focus on one Email Thread
| LMDB                 | LMDB backend for the header cache
| Multiple FCC         | Save multiple copies of outgoing mail
| Nested If            | Allow complex nested conditions in format strings
| New Mail             | Execute a command upon the receipt of new mail.
| NNTP                 | Talk to a Usenet news server
| Notmuch              | Email search engine
| Progress Bar         | Show a visual progress bar on slow operations
| Quasi-Delete         | Mark emails that should be hidden, but not deleted
| Reply With X-Orig-To | Direct reply to email using X-Original-To header
| Sensible Browser     | Make the file browser behave
| Sidebar              | Panel containing list of Mailboxes
| Skip Quoted          | Leave some context visible
| Status Color         | Custom rules for theming the status bar
| TLS-SNI              | Negotiate with a server for a TLS/SSL certificate
| Trash Folder         | Automatically move deleted emails to a trash bin

## Contributed Scripts and Config

| Name                   | Description
| ---------------------- | ---------------------------------------------
| Header Cache Benchmark | Script to test the speed of the header cache
| Keybase                | Keybase Integration
| Useful programs        | List of useful programs interacting with NeoMutt
| Vi Keys                | Easy and clean Vi-keys for NeoMutt
| Vim Syntax             | Vim Syntax File

## Where is NeoMutt?

- Source Code:     https://github.com/neomutt/neomutt
- Releases:        https://github.com/neomutt/neomutt/releases/latest
- Questions/Bugs:  https://github.com/neomutt/neomutt/issues
- Website:         https://www.neomutt.org
- IRC:             irc://irc.freenode.net/neomutt - please be patient.
  We're a small group, so our answer might take some time.
- Mailinglists:    [neomutt-users](mailto:neomutt-users-request@neomutt.org?subject=subscribe)
  and [neomutt-devel](mailto:neomutt-devel-request@neomutt.org?subject=subscribe)
- Development:     https://www.neomutt.org/dev.html

## Contributors

Here's a list of everyone who's helped NeoMutt:

[Adam Borowski](https://github.com/kilobyte "kilobyte"),
[Alad Wenter](https://github.com/AladW "AladW"),
[Aleksa Sarai](https://github.com/cyphar "cyphar"),
[Alex Pearce](https://github.com/alexpearce "alexpearce"),
[Alok Singh](https://github.com/Alok "Alok"),
[Ander Punnar](https://github.com/4nd3r "4nd3r"),
[Andreas Rammhold](https://github.com/andir "andir"),
[André Berger](https://github.com/hvkls "hvkls"),
[Anton Rieger](https://github.com/seishinryohosha "seishinryohosha"),
[Antonio Radici](https://github.com/aradici "aradici"),
[Austin Ray](https://github.com/Austin-Ray "Austin-Ray"),
[Baptiste Daroussin](https://github.com/bapt "bapt"),
[Benjamin Mako Hill](https://github.com/makoshark "makoshark"),
[Bernard Pratz](https://github.com/guyzmo "guyzmo"),
[Bletchley Park](https://github.com/libBletchley "libBletchley"),
[Bo Yu](https://github.com/yuzibo "yuzibo"),
[Bryan Bennett](https://github.com/bbenne10 "bbenne10"),
[Chris Czettel](https://github.com/christopher-john-czettel "christopher-john-czettel"),
[Chris Salzberg](https://github.com/shioyama "shioyama"),
[Christian Dröge](https://github.com/cdroege "cdroege"),
[Christoph Berg](https://github.com/ChristophBerg "ChristophBerg"),
[cinder88](https://github.com/cinder88 "cinder88"),
[Clemens Lang](https://github.com/neverpanic "neverpanic"),
[Damien Riegel](https://github.com/d-k-c "d-k-c"),
[Darshit Shah](https://github.com/darnir "darnir"),
[David Sterba](https://github.com/kdave "kdave"),
[Dimitrios Semitsoglou-Tsiapos](https://github.com/dset0x "dset0x"),
[Doug Stone-Weaver](https://github.com/doweaver "doweaver"),
[Edward Betts](https://github.com/EdwardBetts "EdwardBetts"),
[Elimar Riesebieter](https://github.com/riesebie "riesebie"),
[Evgeni Golov](https://github.com/evgeni "evgeni"),
[Fabian Groffen](https://github.com/grobian "grobian"),
[Fabio Locati](https://github.com/Fale "Fale"),
[Fabrice Bellet](https://github.com/fbellet "fbellet"),
[Faidon Liambotis](https://github.com/paravoid "paravoid"),
[Federico Kircheis](https://github.com/fekir "fekir"),
[Florian Klink](https://github.com/flokli "flokli"),
[Floyd Anderson](https://github.com/floand "floand"),
[František Hájik](https://github.com/ferkohajik "ferkohajik"),
[Guillaume Brogi](https://github.com/guiniol "guiniol"),
[Hugo Barrera](https://github.com/WhyNotHugo "WhyNotHugo"),
[Ian Zimmerman](https://github.com/nobrowser "nobrowser"),
[Ismaël Bouya](https://github.com/immae "immae"),
[Ivan Tham](https://github.com/pickfire "pickfire"),
[Jack Stratton](https://github.com/phroa "phroa"),
[Jakub Jindra](https://github.com/jindraj "jindraj"),
[Jakub Wilk](https://github.com/jwilk "jwilk"),
[Jasper Adriaanse](https://github.com/jasperla "jasperla"),
[Jelle van der Waa](https://github.com/jelly "jelly"),
[Jenya Sovetkin](https://github.com/esovetkin "esovetkin"),
[Johannes Frankenau](https://github.com/tsuflux "tsuflux"),
[Johannes Weißl](https://github.com/weisslj "weisslj"),
[Jonathan Perkin](https://github.com/jperkin "jperkin"),
[Joshua Jordi](https://github.com/JakkinStewart "JakkinStewart"),
[Julian Andres Klode](https://github.com/julian-klode "julian-klode"),
[Justin Vasel](https://github.com/justinvasel "justinvasel"),
[Karel Zak](https://github.com/karelzak "karelzak"),
[Kevin Decherf](https://github.com/Kdecherf "Kdecherf"),
[Kevin Velghe](https://github.com/paretje "paretje"),
[Kurt Jaeger](https://github.com/opsec "opsec"),
[Larry Rosenman](https://github.com/lrosenman "lrosenman"),
[Leo Lundgren](https://github.com/rawtaz "rawtaz"),
[Leonardo Schenkel](https://github.com/lbschenkel "lbschenkel"),
[Leonidas Spyropoulos](https://github.com/inglor "inglor"),
[Manos Pitsidianakis](https://github.com/epilys "epilys"),
[Marcin Rajner](https://github.com/mrajner "mrajner"),
[Marco Hinz](https://github.com/mhinz "mhinz"),
[Marius Gedminas](https://github.com/mgedmin "mgedmin"),
[Mateusz Piotrowski](https://github.com/0mp "0mp"),
[Matteo Vescovi](https://github.com/mfvescovi "mfvescovi"),
[Mehdi Abaakouk](https://github.com/sileht "sileht"),
[Michael Bazzinotti](https://github.com/bazzinotti "bazzinotti"),
[ng0](https://github.com/ng-0 "ng-0"),
[Nicolas Bock](https://github.com/nicolasbock "nicolasbock"),
[Olaf Lessenich](https://github.com/xai "xai"),
[parazyd](https://github.com/parazyd "parazyd"),
[Perry Thompson](https://github.com/rypervenche "rypervenche"),
[Peter Hogg](https://github.com/pigmonkey "pigmonkey"),
[Peter Lewis](https://github.com/petelewis "petelewis"),
[Phil Pennock](https://github.com/philpennock "philpennock"),
[Philipp Marek](https://github.com/phmarek "phmarek"),
[Pierre-Elliott Bécue](https://github.com/P-EB "P-EB"),
[Pietro Cerutti](https://github.com/gahr "gahr"),
[r3lgar](https://github.com/r3lgar "r3lgar"),
Regid Ichira,
[Reis Radomil](https://github.com/reisradomil "reisradomil"),
[Riad Wahby](https://github.com/kwantam "kwantam"),
[Richard Hartmann](https://github.com/RichiH "RichiH"),
[Richard Russon](https://github.com/flatcap "flatcap"),
[Roger Pau Monne](https://github.com/royger "royger"),
Rubén Llorente,
[Santiago Torres](https://github.com/SantiagoTorres "SantiagoTorres"),
[Serge Gebhardt](https://github.com/sgeb "sgeb"),
[sharktamer](https://github.com/sharktamer "sharktamer"),
[Shi Lee](https://github.com/rtlanceroad "rtlanceroad"),
[somini](https://github.com/somini "somini"),
[Stefan Assmann](https://github.com/sassmann "sassmann"),
[Stefan Bühler](https://github.com/stbuehler "stbuehler"),
[Stephen Gilles](https://github.com/s-gilles "s-gilles"),
[Steve Bennett](https://github.com/msteveb "msteveb"),
[Steven Ragnarök](https://github.com/nuclearsandwich "nuclearsandwich"),
[Sven Guckes](https://github.com/guckes "guckes"),
[Theo Jepsen](https://github.com/theojepsen "theojepsen"),
[Thiago Costa de Paiva](https://github.com/tecepe "tecepe"),
[Thomas Adam](https://github.com/ThomasAdam "ThomasAdam"),
[Thomas Klausner](https://github.com/0-wiz-0 "0-wiz-0"),
[Thomas Schneider](https://github.com/qsx "qsx"),
[Tobias Angele](https://github.com/toogley "toogley"),
Udo Schweigert,
Vsevolod Volkov,
[Werner Fink](https://github.com/bitstreamout "bitstreamout"),
[Wieland Hoffmann](https://github.com/mineo "mineo"),
[William Pettersson](https://github.com/WPettersson "WPettersson"),
[Yoshiki Vázquez Baeza](https://github.com/ElDeveloper "ElDeveloper"),
[Zero King](https://github.com/l2dy "l2dy").

### Patch Authors

Without the original patch authors, there would be nothing.
So, a Big Thank You to:

Aaron Schrab, Alain Penders, Benjamin Kuperman, Cedric Duval, Chris Mason,
Christian Aichinger, Christoph Rissner, David Champion, David Riebenbauer,
David Wilson, Don Zickus, Eric Davis, Felix von Leitner, Jan Synacek,
Jason DeTiberus, Jeremiah Foster, Jeremy Katz, Josh Poimboeuf, Julius Plenz,
Justin Hibbits, Kirill Shutemov, Luke Macken, Mantas Mikulenas, Patrick Brisbin,
Paul Miller, Philippe Le Brouster, Rocco Rutte, Roland Rosenfeld, Sami Farin,
Stefan Kuhn, Steve Kemp, Terry Chan, Thomas Glanzmann, Thomer Gil, Tim Stoakes,
Tyler Earnest, Victor Manuel Jaquez Leal, Vincent Lefevre, Vladimir Marek.

### Mutt

While NeoMutt is technically a fork of Mutt, the intention of the project is not to
diverge from Mutt, but rather to act as a common ground for developers to improve Mutt.

Collecting, sorting out and polishing patches to be incorporated upstream (into Mutt),
as well as being a place to gather and encourage further collaboration while reducing
redundant work, are among the main goals of NeoMutt. NeoMutt merges all changes from Mutt.

More information is available on the [About](https://www.neomutt.org/about.html) page on
the NeoMutt website.

Mutt was created by **Michael Elkins** and is now maintained by **Kevin McCarthy**.

https://www.neomutt.org/guide/miscellany.html#acknowledgements

