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
  file \
  repo \
  git \
  curl \
  wget \
  ca-certificates \
  ninja-build \
  meson \
  cmake \
  pkg-config \
  python3 \
  python3-pip \
  python3-pyelftools \
  python3-cryptography \
  device-tree-compiler \
  bison \
  flex \
  libssl-dev \
  libgnutls28-dev \
  libglib2.0-dev \
  libpixman-1-dev \
  libslirp-dev \
  cpio \
  rsync \
  bc \
  unzip \
  xz-utils
