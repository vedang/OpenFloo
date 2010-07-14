#include<stdio.h>
#include<fcntl.h>
#include<sys/types.h>
#include "user_communicate.h"

int main()
{
    char buffer[4096]="Hello";
    int dev;
    unsigned long handle=2;

    dev=open_communication_module();
    if(dev==-1)
    {
	printf("Error opening device");
	exit(0);
    }
    shared_comm_bind(dev, handle);
	
	shared_comm_recv(dev, handle, buffer, 4096, 0);
	//shared_comm_send(dev, handle, buffer, 1, 0);
    return(0);
}
