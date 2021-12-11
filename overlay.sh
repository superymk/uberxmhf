#!/bin/bash
# Create overlay over xmhf source code

set -xe

GIT_DIR=~/Documents/GreenBox/xmhf64
BUILD_DIR="$1"
[ -d "$BUILD_DIR" ]

sudo mount -t tmpfs tmpfs "$BUILD_DIR"

MOUNT_OPTS="-o"
MOUNT_OPTS="${MOUNT_OPTS}lowerdir=$GIT_DIR"
MOUNT_OPTS="${MOUNT_OPTS},upperdir=$BUILD_DIR/diff"
MOUNT_OPTS="${MOUNT_OPTS},workdir=$BUILD_DIR/diff_work"

# Not a good choice, because overlayfs has undefined behavior when lower changes
# https://www.kernel.org/doc/html/latest/filesystems/overlayfs.html

if [ -d "$BUILD_DIR/work" ]; then
	# Refresh mount point
	MOUNT_OPTS="${MOUNT_OPTS},remount"
else
	mkdir "$BUILD_DIR/diff_work"
	mkdir "$BUILD_DIR/diff"
	mkdir "$BUILD_DIR/work"
fi

sudo mount -t overlay overlay "$MOUNT_OPTS" "$BUILD_DIR/work"
echo SUCCESS

