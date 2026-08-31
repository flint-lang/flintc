#!/usr/bin/env sh
set -e

if [ $# -ne 1 ]; then
  echo "Usage: $0 <version>"
  echo "Example: $0 0.4.1"
  exit 1
fi

# dput verifies the signatures on .changes/.dsc, which needs the PUBLIC key.
# Fetch it from a keyserver if not already present. The secret key/passphrase
# never enter the container; signing is done on the host via ../sign.sh.
KEY_EMAIL="${KEY_EMAIL:-marc.zweiler@outlook.at}"
KEY_ID="${KEY_ID:-5D51A11D0F30FD88D29173AC6D7CF0EA0ABD3370}"

export GNUPGHOME="${GNUPGHOME:-$HOME/.gnupg}"
mkdir -m 700 -p "$GNUPGHOME"

if ! gpg --list-keys "$KEY_EMAIL" >/dev/null 2>&1; then
  echo "Fetching public key $KEY_ID ..."
  gpg --keyserver keyserver.ubuntu.com --recv-keys "$KEY_ID"
fi

dput ppa:zweiler1/flint "flintc_$1-1_source.changes"
