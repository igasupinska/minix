#!/bin/bash
qemu-img create -f qcow2 -o backing_file=backup_minix.img minix_clean.img

qemu-system-x86_64 -curses -drive file=minix_clean.img -enable-kvm -localtime -net user,hostfwd=tcp::10023-:22 -net nic,model=virtio -m 1024M
