#!/usr/bin/env python3
#
# Mutt OAuth2 token management script, version 2020-08-07
# Written against python 3.7.3, not tried with earlier python versions.
#
#   Copyright (C) 2020 Alexander Perlis
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation; either version 2 of the
#   License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

#
# DESCRIPTION OF USE
#
#   This script interacts with OAuth2 endpoints (URLs) to obtain authorization
#   and access tokens.  When first run, the user should pass `--authorize`
#   triggering the script to obtain a new authorization, access, and refresh
#   tokens.  Subsequent runs do not need this flag as they will use the
#   tokens and info already provided to update the access token (if necessary)
#   and write it to stdout for neomutt/mbsync to use.
#
#   When running the program for the first time, the user will need to specify
#   several parameters.  If the user does not specify them as arguments, the
#   program will prompt the user to input that information interactively.
#
#   This program takes a path to the *tokenfile*.  Once an authorization token
#   is obtained, a table of parameters will be converted to JSON, passed through an
#   encryption program (`gpg` by default) and written to the path specified for
#   the tokenfile.
#
#   WARN: If the tokenfile exists, the data in it will override any user
#         provided arguments.
#
#   In OAuth2, a user logs in and authorizes an application (this script or
#   whatever application it's posing as via --client-id) and the permissions
#   (a.k.a. scope) they request.  Once the provider authenticates the user and
#   the user authorizes any permissions requested by the app (this script), the
#   provider will generate an *authorization code* that the script will then
#   trade in for an authorization token, refresh token, and some other data.
#
#   The way this script interacts with the provider changes depending on
#   --authflow.  In all cases, the script will open the provider's authorization
#   endpoint in a browser.  However, the way the provider passes the
#   authorization code back to this script varies based on `--authflow`.  When
#   using `localhostauthcode` or `devicecode` the script will start a web server
#   to receive the authorization code.  The URL of our web server is passed as
#   `redirect_uri` to the authorization endpoint.  With these two workflows, the
#   script will complete the process of obtaining the access token and generate
#   the tokenfile.
#
#   If you specify `authcode`, the script will prompt you for the address from
#   your browser or the authorization code, which you must input.  Most of
#   the time, you'll copy the contents of the address bar (of the failed request
#   to the `redirect_uri`) and paste it into this script.
#
#   FYI, authorization codes look like:
#
#           M.C517_SN1.2.U.An7Ux3hV...
#
#   More detail is available at
#
#     https://github.com/neomutt/neomutt/blob/main/contrib/oauth2/README.md

#
# EXAMPLES
#
#   Obtain authorization, refresh, and access tokens:
#
#     rm -f ~/.mutt_oauth2-outlook.gpg; mutt_oauth2.py --authorize --verbose \
#       --provider microsoft --tenant 'consumers' \
#       --authflow authcode \
#       --email 'bill@microsoft.com' \
#       --client-id '9e5f94bc-e8a4-4e73-b8be-63364c29d753' \
#       --client-secret '' \
#       --redirect_uri 'https://localhost'
#       ~/.mutt_oauth2-outlook.gpg
#
#   WARN: You'll likely run this program repeatedly to try and find the right
#         combination of parameters.  If you don't remove the tokenfile first,
#         the program will use the settings in the token file rather than the
#         ones you specify.
#
#   NOTE: `--email` Some Microsoft accounts have a `preferred_username` and one
#          or more aliases.  If Bill logs in with bill@microsoft.com but has an
#          alias bill@outlook.com, he'd want to use bill@microsoft.com as the
#          value of --email because microsoft (particularly SMTP) will REJECT
#          SASL strings (created for --test only) with aliases in `user=`.  SASL
#          strings look like:
#
#            user={user}\x01auth=Bearer {token}\x01\x01
#
#   NOTE: `--client-secret` being empty is not detectable by this script's
#         argument parser.  It will prompt you, and you hit enter to leave
#         blank (which works for Thunderbird, btw).
#
#   NOTE: `--redirect-uri` may not be necessary.  The Thunderbird client id
#         that Mozilla maintains at Microsoft (used in the example) requires a
#         redirect-uri of 'https://localhost'.  This forces authflow `authcode`
#         since this script does not support an HTTPS server.
#
#   NOTE: `--tenant` is only applicable for the `microsoft` provider.  It will
#         either be "common" (the default), "consumers", or the *Microsoft
#         Entra* or "Directory" ID (UUID) of your work or school organization.
#         When using `microsoft`, you may need to sign up with Azure to have a
#         valid Entra ID.
#
#   NOTE: If you use authflow `devicecode` with provider `microsoft`, you visit
#         the URL provided by the script, enter the code provided by the script,
#         then it will prompt you for a login.  If the browser asks for a
#         passkey, input the user and click *NEXT* first, or Microsoft will get
#         confused and return an error.
#
#   Test access token with IMAP, POP, and SMTP:
#
#     mutt_oauth2.py ~/.mutt_oauth2-outlook.gpg --verbose --test
#
#   Use refresh token if necessary to obtain a new access token then output:
#
#     mutt_oauth2.py ~/.mutt_oauth2-outlook.gpg
#
#   Inspect tokenfile:
#
#     gpg --decrypt ~/.mutt_oauth2-outlook.gpg

