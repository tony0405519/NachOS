#include "syscall.h"



int 
main()
{
	int i = 0;
	Sleep(10);
	
	for(i = 0; i < 5; i++)
		Example(i);
	
	for(i = 5; i < 10; i++)
	{
		Example(i);
		Sleep(20);
	}
}
