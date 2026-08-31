#!/usr/bin/env bash
# Helper script to prepare a Debian source package for flintc
# from the prebuilt binaries published on GitHub Releases.
#
# Usage:
#   ./prepare-source.sh <version>
# Example:
#   ./prepare-source.sh 0.4.1

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <version>"
  echo "Example: $0 0.4.1"
  exit 1
fi

VERSION="$1"
PKG_NAME="flintc"
RELEASE_TAG="v${VERSION}-core"
BASE_URL="https://github.com/flint-lang/flintc/releases/download/${RELEASE_TAG}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SCRIPT_DIR}"
SRC_DIR="${WORK_DIR}/${PKG_NAME}-${VERSION}"

echo "==> Preparing source package for ${PKG_NAME} ${VERSION}"

# Clean previous attempt
rm -rf "${SRC_DIR}"
mkdir -p "${SRC_DIR}"

echo "==> Downloading prebuilt binaries..."
curl -L --fail -o "${SRC_DIR}/flintc" "${BASE_URL}/flintc"
curl -L --fail -o "${SRC_DIR}/fls"    "${BASE_URL}/fls"
chmod +x "${SRC_DIR}/flintc" "${SRC_DIR}/fls"

echo "==> Copying debian/ packaging files..."
cp -a "${SCRIPT_DIR}/debian" "${SRC_DIR}/"

echo "==> Creating orig tarball..."
cd "${WORK_DIR}"
tar --exclude=debian -czf "${PKG_NAME}_${VERSION}.orig.tar.gz" -C "${SRC_DIR}" .

echo "==> Source tree is ready: ${SRC_DIR}"
echo ""
echo "Next steps:"
echo "  1. Build (unsigned) inside the Docker container:"
echo "     cd ${SCRIPT_DIR} && nix-shell"
echo "     cd flintc-${VERSION} && ../build.sh"
echo "  2. Sign on the HOST (has your GPG key + passphrase):"
echo "     cd ${SCRIPT_DIR} && ./sign.sh ${VERSION}"
echo "  3. Upload inside the Docker container:"
echo "     ./publish.sh ${VERSION}"
