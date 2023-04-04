#!/bin/bash

BASEDIR=$(dirname "$0")
cd $BASEDIR

# Usage: ./compile-mount-and-copy.sh file1 file2 ...
# If no arguments are given, fallback is rotd, professor and student
if [ $# -eq 0 ]; then
    files=("rotd" "professor" "student")
else
    files=("$@")
fi

make clean
make "${files[@]}"
mkdir ./mntdir
sudo mount ../tizen-image/rootfs.img ./mntdir
sudo cp "${files[@]}" ./mntdir/root

sudo umount ./mntdir
rmdir ./mntdir