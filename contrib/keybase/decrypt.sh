#! /usr/bin/bash
sed -n 's/^.*BEGIN KEYBASE/BEGIN KEYBASE/p' | keybase decrypt
