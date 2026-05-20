// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

#ifdef TEAR_HOST_BUILD
#define GREETING "TEAR: hello from native host"
#else
#define GREETING "TEAR: hello from aarch64 qemu"
#endif

int main(void)
{
    puts(GREETING);
    return 0;
}
