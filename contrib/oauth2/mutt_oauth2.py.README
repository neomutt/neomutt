mutt_oauth.py README by Alexander Perlis, 2020-07-15
====================================================


Background on plain passwords, app passwords, OAuth2 bearer tokens
------------------------------------------------------------------

An auth stage occurs near the start of the IMAP/POP/SMTP protocol
conversation. Various SASL methods can be used (depends on what the
server offers, and what the client supports). The PLAIN method, also
known as "basic auth", involves simply sending the username and
password (this occurs over an encrypted connection), and used to be
common; but, for large cloud mail providers, basic auth is a security
hole. User passwords often have low entropy (humans generally choose
passwords that can be produced from human memory), thus are targets
for various types of exhaustive attacks.  Older attacks try different
passwords against one user, whereas newer spray attacks try one
password against different users. General mitigation efforts such as
rate-limiting, or detection and outright blocking efforts, lead to
degraded or outright denied services for legitimate users. The
security weakness is two-fold: the low entropy of the user password,
together with the alarming consequence that the password often unlocks
many disparate systems in a typical enterprise single-sign-on
environment. Also, humans type passwords or copy/paste them from
elsewhere on the screen, so they can also be grabbed via keyloggers or
screen capture (or a human bystander). Two ways to solve these
conundrums:

 - app passwords
 - bearer tokens

App passwords are simply high-entropy protocol-specific passwords, in
other words a long computer-generated random string, you use one for
your mail system, a different one for your payroll system, and so
on. With app passwords in use, brute-force attacks become useless. App
passwords require no modifications to client software, and only minor
changes on the server side.  One way to think about app passwords is
that they essentially impose on you the use of a password manager. Any
user can go to the trouble of using a password manager but most users
don't bother. App passwords put the password manager inside the server
and force you to use it.

