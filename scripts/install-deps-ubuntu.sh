#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

sudo apt update
sudo apt install -y \
  build-essential \
  gcc-aarch64-linux-gnu \
  qemu-system-arm \
  qemu-user \
  make \
  file
