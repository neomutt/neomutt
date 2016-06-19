#! /usr/bin/env python
# Written by Joshua Jordi

import os

def helpfunc():
    print("Run keybase commands here as if you were using keybase. WARNING: this program is not capable of MIME formatting. Only inline.")
    print("To encrypt, use keybase syntax. (ie. 'keybase encrypt jakkinstewart' or 'keybase pgp encrypt jakkinstewart') Do not include a '-i' or '-o'. This script uses them in the background. Including either flag will mess with the script. (Unless that's what you want to do.)")
    print("Don't worry about finding or attaching the file, the macro will take care of that.")
    print("To sign, give it the style ('sign' or 'pgp sign'. It will automatically include the signature in the file.")
    print("This program will not be able to decrypt or verify messages. I've created separate scripts for that.")
    print('Type "quit" to quit.')

def encrypt(parameters):
    os.system('pwd > .file')
    directory = open('.file', 'r')
    pwd = directory.read().strip('\n')
    directory.close()
    tmp = open("%s/.mutt/keybaseMutt/.tmp" % pwd, "r")
    tmp = tmp.read().strip("\n")
    print("Working....")
    os.system('%s -i %s -o %s' % (parameters, tmp, tmp))
    print("Done!")

def sign(parameters):
    os.system('pwd > .file')
    directory = open('.file', 'r')
    pwd = directory.read().strip('\n')
    directory.close()
    tmp = open('%s/.mutt/keybaseMutt/.tmp' % pwd, 'r')
    tmp = tmp.read().strip('\n')
    print("Working...")
    os.system('%s -i %s -o /tmp/signature.asc && cat /tmp/signature.asc > %s && rm /tmp/signature.asc' % (parameters, tmp, tmp))
    print("Done!")

exitVar = ''

print("Type help to learn how to use me.")

while exitVar.lower() != 'quit':
    inputStuffs = input()
    if (inputStuffs.lower() == 'help'):
        helpfunc()

    elif ('encrypt' in inputStuffs):
        encrypt(inputStuffs)

    elif ('decrypt' in inputStuffs):
        decrypt(inputStuffs)

    elif ('quit' in inputStuffs):
        exitVar = 'quit'

    else:
        print("You didn't use a known keybase command.")

os.system('rm .file')