Bearer tokens take the idea of app passwords to the next level. Much
like app passwords, they too are just long computer-generated random
strings, knowledge of which simply "lets you in". But unlike an app
password which the user must manually copy from a server password
screen and then paste into their client account config screen (a
process the user doesn't want to follow too often), bearer tokens get
swapped out approximately once an hour without user interaction. For
this to work, both clients and servers must be modified to speak a
separate out-of-band protocol (the "OAuth2" protocol) to swap out
tokens. More precisely, from start to finish, the process goes like
this: the client and server must once-and-for-all be informed about
each other (this is called "app registration" and might be done by the
client developer or left to each end user), then the client informs
the server that it wants to connect, then the user is informed to
independently use a web browser to visit a server destination to
approve this request (at this stage the server will require the user
to authenticate using say their password and perhaps additional
factors such as an SMS verification or crypto device), then the client
will have a long-term "refresh token" as well as an "access token"
good for about an hour. The access token can now be used with
IMAP/POP/SMTP to access the account. When it expires, the refresh
token is used to get a new access token and perhaps a new refresh
token.  After several months of such usage, even the refresh token may
expire and the human user will have to go back and re-authenticate
(password, SMS, crypto device, etc) for things to start anew.

Since app passwords and tokens are high-entropy and their compromise
should compromise only a particular system (rather than all systems in
a single-sign-on environment), they have similar security strength
when compared to stark weakness of traditional human passwords. But if
compared only to each other, tokens provide more security. App
passwords must be short enough for humans to easily copy/paste them,
might get written down or snooped during that process, and anyhow are
long-lived and thus could get compromised by other means. The main
drawback to tokens is that their support requires significant changes
to clients and servers, but once such support exists, they are
superior and easier to use.

Many cloud providers are eliminating support for human passwords. Some are
allowing app passwords in addition to tokens. Some allow only tokens.


OAuth2 token support in mutt
----------------------------

Mutt supports the two SASL methods OAUTHBEARER and XOAUTH2 for presenting an
OAuth2 access token near the start of the IMAP/POP/SMTP connection.

(Two different SASL methods exist for historical reasons. While OAuth2
was under development, the experimental offering by servers was called
XOAUTH2, later fleshed out into a standard named OAUTHBEARER, but not
all servers have been updated to offer OAUTHBEARER. Once the major
cloud providers all support OAUTHBEARER, clients like mutt might be
modified to no longer know about XOAUTH2.)

Mutt can present a token inside IMAP/POP/SMTP, but by design mutt itself
does not know how to have a separate conversation (outside of IMAP/POP/SMTP)
with the server to authorize the user and obtain refresh and access tokens.
Mutt just needs an access token, and has a hook for an external script to
somehow obtain one.

mutt_oauth2.py is an example of such an external script. It likely can be
adapted to work with OAuth2 on many different cloud mail providers, and has
been tested against:

 - Google consumer account (@gmail.com)
 - Google work/school account (G Suite tenant)
 - Microsoft consumer account (e.g., @live.com, @outlook.com, ...)
 - Microsoft work/school account (Azure tenant)
   (Note that Microsoft uses the marketing term "Modern Auth" in lieu of
   "OAuth2". In that terminology, mutt indeed supports "Modern Auth".)


Configure script's token file encryption
----------------------------------------

The script remembers tokens between invocations by keeping them in a
token file. This file is encrypted. Inside the script are two lines
  ENCRYPTION_PIPE
  DECRYPTION_PIPE
that must be edited to specify your choice of encryption system. A
popular choice is gpg. To use this:

 - Install gpg. For example, "sudo apt install gpg".
 - "gpg --gen-key". Answer the questions. Instead of your email
   address you could choose say "My mutt_oauth2 token store", then
   choose a passphrase. You will need to produce that same passphrase
   whenever mutt_oauth2 needs to unlock the token store.
 - Edit mutt_oauth2.py and put your GPG identity (your email address or
   whatever you picked above) in the ENCRYPTION_PIPE line.
 - For the gpg-agent to be able to ask you the unlock passphrase,
   the environment variable GPG_TTY must be set to the current tty.
   Typically you would put the following inside your .bashrc or equivalent:
     export GPG_TTY=$(tty)


Create an app registration
--------------------------

Before you can connect the script to an account, you need an
"app registration" for that service. Cloud entities (like Google and
Microsoft) and/or the tenant admins (the central technology admins at
your school or place of work) might be restrictive in who can create
app registrations, as well as who can subsequently use them. For
personal/consumer accounts, you can generally create your own
registration and then use it with a limited number of different personal
accounts. But for work/school accounts, the tenant admins might approve an
app registration that you created with a personal/consumer account, or
might want an official app registration from a developer (the creation of
which and blessing by the cloud provider might require payment and/or arduous
review), or might perhaps be willing to roll their own "in-house" registration.

What you ultimately need is the "client_id" (and "client_secret" if
one was set) for this registration. Those values must be edited into
the mutt_oauth2.py script.  If your work or school environment has a
knowledge base that provides the client_id, then someone already took
care of the app registration, and you can skip the step of creating
your own registration.


-- How to create a Google registration --

Go to console.developers.google.com, and create a new project. The name doesn't
matter and could be "mutt registration project".

 - Go to Library, choose Gmail API, and enable it
 - Hit left arrow icon to get back to console.developers.google.com
 - Choose OAuth Consent Screen
   - Choose Internal for an organizational G Suite
   - Choose External if that's your only choice
   - For Application Name, put for example "Mutt"
   - Under scopes, choose Add scope, scroll all the way down, enable the "https://mail.google.com/" scope
   - Fill out additional fields (application logo, etc) if you feel like it (will make the consent screen look nicer)
 - Back at console.developers.google.com, choose Credentials
 - At top, choose Create Credentials / OAuth2 client iD
   - Application type is "Desktop app"

Edit the client_id (and client_secret if there is one) into the
mutt_oauth2.py script.


-- How to create a Microsoft registration --

Go to portal.azure.com, log in with a Microsoft account (get a free
one at outlook.com), then search for "app registration", and add a
new registration. On the initial form that appears, put a name like
"Mutt", allow any type of account, and put "http://localhost/" as
the redirect URI, then more carefully go through each
screen:

Branding
 - Leave fields blank or put in reasonable values
 - For official registration, verify your choice of publisher domain
Authentication:
 - Platform "Mobile and desktop"
 - Redirect URI "http://localhost/"
 - Any kind of account
 - Enable public client (allow device code flow)
API permissions:
 - Microsoft Graph, Delegated, "offline_access"
 - Microsoft Graph, Delegated, "IMAP.AccessAsUser.All"
 - Microsoft Graph, Delegated, "POP.AccessAsUser.All"
 - Microsoft Graph, Delegated, "SMTP.Send"
 - Microsoft Graph, Delegated, "User.Read"
Overview:
 - Take note of the Application ID (a.k.a. Client ID), you'll need it shortly

End users who aren't able to get to the app registration screen within
portal.azure.com for their work/school account can temporarily use an
incognito browser window to create a free outlook.com account and use that
to create the app registration.

Edit the client_id (and client_secret if there is one) into the
mutt_oauth2.py script.


Running the script manually to authorize tokens
-----------------------------------------------

Run "mutt_oauth2.py --help" to learn script usage. To obtain the
initial set of tokens, run the script specifying a name for a
disposable token storage file, as well as "--authorize", for example
using this naming scheme:

 mutt_oauth2.py userid@myschool.edu.tokens --verbose --authorize

The script will ask questions and provide some instructions. For the
flow question:

 - "authcode": you paste a complicated URL into a browser, then
manually extract a "code" parameter from a subsequent URL in the
browser address bar and paste that back to the script.

- "localhostauthcode": you again paste the complicated URL into a browser
but that's it --- the code is automatically extracted from the response
relying on a localhost redirect and temporarily listening on a localhost
port. This flow can only be used if the web browser opening the redirect
URL sits on the same machine as where mutt is running, in other words can not
be used if you ssh to a remote machine and run mutt on that remote machine
while your web browser remains on your local machine.

 - "devicecode": you go to a simple URL and just enter a short code.

Your answer here determines the default flow, but on any invocation of
the script you can override the default with the optional "--authflow"
parameter. To change the default, delete your token file and start over.

To figure out which flow to use, I suggest trying all three.
Depending on the OAuth2 provider and how the app registration was
configured, some flows might not work, so simply trying them is the
best way to figure out what works and which one you prefer. Personally
I prefer the "localhostauthcode" flow when I can use it.


Once you attempt an actual authorization, you might get stuck because
the web browser step might indicate your institution admins must grant
approval.  Indeed engage them in a conversation about approving the
use of mutt to access mail. If that fails, an alternative is to
identify some other well-known IMAP/POP/SMTP client that they might
have already approved, or might be willing to approve, and first go
configure it for OAuth2 and see whether it will work to reach your
mail, and then you could dig into the source code for that client and
extract its client_id, client_secret, and redirect_uri and put those
into the mutt_oauth2.py script. This would be a temporary punt for
end-user experimentation, but not an approach for configuring systems
to be used by other people. Engaging your institution admins to create
a mutt registration is the better way to go.

Once you've succeeded authorizing mutt_oauth2.py to obtain tokens, try
one of the following to see whether IMAP/POP/SMTP are working:

 mutt_oauth2.py userid@myschool.edu.tokens --verbose --test
 mutt_oauth2.py userid@myschool.edu.tokens --verbose --debug --test

Without optional parameters, the script simply returns an access token
(possibly first conducting a behind-the-scenes URL retrieval using a
stored refresh token to obtain an updated access token). Calling the
script without optional parameters is how it will be used by
mutt. Your .muttrc would look something like:

 set imap_user="userid@myschool.edu"
 set folder="imap://outlook.office365.com/"
 set smtp_url="smtp://${imap_user}@smtp.office365.com:587/"
 set imap_authenticators="oauthbearer:xoauth2"
 set imap_oauth_refresh_command="/path/to/script/mutt_oauth2.py ${imap_user}.tokens"
 set smtp_authenticators=${imap_authenticators}
 set smtp_oauth_refresh_command=${imap_oauth_refresh_command}

