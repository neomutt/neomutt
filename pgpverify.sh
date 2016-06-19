#! /usr/bin/bash
gawk '/BEGIN/{y=1}y' | keybase pgp verify
