#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

OUT="${1:-build/kernel/Image}"
URL="https://deb.debian.org/debian/dists/bookworm/main/installer-arm64/current/images/netboot/debian-installer/arm64/linux"

mkdir -p "$(dirname "$OUT")"

if [ -f "$OUT" ]; then
    echo "Kernel already exists: $OUT"
    exit 0
fi

echo "Fetching QEMU ARM64 kernel..."
echo "URL: $URL"

wget "$URL" -O "$OUT"

echo "Fetched:"
file "$OUT"
