#!/usr/bin/env bash
set -euo pipefail

OPTEE_QEMU_DIR="${1:-external/optee-qemu-v8}"
TARGET="$OPTEE_QEMU_DIR/out-br/target"

mkdir -p "$TARGET/bin" "$TARGET/etc/tear"

cp -v build/tear-supervisor "$TARGET/bin/"
cp -v build/tear-trustd "$TARGET/bin/"
cp -v build/tearictl "$TARGET/bin/"
cp -v build/demo-model "$TARGET/bin/"
cp -v build/tear-runtime-manager "$TARGET/bin/"

cp -v examples/model-v1.json "$TARGET/etc/tear/"
cp -v examples/model-v2.json "$TARGET/etc/tear/"

cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh

echo "TEAR_OPTEE_QEMU_TEST start"

/bin/tear-supervisor --workload /bin/demo-model
rc=$?

echo "TEAR_OPTEE_QEMU_TEST exit=$rc"

poweroff -f
EOS

chmod +x "$TARGET/etc/init.d/S99tear-test"
