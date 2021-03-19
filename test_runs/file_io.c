#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main () {
	int num;
	FILE *fptr_out;
	
	fptr_out = fopen("out.txt", "w");

	if(fptr_out == NULL) {
		printf("Error while opening file ptr\n");
		exit(1);
	}
	
	scanf("%d", &num);
	printf("%d\n", num);
	while (num != 0) {
		fprintf(fptr_out, "%d\n", num);
		scanf("%d", &num);
		printf("%d\n", num);
	}
	
	fclose(fptr_out);
	return 0;
}
