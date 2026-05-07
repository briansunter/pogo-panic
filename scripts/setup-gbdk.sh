#!/usr/bin/env bash
set -euo pipefail

VERSION="4.5.0"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS_DIR="$ROOT_DIR/.tools"
CACHE_DIR="$TOOLS_DIR/cache"
INSTALL_DIR="$TOOLS_DIR/gbdk"

os="$(uname -s)"
arch="$(uname -m)"
asset=""
sha=""

case "$os:$arch" in
  Darwin:arm64)
    asset="gbdk-macos-arm64.tar.gz"
    sha="289ee60e46c5a2785a21e35533f84a5131ed4a063b21b0dbdedc9a10af15bf78"
    ;;
  Darwin:x86_64)
    asset="gbdk-macos.tar.gz"
    sha="1aa549d12032d8f6509d11923bb28b1a453098f42597feb378e9a42541f8fd89"
    ;;
  Linux:arm64|Linux:aarch64)
    asset="gbdk-linux-arm64.tar.gz"
    sha="31eb2235f0fdb60163d0b1e9574a022098d6069cd56606a1daca4478a46e0439"
    ;;
  Linux:x86_64)
    asset="gbdk-linux64.tar.gz"
    sha="d7857a5f6d135ee4c249043ca26aad9f2ec8ab5d4106d97720d404114f42605c"
    ;;
  *)
    echo "Unsupported host for automatic GBDK setup: $os $arch" >&2
    exit 1
    ;;
esac

archive="$CACHE_DIR/$asset"
url="https://github.com/gbdk-2020/gbdk-2020/releases/download/$VERSION/$asset"

mkdir -p "$CACHE_DIR" "$TOOLS_DIR"

if [[ ! -f "$archive" ]]; then
  echo "Downloading GBDK $VERSION: $asset"
  curl -L --fail --output "$archive" "$url"
fi

actual="$(sha256sum "$archive" | awk '{print $1}')"
if [[ "$actual" != "$sha" ]]; then
  echo "Checksum mismatch for $asset" >&2
  echo "Expected: $sha" >&2
  echo "Actual:   $actual" >&2
  exit 1
fi

rm -rf "$INSTALL_DIR" "$TOOLS_DIR/gbdk-extract"
mkdir -p "$TOOLS_DIR/gbdk-extract"
tar -xzf "$archive" -C "$TOOLS_DIR/gbdk-extract"

lcc_path="$(find "$TOOLS_DIR/gbdk-extract" -type f -path "*/bin/lcc" -print -quit)"
if [[ -z "$lcc_path" ]]; then
  echo "Downloaded archive did not contain bin/lcc" >&2
  exit 1
fi

gbdk_root="$(dirname "$(dirname "$lcc_path")")"
mv "$gbdk_root" "$INSTALL_DIR"
rm -rf "$TOOLS_DIR/gbdk-extract"

if command -v xattr >/dev/null 2>&1; then
  xattr -dr com.apple.quarantine "$INSTALL_DIR" 2>/dev/null || true
fi

"$INSTALL_DIR/bin/lcc" -v >/dev/null
echo "GBDK installed at $INSTALL_DIR"
