#!/bin/bash

BASEDIR=$(dirname "$0")
cd $BASEDIR

sudo ./build-rpi3.sh
sudo ./setup-images.sh
./install-tmux.sh
./test/compile-mount-and-copy.sh