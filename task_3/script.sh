#!/bin/sh

make && make install -C /usr/src/minix/servers/pm
make && make install -C /usr/src/lib/libc
make hdboot -C /usr/src/releasetools
reboot