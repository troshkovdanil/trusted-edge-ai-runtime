CC := aarch64-linux-gnu-gcc

BUILD := build
HELLO := $(BUILD)/hello-aarch64
INITRAMFS := $(BUILD)/initramfs.cpio.gz
INIT := $(BUILD)/init-aarch64
KERNEL := $(BUILD)/kernel/Image

.PHONY: build test initramfs qemu-system clean kernel-image

build:
	mkdir -p $(BUILD)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(HELLO) runtime/hello.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(INIT) initramfs/init.c
test: build
	qemu-aarch64 ./$(HELLO)

initramfs: build
	rm -rf $(BUILD)/rootfs
	mkdir -p $(BUILD)/rootfs/bin
	cp $(HELLO) $(BUILD)/rootfs/bin/tear-hello
	cp $(INIT) $(BUILD)/rootfs/init
	chmod +x $(BUILD)/rootfs/init
	cd $(BUILD)/rootfs && \
		find . | cpio -H newc -o | gzip > ../initramfs.cpio.gz

kernel-image:
	./scripts/fetch-qemu-kernel.sh $(KERNEL)

qemu-system: kernel-image initramfs
	./scripts/run-qemu-system.sh

clean:
	rm -rf $(BUILD)
