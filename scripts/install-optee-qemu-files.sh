#!/usr/bin/env bash
set -euo pipefail

OPTEE_QEMU_DIR="${1:-external/optee-qemu-v8}"
TARGET="$OPTEE_QEMU_DIR/out-br/target"

TEAR_TA_UUID="7c9d7b3a-2f4e-4c8f-9a11-6b4454454152"
TEAR_TA="build/optee/tear_ta/${TEAR_TA_UUID}.ta"
TEAR_CA="build/optee/tear-optee-ca"

mkdir -p "$TARGET/bin" "$TARGET/etc/tear" "$TARGET/lib/optee_armtz"

make optee-ta
make optee-ca

cp -v "$TEAR_TA" "$TARGET/lib/optee_armtz/"
cp -v "$TEAR_CA" "$TARGET/bin/tear-optee-ca"

cp -v build/tear-supervisor "$TARGET/bin/"
cp -v build/tear-trustd "$TARGET/bin/"
cp -v build/tearictl "$TARGET/bin/"
cp -v build/demo-model "$TARGET/bin/"
cp -v build/tear-runtime-manager "$TARGET/bin/"

cp -v examples/model-v1.json "$TARGET/etc/tear/"
cp -v examples/model-v2.json "$TARGET/etc/tear/"

cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh

echo "TEAR_OPTEE_CA_TEST start"
/bin/tear-optee-ca || exit 1
echo "TEAR_OPTEE_CA_TEST done"

echo "TEAR_OPTEE_QEMU_TEST start"

/bin/tear-supervisor --workload /bin/demo-model
rc=$?

echo "TEAR_OPTEE_QEMU_TEST exit=$rc"

poweroff -f
EOS

chmod +x "$TARGET/etc/init.d/S99tear-test"
