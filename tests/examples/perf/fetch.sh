#!/usr/bin/env sh

ROOT="$(cd "$(dirname "$0")" && pwd)"

N=3
GITHUB_API="https://api.github.com"
OWNER="flint-lang"
REPO="flintc"

readarray -t RELEASES < <(curl -s "${GITHUB_API}/repos/${OWNER}/${REPO}/releases?per_page=${N}" \
	| jq -r '.[]
      | {tag: .tag_name, asset: (.assets[] | select(.name=="flintc")?)}
      | select(.asset != null)
      | "\(.tag),\(.asset.browser_download_url),\(.asset.digest)"')

mkdir -p "$ROOT/bin"
for RELEASE in "${RELEASES[@]}"; do
	TAG="$(echo "$RELEASE" | cut -d',' -f1)"
	echo "$TAG"
	OUT="$ROOT/bin/flintc-$TAG"
	if [ -e "$OUT" ]; then
		continue
	fi

	LINK="$(echo "$RELEASE" | cut -d',' -f2)"
	DIGEST="$(echo "$RELEASE" | cut -d',' -f3)"
	HASH="${DIGEST#sha256:}"

	echo "Downloading -> $OUT"
	curl -L --fail --show-error --silent "$LINK" -o "$OUT"

	BIN_HASH="$(sha256sum "$OUT" | cut -d' ' -f1)"
	if [ "$BIN_HASH" != "$HASH" ]; then
		echo "SHA256 mismatch for $OUT"
		echo " expected: $EXPECTED"
		echo " actual:   $ACTUAL"
		rm -f "$OUT"
		exit 1
	fi

	chmod +x "$OUT"
done
