#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>

int main(void)
{
    printf("TEAR: init starting\n");
    fflush(stdout);

    execl("/bin/tear-supervisor", "/bin/tear-supervisor", NULL);

    perror("exec /bin/tear-supervisor failed");

    sync();
    reboot(RB_POWER_OFF);

    return 127;
}