#
# CONFIGURATION
#
#   Once this works, you'll configure neomutt with:
#
#     set my_oauth_script = "python3 /path/to/mutt_oauth2.py /path/to/.mutt_oauth2-outlook.gpg"
#
#   ... or mbsync with:
#
#     PassCmd "python3 /path/to/mutt_oauth2.py /path/to/.mutt_oauth2-outlook.gpg"

'''Mutt OAuth2 token management'''

import sys
import json
import argparse
import urllib.parse
import urllib.request
import urllib.error
import imaplib
import poplib
import smtplib
import base64
import secrets
import hashlib
import time
from datetime import timedelta, datetime
from pathlib import Path
import shlex
import socket
import http.server
import subprocess
import readline # Enables the input of long lines like the log URL returned in  authflow: authcode
import webbrowser


# The token file must be encrypted because it contains multi-use bearer tokens
# whose usage does not require additional verification. Specify whichever
# encryption and decryption pipes you prefer. They should read from standard
# input and write to standard output. The example values here invoke GPG.
ENCRYPTION_PIPE = ['gpg', '--encrypt', '--default-recipient-self']
DECRYPTION_PIPE = ['gpg', '--decrypt']

registrations = {
    'google': {
        'authorize_endpoint': 'https://accounts.google.com/o/oauth2/auth',
        'devicecode_endpoint': 'https://oauth2.googleapis.com/device/code',
        'token_endpoint': 'https://accounts.google.com/o/oauth2/token',
        'redirect_uri': 'urn:ietf:wg:oauth:2.0:oob',
        'imap_endpoint': 'imap.gmail.com',
        'pop_endpoint': 'pop.gmail.com',
        'smtp_endpoint': 'smtp.gmail.com',
        'sasl_method': 'OAUTHBEARER',
        'scope': 'https://mail.google.com/',
    },
    'microsoft': {
        'authorize_endpoint': 'https://login.microsoftonline.com/common/oauth2/v2.0/authorize',
        'devicecode_endpoint': 'https://login.microsoftonline.com/common/oauth2/v2.0/devicecode',
        'token_endpoint': 'https://login.microsoftonline.com/common/oauth2/v2.0/token',
        'redirect_uri': 'https://login.microsoftonline.com/common/oauth2/nativeclient',
        'tenant': 'common',
        'imap_endpoint': 'outlook.office365.com',
        'pop_endpoint': 'outlook.office365.com',
        'smtp_endpoint': 'smtp.office365.com',
        'sasl_method': 'XOAUTH2',
        'scope': ('offline_access https://outlook.office.com/IMAP.AccessAsUser.All '
                  'https://outlook.office.com/POP.AccessAsUser.All '
                  'https://outlook.office.com/SMTP.Send'),
    },
}

