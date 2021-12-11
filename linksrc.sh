#!/bin/bash

set -xe

GIT_DIR=~/Documents/GreenBox/xmhf64
BUILD_DIR="$(realpath "$1")"
[ -d "$BUILD_DIR" ]

if ! mountpoint "$BUILD_DIR"; then
	sudo mount -t tmpfs tmpfs -o uid=$USER,gid=$USER "$BUILD_DIR"
fi

cd "$GIT_DIR"
find * -type d -exec mkdir -p "${BUILD_DIR}"/{} \;
find * -type f -exec ln -sf "${GIT_DIR}"/{} "${BUILD_DIR}"/{} \;
echo SUCCESS

