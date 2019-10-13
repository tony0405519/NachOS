#include "syscall.h"



int 
main()
{
	int i = 0;
	for(i = 0; i < 5; i++)
		Example(i);
	Sleep(5000);
	for(i = 5; i < 10; i++)
		Example(i);
}
