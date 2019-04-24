#!/bin/bash
qemu-system-x86_64 -curses -drive file=minix.img -enable-kvm -localtime -net user,hostfwd=tcp::10022-:22 -net nic,model=virtio -m 1024M