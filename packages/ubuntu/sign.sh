#!/usr/bin/env sh
set -e

# Signs the Debian source package on the HOST (where the real GPG key and
# passphrase live). Run this on Arch, NOT in the Docker container.
#
# The tricky part: `debuild` wrote the checksums in .changes for the UNSIGNED
# .dsc. Clear-signing the .dsc changes its bytes, so we must recompute the
# .dsc checksums (md5/sha1/sha256/size) in .changes to match, before
# clear-signing .changes. This is exactly what `debsign` does internally.

if [ $# -ne 1 ]; then
  echo "Usage: $0 <version>"
  echo "Example: $0 0.4.1"
  exit 1
fi

VERSION="$1"
KEY_EMAIL="${KEY_EMAIL:-marc.zweiler@outlook.at}"
DSC="flintc_${VERSION}-1.dsc"
CHANGES="flintc_${VERSION}-1_source.changes"

if [ ! -f "$DSC" ] || [ ! -f "$CHANGES" ]; then
  echo "ERROR: missing $DSC or $CHANGES in $(pwd)" >&2
  exit 1
fi

echo "==> Clear-signing $DSC"
gpg --clearsign -u "$KEY_EMAIL" "$DSC"
mv "${DSC}.asc" "$DSC"

SIZE=$(stat -c %s "$DSC")
MD5=$(md5sum "$DSC" | awk '{print $1}')
SHA1=$(sha1sum "$DSC" | awk '{print $1}')
SHA256=$(sha256sum "$DSC" | awk '{print $1}')

echo "==> Updating $DSC checksums in $CHANGES"
awk -v dsc="$DSC" -v size="$SIZE" -v md5="$MD5" -v sha1="$SHA1" -v sha256="$SHA256" '
  BEGIN { section = "" }
  /^Checksums-Sha1:/   { section = "sha1"; print; next }
  /^Checksums-Sha256:/ { section = "sha256"; print; next }
  /^Files:/            { section = "files"; print; next }
  /^[A-Za-z][A-Za-z-]*:/ { section = "" }
  {
    if (section != "" && $NF == dsc) {
      if (section == "sha1")   print "  " sha1 " " size " " dsc
      else if (section == "sha256") print "  " sha256 " " size " " dsc
      else print "  " md5 " " size " devel optional " dsc
      next
    }
    print
  }
' "$CHANGES" > "${CHANGES}.tmp"
mv "${CHANGES}.tmp" "$CHANGES"

echo "    updated $DSC: md5=${MD5} sha1=${SHA1} sha256=${SHA256} size=$SIZE"

echo "==> Clear-signing $CHANGES"
gpg --clearsign -u "$KEY_EMAIL" "$CHANGES"
mv "${CHANGES}.asc" "$CHANGES"

echo "==> Done. Signatures applied and $DSC checksums refreshed in $CHANGES."