ap = argparse.ArgumentParser(epilog='''
This script obtains and prints a valid OAuth2 access token.  State is maintained in an
encrypted TOKENFILE.  Run with "--verbose --authorize --encryption-pipe 'foo@bar.org'"
to get started or whenever all tokens have expired, optionally with "--authflow" to override
the default authorization flow.  To truly start over from scratch, first delete TOKENFILE.
Use "--verbose --test" to test the IMAP/POP/SMTP endpoints.
''')
ap.add_argument('-v', '--verbose', action='store_true', help='increase verbosity')
ap.add_argument('-d', '--debug', action='store_true', help='enable debug output')
ap.add_argument('tokenfile', help='persistent token storage')
ap.add_argument('-a', '--authorize', action='store_true', help='manually authorize new tokens')
ap.add_argument('--authflow', help='authcode | localhostauthcode | devicecode')
ap.add_argument('--format', type=str, choices=['token', 'sasl', 'msasl'], default='token',
                help='''output format:
    token - plain access token (default);
    sasl - base64 encoded SASL token string for the specified protocol [--protocol] and user [--email];
    msasl - like sasl, preceded with the SASL method''')
ap.add_argument('--protocol', type=str, choices=['imap', 'pop', 'smtp'], default='imap',
                help='protocol used for SASL output (default: imap)')
ap.add_argument('-t', '--test', action='store_true', help='test IMAP/POP/SMTP endpoints')
ap.add_argument('--decryption-pipe', type=shlex.split, default=DECRYPTION_PIPE,
                help='decryption command (string), reads from stdin and writes '
                'to stdout, default: "{}"'.format(
                    " ".join(DECRYPTION_PIPE)))
ap.add_argument('--encryption-pipe', type=shlex.split, default=ENCRYPTION_PIPE,
                help='encryption command (string), reads from stdin and writes '
                'to stdout, suggested: "{}"'.format(
                    " ".join(ENCRYPTION_PIPE)))
ap.add_argument('--client-id', type=str, default='',
                help='Provider id from registration')
ap.add_argument('--client-secret', type=str, default='',
                help='(optional) Provider secret from registration')
ap.add_argument('--redirect-uri', type=str, default='',
                help='Specify redirect-uri (May need to match match app registration with provider).')
ap.add_argument('--provider', type=str, choices=registrations.keys(),
                help='Specify provider to use.')
ap.add_argument('--tenant', type=str, default='common',
                help='Specify tenant (common: Work on School, consumers - personal).')
ap.add_argument('--email', type=str, help='Your email address.')
args = ap.parse_args()

ENCRYPTION_PIPE = args.encryption_pipe
DECRYPTION_PIPE = args.decryption_pipe

token = {}
path = Path(args.tokenfile)
if path.exists():
    if 0o777 & path.stat().st_mode != 0o600:
        sys.exit('Token file has unsafe mode. Suggest deleting and starting over.')
    try:
        sub = subprocess.run(DECRYPTION_PIPE, check=True, input=path.read_bytes(),
                             capture_output=True)
        token = json.loads(sub.stdout)
    except subprocess.CalledProcessError:
        sys.exit('Difficulty decrypting token file. Is your decryption agent primed for '
                 'non-interactive usage, or an appropriate environment variable such as '
                 'GPG_TTY set to allow interactive agent usage from inside a pipe?')


def writetokenfile():
    '''Writes global token dictionary into token file.'''
    if not path.exists():
        path.touch(mode=0o600)
    if 0o777 & path.stat().st_mode != 0o600:
        sys.exit('Token file has unsafe mode. Suggest deleting and starting over.')
    sub2 = subprocess.run(ENCRYPTION_PIPE, check=True, input=json.dumps(token).encode(),
                          capture_output=True)
    path.write_bytes(sub2.stdout)


if args.debug:
    print('Obtained from token file:', json.dumps(token))

