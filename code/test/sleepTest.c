#include "syscall.h"



int 
main()
{
	int i = 0;
	for(i = 5; i < 20; i++)
	{
		Sleep(100000);
		Example(i);
		
	}
}
