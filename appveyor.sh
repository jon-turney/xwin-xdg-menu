#!/bin/sh
set -e

echo autoreconf running...
autoreconf -fvi

echo configure running...
./configure --prefix=/usr

echo make running...
make

echo make install running...
make install DESTDIR=./staging

echo make distcheck running...
make distcheck