if not token:
    if not args.authorize:
        sys.exit('You must run script with "--authorize" at least once.')
    if not ENCRYPTION_PIPE:
        sys.exit("You need to provide a suitable --encryption-pipe setting")
    print('', )
    token['registration'] = args.provider or input(
        'Available app and endpoint registrations: {regs}\nOAuth2 registration: '.format(
            regs=', '.join(registrations.keys())))
    token['authflow'] = args.authflow or input(
        'Preferred OAuth2 flow ("authcode" or "localhostauthcode" or "devicecode"): '
    )
    token['email'] = args.email or input('Account e-mail address: ')
    token['access_token'] = ''
    token['access_token_expiration'] = ''
    token['refresh_token'] = ''
    token['client_id'] = args.client_id or input('Client ID: ')
    token['client_secret'] = args.client_secret or input('Client secret: ')
    if token['registration'] == "microsoft":
        token['tenant'] = args.tenant
    if args.redirect_uri:
        # This overrides redirect_uri in registration
        token['redirect_uri'] = args.redirect_uri
    writetokenfile()

if token['registration'] not in registrations:
    sys.exit(f'ERROR: Unknown registration "{token["registration"]}". Delete token file '
             f'and start over.')
registration = registrations[token['registration']]
if token['registration'] == "microsoft":
    if not 'tenant' in token:
        # Existing token was not written with tenant.  Just default to common.
        token['tenant'] = 'common'
    # This only matters for microsoft registration
    if token['tenant'] != "common":
        registration['authorize_endpoint'] = registration['authorize_endpoint'].replace("common", token['tenant'], 1)
        registration['devicecode_endpoint'] = registration['devicecode_endpoint'].replace("common", token['tenant'], 1)
        registration['token_endpoint'] = registration['token_endpoint'].replace("common", token['tenant'], 1)
        registration['redirect_uri'] = registration['redirect_uri'].replace("common", token['tenant'], 1)
        registration['tenant'] = token['tenant']


authflow = token['authflow']
if args.authflow:
    authflow = args.authflow

baseparams = {'client_id': token['client_id']}
# Microsoft uses 'tenant' but Google does not
if 'tenant' in registration:
    baseparams['tenant'] = registration['tenant']


def access_token_valid():
    '''Returns True when stored access token exists and is still valid at this time.'''
    token_exp = token['access_token_expiration']
    return token_exp and datetime.now() < datetime.fromisoformat(token_exp)


def update_tokens(r):
    '''Takes a response dictionary, extracts tokens out of it, and updates token file.'''
    token['access_token'] = r['access_token']
    token['access_token_expiration'] = (datetime.now() +
                                        timedelta(seconds=int(r['expires_in']))).isoformat()
    if 'refresh_token' in r:
        token['refresh_token'] = r['refresh_token']
    writetokenfile()
    if args.verbose:
        print()
        print(f'NOTICE: Obtained new access token, expires {token["access_token_expiration"]}.')
        print()

def osc52_copy(text: str):
    """
    Copies a string to the system clipboard using the OSC52 escape sequence.
    """
    text_bytes = text.encode('utf-8')
    b64_bytes = base64.b64encode(text_bytes)
    b64_string = b64_bytes.decode('utf-8')

    # The OSC52 sequence
    sys.stdout.write(f"\033]52;c;{b64_string}\007")
    sys.stdout.flush()

def open_url(url: str):
    """
    Opens a URL in the system's default browser.
    Works on macOS, Linux, and Windows.
    """
    try:
        # new=2 opens the URL in a new tab, if possible
        success = webbrowser.open(url, new=2)

        if not success:
            print(f"Failed to open browser for: {url}", file=sys.stderr)
            return False
        return True

    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)
        return False

def extract_auth_code(s: str):
    """
    returns the auth code from a URI the string itself
    """
    if s.startswith("http"):
        decoded_s = urllib.parse.unquote(s)
        parsed_url = urllib.parse.urlparse(decoded_s)
        query_params = urllib.parse.parse_qs(parsed_url.query)
        code = query_params.get('code')
        if code:
            return code[0]  # Return the first instance of 'code'
    return s


