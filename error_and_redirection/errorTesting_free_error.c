#include <stdio.h>

void releaseFunction(char * pointer)
{
	free(pointer);
}

int main(int argc, char const* argv[])
{
	unsigned int index;
	char nullPointer[5];

	for (index = 0; 3 > index; ++index) 
	{
		printf("sleep %d second\n", 3 - index);
		sleep(1);
	}

	releaseFunction(nullPointer);


	
	return 0;
}
