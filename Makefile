.PHONY: build test clean

CC := aarch64-linux-gnu-gcc
OUT := build/hello-aarch64

build:
	mkdir -p build
	$(CC) -static -O2 -Wall -Wextra \
		-o $(OUT) runtime/hello.c

test: build
	qemu-aarch64 ./$(OUT)

clean:
	rm -rf build