if args.authorize:
    p = baseparams.copy()
    p['scope'] = registration['scope']

    if authflow in ('authcode', 'localhostauthcode'):
        verifier = secrets.token_urlsafe(90)
        challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest())[:-1]
        redirect_uri = registration['redirect_uri']
        if 'redirect_uri' in token: # Override if user specified
            redirect_uri = token['redirect_uri']
        listen_port = 0
        if authflow == 'localhostauthcode':
            # Find an available port to listen on
            s = socket.socket()
            s.bind(('127.0.0.1', 0))
            listen_port = s.getsockname()[1]
            s.close()
            redirect_uri = 'http://localhost:'+str(listen_port)+'/'
            # Probably should edit the port number into the actual redirect URI.

        p.update({'login_hint': token['email'],
                  'response_type': 'code',
                  'redirect_uri': redirect_uri,
                  'code_challenge': challenge,
                  'code_challenge_method': 'S256'})

        authorize_uri = registration["authorize_endpoint"] + '?' + \
              urllib.parse.urlencode(p, quote_via=urllib.parse.quote)
        open_url(authorize_uri)
        print()
        print("If your browser didn't open, visit the following URL (Ctrl-click in some terminals) to authenticate and authorize.")
        print()
        print('  '+authorize_uri)
        print()

        authcode = ''
        if authflow == 'authcode':
            print("NOTE: This will likely end with 'This site can’t be reached'.  That's good!")
            print("      Copy the URL from the address bar and paste it below.")
            print()
            user_input = input('URL (from address bar) or code > ')
            authcode = extract_auth_code(user_input)
        else:
            print("Starting a tiny web server to receive the authorization code...")
            print()

            class MyHandler(http.server.BaseHTTPRequestHandler):
                '''Handles the browser query resulting from redirect to redirect_uri.'''

                # pylint: disable=C0103
                def do_HEAD(self):
                    '''Response to a HEAD requests.'''
                    self.send_response(200)
                    self.send_header('Content-type', 'text/html')
                    self.end_headers()

                def do_GET(self):
                    '''For GET request, extract code parameter from URL.'''
                    # pylint: disable=W0603
                    global authcode
                    querystring = urllib.parse.urlparse(self.path).query
                    querydict = urllib.parse.parse_qs(querystring)
                    if 'code' in querydict:
                        authcode = querydict['code'][0]
                        self.do_HEAD()
                        self.wfile.write(b'<html>')
                        self.wfile.write(b'<head>')
                        self.wfile.write(b'<title>mutt_oauth2</title>')
                        self.wfile.write(b'<style>')
                        self.wfile.write(b'  body {')
                        self.wfile.write(b'    max-width: 100%;')
                        self.wfile.write(b'    margin: 30px;')
                        self.wfile.write(b'  }')
                        self.wfile.write(b'</style>')
                        self.wfile.write(b'</head>')
                        self.wfile.write(b'<body>')
                        self.wfile.write(b'<img src="https://neomutt.org/images/mutt-48x48.png">')
                        self.wfile.write(b'<h1>Success!!</h1>')
                        self.wfile.write(b'<p>mutt_oauth2.py has received the following authorization code from the provider.</p>')
                        self.wfile.write(b'<textarea cols=80 rows=20>'+authcode.encode('utf-8')+b'</textarea>')
                        self.wfile.write(b'<p>You may close this window.</p>')
                        self.wfile.write(b'</body></html>')
                    else:
                        self.do_HEAD()
                        self.wfile.write(b'<html>')
                        self.wfile.write(b'<head>')
                        self.wfile.write(b'<title>mutt_oauth2</title>')
                        self.wfile.write(b'<style>')
                        self.wfile.write(b'  body {')
                        self.wfile.write(b'    max-width: 100%;')
                        self.wfile.write(b'    margin: 30px;')
                        self.wfile.write(b'  }')
                        self.wfile.write(b'</style>')
                        self.wfile.write(b'</head>')
                        self.wfile.write(b'<body>')
                        self.wfile.write(b'<img src="https://neomutt.org/images/mutt-48x48.png">')
                        self.wfile.write(b'<h1>Error</h1>')
                        self.wfile.write(b'<p>mutt_oauth2.py received the following from the provider.</p>')
                        self.wfile.write(b'<dl>')
                        for key in querydict.keys():
                            values = querydict[key]
                            self.wfile.write(b'<dt>'+key.encode("utf-8")+b'</dt>')
                            self.wfile.write(b'<dd>')
                            for v in values:
                                self.wfile.write(v.encode('utf-8')+b'<br>')
                            self.wfile.write(b'</dd>')
                        self.wfile.write(b'</dl>')
                        self.wfile.write(b'</body>')
                        self.wfile.write(b'</html>')

                        #http://localhost:60041/?error=invalid_request&error_description=The%20request%20is%20not%20valid%20for%20the%20application%27s%20%27userAudience%27%20configuration.%20In%20order%20to%20use%20/common/%20endpoint%2c%20the%20application%20must%20not%20be%20configured%20with%20%27Consumer%27%20as%20the%20user%20audience.%20The%20userAudience%20should%20be%20configured%20with%20%27All%27%20to%20use%20/common/%20endpoint.&sermeta=0x80049DCF%7c0x0%7cm

            with http.server.HTTPServer(('127.0.0.1', listen_port), MyHandler) as httpd:
                try:
                    httpd.handle_request()
                except KeyboardInterrupt:
                    pass

        print()

        if not authcode:
            sys.exit('Did not obtain an authcode.')

        for k in 'response_type', 'login_hint', 'code_challenge', 'code_challenge_method':
            del p[k]
        p.update({'grant_type': 'authorization_code',
                  'code': authcode,
                  'client_secret': token['client_secret'],
                  'code_verifier': verifier})
        print('Exchanging the authorization code for an access token')
        try:
            response = urllib.request.urlopen(registration['token_endpoint'],
                                              urllib.parse.urlencode(p).encode())
        except urllib.error.HTTPError as err:
            print(err.code, err.reason)
            response = err
        response = response.read()
        if args.debug:
            print(response)
        response = json.loads(response)
        if 'error' in response:
            print(response['error'])
            if 'error_description' in response:
                print(response['error_description'])
            sys.exit(1)

    elif authflow == 'devicecode':
        try:
            response = urllib.request.urlopen(registration['devicecode_endpoint'],
                                              urllib.parse.urlencode(p).encode())
        except urllib.error.HTTPError as err:
            print(err.code, err.reason)
            response = err
        response = response.read()
        if args.debug:
            print(response)
        response = json.loads(response)
        if 'error' in response:
            print(response['error'])
            if 'error_description' in response:
                print(response['error_description'])
            sys.exit(1)

        print("Copying user_code ("+response['user_code']+") to the clipboard via OSC52")
        osc52_copy(response['user_code'])

        open_url(response['verification_uri'])

        print(response['message'])
        del p['scope']
        p.update({'grant_type': 'urn:ietf:params:oauth:grant-type:device_code',
                  'client_secret': token['client_secret'],
                  'device_code': response['device_code']})
        interval = int(response['interval'])
        print('Polling...', end='', flush=True)
        while True:
            time.sleep(interval)
            print('.', end='', flush=True)
            try:
                response = urllib.request.urlopen(registration['token_endpoint'],
                                                  urllib.parse.urlencode(p).encode())
            except urllib.error.HTTPError as err:
                # Not actually always an error, might just mean "keep trying..."
                response = err
            response = response.read()
            if args.debug:
                print(response)
            response = json.loads(response)
            if 'error' not in response:
                break
            if response['error'] == 'authorization_declined':
                print(' user declined authorization.')
                sys.exit(1)
            if response['error'] == 'expired_token':
                print(' too much time has elapsed.')
                sys.exit(1)
            if response['error'] != 'authorization_pending':
                print(response['error'])
                if 'error_description' in response:
                    print(response['error_description'])
                sys.exit(1)
        print()

    else:
        sys.exit(f'ERROR: Unknown OAuth2 flow "{token["authflow"]}. Delete token file and '
                 f'start over.')

    update_tokens(response)


