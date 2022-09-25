#!/bin/bash -eux
# -e: Exit immediately if a command exits with a non-zero status.
# -u: Treat unset variables as an error when substituting.
# -x: Display expanded script commands

DOWNLOAD_DIR=web196@server43.webgo24.de:/home/www/download/mint
SYSROOT_DIR=${SYSROOT_DIR:-"/"}

sudo mkdir -p "${SYSROOT_DIR}"

for package in $*
do
	scp -o "StrictHostKeyChecking no" "$DOWNLOAD_DIR/${package}-*mint-*dev*" .
	f=`ls ${package}-*`
	if test -f "$f"; then
		sudo tar -C "${SYSROOT_DIR}" -xf "$f"
		rm -f $f
	fi
done
