#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

#define SCHED_WRR 7

int main(int argc, char *argv[])
{

	int pid, policy;
	struct sched_param param = {};
	pid = atoi(argv[1]);
	
	policy = sched_getscheduler(pid);
	printf("Process with PID %d has policy %d\n", pid, policy);
	
	if (policy != SCHED_WRR)
		sched_setscheduler(pid, SCHED_WRR, &param);

	policy = sched_getscheduler(pid);
        printf("Process with PID %d has policy %d\n", pid, policy);

	sleep(10);
	policy = sched_getscheduler(pid);
        printf("Process with PID %d has policy %d\n", pid, policy);

	return 0;
}
