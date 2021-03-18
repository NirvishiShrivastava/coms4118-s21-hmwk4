#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define __NR_get_wrr_info 436
#define __NR_set_wrr_weight 437

int wrr_set(int weight)
{
	return syscall(__NR_set_wrr_weight, weight);
}

int main()
{
        int ret;
        printf("Entering fork call \n");

        if(fork()) {
                ret = wrr_set(4);
                if (ret < 0) {
                        fprintf(stderr,
				"error from wrr_set system call: %s\n",
                                strerror(errno));
                        exit(EXIT_FAILURE);
                }
                while(1) {}
        } else {
                if(fork()) {
                        ret = wrr_set(20);
                        if (ret < 0) {
                                fprintf(stderr,
					"error from wrr_set system call: %s\n",
                                        strerror(errno));
                                exit(EXIT_FAILURE);
                        }
			while (1) {}
                } else {
                        ret = wrr_set(100);
                        if (ret < 0) {
                                fprintf(stderr,
					"error from wrr_set system call: %s\n",
                                        strerror(errno));
                                exit(EXIT_FAILURE);
                        }
			while (1) {}
		} 
        }

        return 0;
}

