#!/bin/bash
qemu-img create -f qcow2 -o backing_file=backup_minix.img minix_working.img

qemu-system-x86_64 -curses -drive file=minix_working.img -enable-kvm -localtime -net user,hostfwd=tcp::10022-:22 -net nic,model=virtio -m 1024M
