#!/usr/bin/env python3

import os
import re
import sys
import subprocess
from argparse import ArgumentParser

SECURITY_PROG = "/usr/bin/security"

def emit(msg, error = False, quit = False):
    msgType = "Error" if error else "Warning"
    sys.stderr.write(msgType + ": " + msg + "\n")
    if( quit ):
        sys.exit(1)

def getKeychainData(user, host, protocol):
    """Get data from the Keychain data structure.

    user  account name to query in Keychain (-a flag in security program)
    host  server name to query in Keychain (-s flag in security program)
    protcol protocol name. Used for generating the server name query string,
          as 'protocol'://'host'.
    """
    params = {
        "security": SECURITY_PROG,
        "server": protocol + "://" + host,
        "account": user,
        "keychain": os.path.expanduser("~/Library/Keychains/login.keychain")
        }

    cmdStr = "{security} find-internet-password -g -a {account} -s {server} "+\
             "{keychain}"
    cmd = cmdStr.format(**params)
    try:
        output = subprocess.check_output(cmd, shell=True,
                                         stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError:
        return None

    output = output.decode("utf-8")
    match = re.search('password: "(.*)"', output)
    if( match ):
        return {"username": user,
                "password": match.group(1),
                "login": user}
    else:
        return None

KNOWN_PROTOCOLS = ["imap", "imaps", "pop", "pops", "smtp", "smtps", "nntp",
                   "nntps"]

ap = ArgumentParser(description="NeoMutt script for Mac OSX to retrieve " +
  "account credentials from the Keychain using `security` program.")
ap.add_argument("--hostname", metavar = "host", required = True,
                action = "store", help = "hostname associated with account.")
ap.add_argument("--username", metavar = "user", required = True,
                action = "store", help = "username associated with account.")
ap.add_argument("--type", metavar = "t", required = True,
                action = "store", help = "protocol type. Any of the " +
                "following: " + ", ".join(KNOWN_PROTOCOLS) + ".")

todo = ap.parse_args()

if( todo.type not in KNOWN_PROTOCOLS ):
    emit("Unknown protcol type: `" + todo.type + "'.", error = True,
         quit = True)

if( not (os.path.isfile(SECURITY_PROG) or os.path.islink(SECURITY_PROG)) ):
    emit("{} program not found.".format(SECURITY_PROG), error = True,
         quit = True)
if( not os.access(SECURITY_PROG, os.X_OK) ):
    emit("{} is not executable.".format(SECURITY_PROG), error = True,
         quit = True)

credentials = getKeychainData(todo.username, todo.hostname, todo.type)
if( credentials is not None ):
    print("""username: {username}
login: {login}
password: {password}""".format(**credentials))
else:
    emit("No suitable data found in Keychain.", quit = True, error = True)
