#define BIND 0
#define PRINT 1
#define UNREGISTER 2
#define CONNECT 3
#define READER 4
#define WRITER 5

struct shared_comm_message
{
    unsigned long handle;
    int len;
    char *buffer;
};

extern int open_communication_module();
extern int shared_comm_bind(int dev, unsigned long handle);
extern int shared_comm_connect(int dev, unsigned long handle);
extern int shared_comm_send(int dev, unsigned long handle, char *buff, int len, int flags);
extern int shared_comm_recv(int dev, unsigned long handle, char *buff, int len, int flags);
extern void shared_comm_close(int dev);
