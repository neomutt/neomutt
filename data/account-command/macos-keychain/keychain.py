#!/usr/bin/env python3

import os
import re
import sys
import asyncio
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
    protocol protocol name. Used for generating the server name query string,
          as 'protocol'://'host'.
    """
    keychain = os.path.expanduser("~/Library/Keychains/login.keychain")
    server = protocol + "://" + host
    cmd = [SECURITY_PROG, "find-internet-password", "-g",
           "-a", user, "-s", server, keychain]
    try:
        async def _run_cmd():
            proc = await asyncio.create_subprocess_exec(
                *cmd, stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.STDOUT)
            stdout, _ = await proc.communicate()
            if proc.returncode != 0:
                raise RuntimeError("command failed")
            return stdout
        output = asyncio.run(_run_cmd())
    except Exception:
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
    emit("Unknown protocol type: `" + todo.type + "'.", error = True,
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
