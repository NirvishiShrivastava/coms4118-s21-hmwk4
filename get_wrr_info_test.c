#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define __NR_get_wrr_info 436
#define __NR_set_wrr_weight 437
#define MAX_CPUS 8

struct wrr_info {
	int num_cpus;          			/* The name of the process */
	int nr_running[MAX_CPUS];              	/* The pid of the process */
	int total_weight[MAX_CPUS];             /* The state of the process */
};

/* use SYSCALL function */
int wrr_get(struct wrr_info *buf)
{
	return syscall(__NR_get_wrr_info, buf);
}

int wrr_set(int weight)
{
	return syscall(__NR_set_wrr_weight, weight);
}

int main(int argc, char *argv[])
{
	struct wrr_info buf;
	int ret,i;

	printf("Calling wrr_get system call\n");
	ret = wrr_get(&buf);
	if (ret < 0) {
		fprintf(stderr, "error from wrr_get system call: %s\n",
				strerror(errno));
		printf("Enable System Call Failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("Number of CPUs returned by system call is %d\n", ret);

	/* Printing wrr_info details*/

	for (i = 0; i < ret; i++) { 
		printf("NR_RUNNING for CPU-%d = %d\n",i, buf.nr_running[i]);
		printf("TOTAL_WEIGHT for CPU-%d = %d\n",i, buf.total_weight[i]);
		printf("\n");
	}

	return 0;
}
