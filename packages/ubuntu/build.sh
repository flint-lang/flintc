#!/usr/bin/env sh
set -e

# This script needs to be run from within the package using `../build.sh`
# inside the Docker container.
#
# Builds an UNSIGNED source package (-us -uc). Signing happens separately on
# the host (where the GPG key + passphrase live) via `../sign.sh`, because the
# container has no access to the (passphrase-protected) signing key.
debuild -S -sa -us -uc
