#!/bin/sh
set -e

echo meson running...
meson --prefix=/usr --sysconfdir=/etc --buildtype=debugoptimized build

echo ninja running...
ninja -v -C build

echo ninja install running...
DESTDIR=staging ninja -v -C build install
