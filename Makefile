CC := aarch64-linux-gnu-gcc

BUILD := build
HELLO := $(BUILD)/hello-aarch64
INITRAMFS := $(BUILD)/initramfs.cpio.gz

.PHONY: build test initramfs clean

build:
	mkdir -p $(BUILD)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(HELLO) runtime/hello.c

test: build
	qemu-aarch64 ./$(HELLO)

initramfs: build
	rm -rf $(BUILD)/rootfs
	mkdir -p $(BUILD)/rootfs/bin
	cp $(HELLO) $(BUILD)/rootfs/bin/tear-hello
	cp initramfs/init $(BUILD)/rootfs/init
	chmod +x $(BUILD)/rootfs/init
	cd $(BUILD)/rootfs && \
		find . | cpio -H newc -o | gzip > ../initramfs.cpio.gz

clean:
	rm -rf $(BUILD)
