#!/bin/bash
# Reverse overlay.sh

set -xe

sudo umount "$1/work"
sudo umount "$1"
echo SUCCESS

