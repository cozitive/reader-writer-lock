#!/bin/bash

BASEDIR=$(dirname "$0")
cd $BASEDIR

sudo ./build-rpi3.sh
# sudo ./scripts/mkbootimg_rpi3.sh
# cp modules.img boot.img ./tizen-image/
sudo ./setup-images.sh
./install-tmux.sh
./test/compile-mount-and-copy.sh
