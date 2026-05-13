#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    puts("TEAR: qemu-system guest booted");
    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0) {
        printf("TEAR: fork failed: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        execl("/bin/tear-hello", "tear-hello", NULL);
        printf("TEAR: exec failed: %s\n", strerror(errno));
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("TEAR: tear-hello failed: status=0x%x\n", status);
        return 1;
    }

    puts("TEAR: qemu-system smoke test passed");
    fflush(stdout);

    sync();
    reboot(RB_POWER_OFF);

    return 0;
}
