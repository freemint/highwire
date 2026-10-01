#!/bin/sh -eu
# Stage the archive that "make dist" just built under the names the release
# carries.  The product is HighWire whatever the repository is called, so the
# names are fixed here rather than taken from $PROJECT_NAME: the readme links
# to highwire-latest.zip, and a download URL resolves by asset name, so that
# name must not follow a repository rename.
#
# The dated copy accumulates on the release, one per day -- a second build on
# the same day replaces it.  The workflow uploads whatever lands here rather
# than naming the files again, so the two cannot drift apart.

BUILT="build/hw`date +%y%m%d`.zip"
OUT="${DEPLOY_DIR:-/tmp/highwire-deploy}"

if [ ! -f "$BUILT" ]; then
	echo "package.sh: $BUILT not found -- did make dist run?" >&2
	exit 1
fi

mkdir -p "$OUT"
cp "$BUILT" "$OUT/highwire-${PROJECT_VERSION}-`date +%Y%m%d`.zip"
cp "$BUILT" "$OUT/highwire-latest.zip"
ls -l "$OUT"
