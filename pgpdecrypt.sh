#!/bin/sh
sed -n '/BEGIN/,$p' | keybase pgp decrypt
