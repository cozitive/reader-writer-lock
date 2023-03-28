#!/bin/bash

BASEDIR=$(dirname "$0")
cd $BASEDIR

make clean
make
mkdir ./mntdir
sudo mount ../tizen-image/rootfs.img ./mntdir
sudo cp professor student ./mntdir/root
sudo umount ./mntdir
rmdir ./mntdir