#!/bin/sh
gawk '/BEGIN/{y=1}y' | keybase pgp verify