if not access_token_valid():
    if args.verbose:
        print('NOTICE: Invalid or expired access token; using refresh token '
              'to obtain new access token.')
    if not token['refresh_token']:
        sys.exit('ERROR: No refresh token. Run script with "--authorize".')
    p = baseparams.copy()
    p.update({'client_id': token['client_id'],
              'client_secret': token['client_secret'],
              'refresh_token': token['refresh_token'],
              'grant_type': 'refresh_token'})
    try:
        response = urllib.request.urlopen(registration['token_endpoint'],
                                          urllib.parse.urlencode(p).encode())
    except urllib.error.HTTPError as err:
        print(err.code, err.reason)
        response = err
    response = response.read()
    if args.debug:
        print(response)
    response = json.loads(response)
    if 'error' in response:
        print(response['error'])
        if 'error_description' in response:
            print(response['error_description'])
        print('Perhaps refresh token invalid. Try running once with "--authorize"')
        sys.exit(1)
    update_tokens(response)


if not access_token_valid():
    sys.exit('ERROR: No valid access token. This should not be able to happen.')


def build_sasl_string(protocol, user=None):
    '''Build appropriate SASL string, which depends on cloud server's supported SASL method and used protocol.'''
    user = user or token['email']
    bearer_token = token['access_token']
    if protocol == 'imap':
        host, port = registration['imap_endpoint'], 993
    elif protocol == 'pop':
        host, port = registration['pop_endpoint'], 995
    elif protocol == 'smtp':
        # SMTP_SSL would be simpler but Microsoft does not answer on port 465.
        host, port = registration['smtp_endpoint'], 587
    else:
        sys.exit(f'Unknown protocol {protocol}')

    if registration['sasl_method'] == 'OAUTHBEARER':
        return f'n,a={user},\1host={host}\1port={port}\1auth=Bearer {bearer_token}\1\1'
    if registration['sasl_method'] == 'XOAUTH2':
        return f'user={user}\1auth=Bearer {bearer_token}\1\1'
    sys.exit(f'Unknown SASL method {registration["sasl_method"]}.')


