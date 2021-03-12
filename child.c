#include<stdio.h>
#include<stdlib.h>

int main()
{
	sleep(15);
	printf("entering fork call \n");

	if(fork())
	{
		while(1)
		{
			sleep(3);
			printf("hello from parent\n");
		}
	}
	
	else
	{
		while(1)
		{
			sleep(3);
			printf("hello from child\n");
		}
	}


	return 0;
}
