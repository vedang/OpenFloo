#include "user_communicate.h"
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

int open_communication_module()
{
    int fd = open("hello", O_RDWR);
    return fd;
}

int shared_comm_bind(int dev, unsigned long handle)
{
    return(ioctl(dev, BIND, &handle));
}

int shared_comm_connect(int dev, unsigned long handle)
{
    return(ioctl(dev, CONNECT, &handle));
}

int shared_comm_send(int dev, unsigned long handle, char *buff, int len, int flags)
{
    struct shared_comm_message msg;
    msg.handle = handle;
    msg.buffer = buff;
    msg.len = len;
    return(ioctl(dev, WRITER, &msg));
}

int shared_comm_recv(int dev, unsigned long handle, char *buff, int len, int flags)
{
    int ret;
    struct shared_comm_message msg;
    msg.handle = handle;
    ret = ioctl(dev, READER, &msg);
    memcpy(buff, msg.buffer, len);
}

void shared_comm_close(int dev)
{
    close(dev);
}
