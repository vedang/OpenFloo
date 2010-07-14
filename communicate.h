#include <asm-i386/page.h>

#define BIND 0
#define PRINT 1
#define UNREGISTER 2
#define CONNECT 3
#define READER 4
#define WRITER 5

#define CO_BUFF_SIZE (PAGE_SIZE-2*sizeof(unsigned long))

struct co_shared_page_structure
{
	unsigned long start, end;
	unsigned char buffer[CO_BUFF_SIZE];
};

struct shared_comm_message
{
    unsigned long handle;
    int len;
    char *buffer;
};

/*extern int open_communication_module();
extern int shared_comm_bind();
extern int shared_comm_send(unsigned long handle, char *buff, int len, int flags);
extern int shared_comm_recv(unsigned long handle, char *buff, int len, int flags);
extern void shared_comm_close();*/
