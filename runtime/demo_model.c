// SPDX-License-Identifier: Apache-2.0

#include "observability.h"

#include <stdio.h>
#include <unistd.h>

int main(void)
{
    tear_event("model_init");

    printf("TEAR model: loading model metadata\n");
    printf("TEAR model: backend=mock\n");

    sleep(1);

    tear_event("inference_start");

    printf("TEAR model: input=synthetic-frame\n");

    sleep(1);

    printf("TEAR model: running inference\n");

    sleep(1);

    tear_event("inference_done");

    printf("TEAR model: result=object:box confidence=0.87\n");

    tear_event("model_shutdown");

    return 0;
}