if args.format == 'msasl':
    if args.verbose:
        print('SASL Method and String: ', end='')
    print(registration['sasl_method'], base64.standard_b64encode(build_sasl_string(args.protocol, args.email).encode()).decode())
elif args.format == 'sasl':
    if args.verbose:
        print('SASL String: ', end='')
    print(base64.standard_b64encode(build_sasl_string(args.protocol, args.email).encode()).decode())
else:
    if args.verbose:
        print('Access Token: ', end='')
    print(token['access_token'])


if args.test:
    errors = False

    imap_conn = imaplib.IMAP4_SSL(registration['imap_endpoint'])
    sasl_string = build_sasl_string('imap')
    if args.debug:
        imap_conn.debug = 4
    try:
        imap_conn.authenticate(registration['sasl_method'], lambda _: sasl_string.encode())
        # Microsoft has a bug wherein a mismatch between username and token can still report a
        # successful login... (Try a consumer login with the token from a work/school account.)
        # Fortunately subsequent commands fail with an error. Thus we follow AUTH with another
        # IMAP command before reporting success.
        imap_conn.list()
        if args.verbose:
            print('IMAP authentication succeeded')
    except imaplib.IMAP4.error as e:
        print('IMAP authentication FAILED (does your account allow IMAP?):', e)
        errors = True

    pop_conn = poplib.POP3_SSL(registration['pop_endpoint'])
    sasl_string = build_sasl_string('pop')
    if args.debug:
        pop_conn.set_debuglevel(2)
    try:
        # poplib doesn't have an auth command taking an authenticator object
        # Microsoft requires a two-line SASL for POP
        # pylint: disable=W0212
        pop_conn._shortcmd('AUTH ' + registration['sasl_method'])
        pop_conn._shortcmd(base64.standard_b64encode(sasl_string.encode()).decode())
        if args.verbose:
            print('POP authentication succeeded')
    except poplib.error_proto as e:
        print('POP authentication FAILED (does your account allow POP?):', e.args[0].decode())
        errors = True

    smtp_conn = smtplib.SMTP(registration['smtp_endpoint'], 587)
    sasl_string = build_sasl_string('smtp')
    smtp_conn.ehlo('test')
    smtp_conn.starttls()
    smtp_conn.ehlo('test')
    if args.debug:
        smtp_conn.set_debuglevel(2)
    try:
        smtp_conn.auth(registration['sasl_method'], lambda _=None: sasl_string)
        if args.verbose:
            print('SMTP authentication succeeded')
    except smtplib.SMTPAuthenticationError as e:
        print('SMTP authentication FAILED:', e)
        errors = True

    if errors:
        sys.exit(1)
