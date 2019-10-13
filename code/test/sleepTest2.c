#include "syscall.h"



int 
main()
{
	int i = 0;
	for(i = 5; i < 10; i++)
	{
		Sleep(1000000);
		Example(i);
		
	}
}
