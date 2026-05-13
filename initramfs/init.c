// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/mount.h>

#define CMDLINE_PATH "/proc/cmdline"
#define WORKLOAD_KEY "tear.workload="
#define DEFAULT_WORKLOAD "/bin/tear-hello"
#define MAX_CMDLINE 4096
#define MAX_WORKLOAD 256

static const char *get_workload(char *workload, size_t workload_len)
{
    FILE *f = fopen(CMDLINE_PATH, "r");
    char cmdline[MAX_CMDLINE];

    printf("TEAR init: reading %s\n", CMDLINE_PATH);
    if (!f)
        return DEFAULT_WORKLOAD;

    if (!fgets(cmdline, sizeof(cmdline), f)) {
        fclose(f);
        return DEFAULT_WORKLOAD;
    }

    fclose(f);

    printf("TEAR init: cmdline: %s\n", cmdline);

    char *key = strstr(cmdline, WORKLOAD_KEY);
    if (!key) {
        printf("TEAR init: no %s found, using default\n", WORKLOAD_KEY);
        return DEFAULT_WORKLOAD;
    }

    key += strlen(WORKLOAD_KEY);

    size_t len = strcspn(key, " \n");
    if (len == 0 || len >= workload_len)
        return DEFAULT_WORKLOAD;

    memcpy(workload, key, len);
    workload[len] = '\0';

    printf("TEAR init: parsed workload: %s\n", workload);
    return workload;
}

int main(void)
{
    if (mount("proc", "/proc", "proc", 0, NULL) < 0)
        perror("mount /proc");

    char workload[MAX_WORKLOAD];

    const char *selected_workload =
        get_workload(workload, sizeof(workload));

    printf("TEAR: init starting\n");
    printf("TEAR: selected workload: %s\n", selected_workload);
    fflush(stdout);

    execl("/bin/tear-supervisor",
          "/bin/tear-supervisor",
          "--workload",
          selected_workload,
          NULL);

    perror("exec /bin/tear-supervisor failed");

    sync();
    reboot(RB_POWER_OFF);

    return 127;
}
