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
        int ret, j, k;

        sleep(1);
        printf("Entering fork call \n");

        if(fork()) {
                ret = wrr_set(4);
                if (ret < 0) {
                        fprintf(stderr, "error from wrr_set system call: %s\n",
                                strerror(errno));
                        exit(EXIT_FAILURE);
                }

                j = 0;
                while(1) {
                        for (long i = 0; i < 50000000; i++) {}
                        printf("hello from Parent %d\n", j);
                        j++;
			if (j == 100)
				break;
                }
        } else {

                if(fork()) {
                        ret = wrr_set(20);
                        if (ret < 0) {
                                fprintf(stderr, "error from wrr_set system call: %s\n",
                                        strerror(errno));
                                exit(EXIT_FAILURE);
                        }

                        j = 0;
                        while(1) {
                                for (long i = 0; i < 50000000; i++) {}
                                printf("hello from Child %d\n", j);
                                j++;
				if (j == 100)
					break;
                        }
                } else {
                        ret = wrr_set(100);
                        if (ret < 0) {
                                fprintf(stderr, "error from wrr_set system call: %s\n",
                                        strerror(errno));
                                exit(EXIT_FAILURE);
                        }

                        k = 0;
                        while(1) {
                                for (long i = 0; i < 50000000; i++) {}
                                printf("hello from Grandchild %d\n", k);
                                k++;
				if (k == 100)
					break;
                        }
                }
        }


        return 0;
}

