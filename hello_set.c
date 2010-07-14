#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cooperative_internal.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/list.h>

#include "communicate.h"
#include <linux/co_shared_comm.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/gfp.h>
void (*prev_wakeup_proc)(void);

int Major;
static int Device_open = 0;
struct task_struct *sleeping_task;
void shared_comm_print(void);
void wakeup_sleeping_proc(void);

struct alloc_handle_list
{
	struct list_head ptr;
	unsigned long handle;
	struct task_struct *communicating_process;
	//shared page
        struct co_shared_page_structure *shared_page;
};

struct alloc_handle_list *communicating_proc_list_head;

struct alloc_handle_list *shared_comm_find(unsigned long handle)
{
    struct list_head *iterator;
    struct alloc_handle_list *temp;

    list_for_each(iterator, &communicating_proc_list_head->ptr){
	temp = list_entry(iterator, struct alloc_handle_list, ptr);
	printk("In find handle=%lu \n",temp->handle);
	if(temp->handle == handle) 
	    return temp;
    }
    printk ("error! no such handle!!\n");
    return NULL; //remember to CHANGE THIS WITH APPROP error
}


static int device_open(struct inode *inode, struct file *file)
{
	printk("In open - set\n");
	Device_open++;
	try_module_get(THIS_MODULE);
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    struct list_head *iterator, *safe_temp;
    struct alloc_handle_list *connection_node;
    unsigned long flags;
    unsigned long handle;
	

    list_for_each_safe(iterator, safe_temp, &communicating_proc_list_head->ptr){
	connection_node = list_entry(iterator, struct alloc_handle_list, ptr);
	
	if(connection_node->communicating_process->pid != current->pid) continue; //do not mess with other processes
	list_del(&connection_node->ptr);
	printk ("in dev_release unregistering handle: %lu pid: %lu\n", connection_node->handle, (unsigned long)connection_node->communicating_process->pid);
	handle=connection_node->handle;

	/*co_free_pages is called so that the page is not cached.. 
	 caching of _shared_ pages can lead to _disasters_*/
	co_free_pages(connection_node->shared_page, 1);
	free_pages(connection_node->shared_page, 0);
	kfree(connection_node);

	co_passage_page_assert_valid();
	co_passage_page_acquire(&flags);
	co_passage_page->operation=CO_OPERATION_UNREGISTER_HANDLE;
	co_passage_page->params[0]=handle;
	co_switch_wrapper();

	if(co_passage_page->params[0]==0) { //error 
	    co_passage_page_release(flags);
	    printk("Did not unreg Handle (no such handle)!!\n");
	    continue;
	}
	co_passage_page_release(flags);
	
    }
 	Device_open--;
	module_put(THIS_MODULE);
	return 0;
}

int shared_comm_bind(unsigned long handle)
{
    struct alloc_handle_list *temp;
    unsigned long flags;
        pid_t pid;


    printk("In shared_comm_bind...");
    printk("Handle : %lu\n", handle);

    co_passage_page_assert_valid();
    co_passage_page_acquire(&flags);
    co_passage_page->operation=CO_OPERATION_BIND;
    co_passage_page->params[0]=handle;
    co_switch_wrapper();
    if(co_passage_page->params[0]==0) {
	co_passage_page_release(flags);
	printk("Duplicate Handle or Host out of memory\n");
	return -1;
    }
    co_passage_page_release(flags);
    
    temp = (struct alloc_handle_list *)kmalloc(sizeof(struct alloc_handle_list), GFP_KERNEL);
    if(temp==NULL || communicating_proc_list_head==NULL){
	printk("Unable to kmalloc\n");
	return -1;
    }
    temp->handle = handle;
    temp->communicating_process = current;
    temp->shared_page = co_shared_comm_get_free_pages(GFP_KERNEL | __GFP_SHARED_COMM_SERVER, 0, handle);
    memcpy(temp->shared_page->buffer, "Hellu", 6);
    printk("About to add\n");
    list_add_tail(&(temp->ptr), &(communicating_proc_list_head->ptr));
    printk("Added.. and going to sleep waiting for connections\n");
    //get the pid of current process
    pid=current->pid;
    sleeping_task=current;
    printk("Going to sleep proc = %lu \n", (unsigned long)pid);
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();

    return 0;
}

int shared_comm_unregister(unsigned long handle)
{
    struct list_head *iterator, *safe_temp;
    struct alloc_handle_list *connection_node;
    unsigned long flags;

    printk("Unregistering..............%lu\n", handle);

    list_for_each_safe(iterator, safe_temp, &communicating_proc_list_head->ptr){
	connection_node = list_entry(iterator, struct alloc_handle_list, ptr);
	if(connection_node->handle != handle) continue; //not the reqd handle
	if(connection_node->communicating_process->pid != current->pid) continue; //do not mess with other processes
	list_del(&connection_node->ptr);
	printk ("unregistering handle: %lu pid: %lu\n", connection_node->handle, (unsigned long)connection_node->communicating_process->pid);
	co_free_pages(connection_node->shared_page, 1);
	free_pages(connection_node->shared_page, 0);
	kfree(connection_node);

	co_passage_page_assert_valid();
	co_passage_page_acquire(&flags);
	co_passage_page->operation=CO_OPERATION_UNREGISTER_HANDLE;
	co_passage_page->params[0]=handle;
	co_switch_wrapper();

	if(co_passage_page->params[0]==0) { //error 
	    co_passage_page_release(flags);
	    printk("Did not unreg Handle (no such handle)!!\n");
	    return -1;
	}
	co_passage_page_release(flags);
	
	
 	return 0;
    }
	return -1;
}

int shared_comm_connect(unsigned long handle)
{
    struct alloc_handle_list *temp;
    unsigned long flags;

	co_passage_page_assert_valid();
	co_passage_page_acquire(&flags);
	co_passage_page->operation=CO_OPERATION_CONNECT;
	co_passage_page->params[0]=handle;
	co_switch_wrapper();

	if(co_passage_page->params[0]==0) { //error 
	    co_passage_page_release(flags);
	    printk("Could not connect!!");
	    return -1;
	}
	else{
	    co_passage_page_release(flags);
	    temp = (struct alloc_handle_list *)kmalloc(sizeof(struct alloc_handle_list), GFP_KERNEL);
	    if(temp==NULL || communicating_proc_list_head==NULL){
		printk("Unable to kmalloc\n");
		return -1;
	    }
	    temp->handle = handle;
	    temp->communicating_process = current;
	    temp->shared_page = co_shared_comm_get_free_pages(GFP_KERNEL | __GFP_SHARED_COMM_CLIENT, 0, handle);
	    
	    printk("About to add, in connect: %s\n", temp->shared_page->buffer);
	    list_add_tail(&(temp->ptr), &(communicating_proc_list_head->ptr));
	    printk("Connected..........");
	    return 0;
	}
}

void shared_comm_print()
{
    struct list_head *iterator, *safe_temp;
    struct alloc_handle_list *connection_node;
    list_for_each_safe(iterator, safe_temp, &communicating_proc_list_head->ptr){
	connection_node = list_entry(iterator, struct alloc_handle_list, ptr);
	printk ("handle: %lu pid: %lu\n", connection_node->handle, (unsigned long)connection_node->communicating_process->pid);
    } 
}

int isfree(struct co_shared_page_structure *shared_page, int len)
{
    int start = shared_page->start;
    int end = shared_page->end;

    if(start > end){
	if(start-end-1 >= len) return 1;
	else return 0;
    } else{
	if(CO_BUFF_SIZE - 1 - (end-start) >= len) return 1;
	else return 0;
    }

}

void shared_comm_wakeup(unsigned long handle)
{
        unsigned long flags;

	co_passage_page_assert_valid();
	co_passage_page_acquire(&flags);
	co_passage_page->operation=CO_OPERATION_WAKEUP;
	co_passage_page->params[0]=handle;
	co_switch_wrapper();
	co_passage_page_release(flags);

}

void shared_comm_write_to_page(struct alloc_handle_list *temp, char *kmsg, int msg_len)
{

    memcpy(&temp->shared_page->buffer[temp->shared_page->end],kmsg,msg_len);
    temp->shared_page->end+=msg_len;
/*
    while(msg_len > 0){
	temp->shared_page->buffer[temp->shared_page->end] = *kmsg;
	kmsg++;
	temp->shared_page->end = (temp->shared_page->end + 1)% CO_BUFF_SIZE;
	msg_len--;
    }
*/
}


unsigned long shared_comm_write(struct shared_comm_message *msg)
{
    struct alloc_handle_list *temp;
    int msg_len;
    char *kmsg=kmalloc(msg->len, GFP_KERNEL);

    if(copy_from_user(kmsg, msg->buffer, msg->len))
	return -1;

    temp = shared_comm_find(msg->handle);
    if(temp==NULL){
	printk("in write, No such handle\n");
	return -1;
    }
    while(!isfree(temp->shared_page, msg->len+sizeof(int))) {
	printk("Page is full\n");
	set_current_state(TASK_INTERRUPTIBLE);
	schedule();
    }
    msg_len = msg->len;
    shared_comm_write_to_page(temp, &msg_len, sizeof(int));
    shared_comm_write_to_page(temp, kmsg, msg_len);
    shared_comm_wakeup(msg->handle);
    printk("Written... \n");
}


void shared_comm_read_from_page(struct alloc_handle_list *temp, char *kmsg, int msg_len)
{
    while(msg_len > 0) {
	*kmsg = temp->shared_page->buffer[temp->shared_page->start];
	kmsg++;
	temp->shared_page->start = (temp->shared_page->start + 1) % CO_BUFF_SIZE;
	msg_len--;
    }
}


unsigned long shared_comm_read(struct shared_comm_message *msg)
{
    struct alloc_handle_list *temp;
    char *kmsg;
    int msg_len;

    temp = shared_comm_find(msg->handle);
    if(temp==NULL){
	printk("in read, No such handle\n");
	return -1;
    }
    if(temp->shared_page->start == temp->shared_page->end) {
	printk("No data written in page\n");
	set_current_state(TASK_INTERRUPTIBLE);
	schedule();
    }
    
	shared_comm_read_from_page(temp, &msg_len, sizeof(int));
	kmsg = kmalloc(msg_len, GFP_KERNEL);
	shared_comm_read_from_page(temp, kmsg, msg_len);
	if(copy_to_user(msg->buffer, kmsg, msg_len))
	    return -1;
	
	shared_comm_wakeup(msg->handle);
	printk("Read data : %s %lu\n", kmsg, msg->handle);
   
}

int device_ioctl(struct inode *inode, struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    unsigned long handle;
    int ret;

    switch(ioctl_num)
    {
	case BIND:
	    handle = *((unsigned long *)ioctl_param);
	    ret = shared_comm_bind(handle);
	    if(ret) return ret;
	    else return 0;
 
	case PRINT:
	    shared_comm_print();
	    break;

	case UNREGISTER:
	    handle = *((unsigned long *)ioctl_param);
	    ret = shared_comm_unregister(handle);
	    if(ret) return ret;
	    else return 0;
	    break;

	case CONNECT:
	    handle=*((unsigned long *)ioctl_param);
	    ret=shared_comm_connect(handle);
	    if(ret) return ret;
	    else return 0;
	    break;

	case READER:
	    handle=*((unsigned long *)ioctl_param);
	    shared_comm_read((struct shared_comm_message *)ioctl_param);
	    printk("in read, Handle = %lu\n", handle);
	    return 0;
	    
	case WRITER:
	    handle=*((unsigned long *)ioctl_param);
	    shared_comm_write((struct shared_comm_message *)ioctl_param);
	    printk("in write, Handle = %lu\n", handle);
	    return 0;
	    
	default:
	    printk("something wrong\n");
	    break;
    }
    return 0;
}


static ssize_t device_read(struct file *file, char __user *user, size_t count, loff_t *offset)
{
return 0;
}

static ssize_t device_write(struct file *file, const char __user *user, size_t size, loff_t *offset)
{
return 0;
}


static struct file_operations fops =
{
	.open = device_open,
	.release = device_release,
	.ioctl = device_ioctl,
	.read = device_read,
	.write = device_write,
};

int init_module(void)
{
	Major=register_chrdev(253,"abc" , &fops);
	if(Major < 0)
	{
		printk("Registering failed !!\n");
		return Major;
	}	
	printk("Reading cruel world!!\n");
		communicating_proc_list_head = (struct alloc_handle_list *)kmalloc (sizeof(struct alloc_handle_list), GFP_KERNEL);
	if(communicating_proc_list_head==NULL)
	{
	    printk("Unable to allocate head");
	    return 1;
	    }
	INIT_LIST_HEAD(&communicating_proc_list_head->ptr);
	if(communicating_proc_list_head->ptr.prev==NULL || communicating_proc_list_head->ptr.next==NULL)
	{
	    printk("Not initialised");
	    return -1;
	    }
	prev_wakeup_proc = comm_fops.wakeup_proc;
	comm_fops.wakeup_proc = wakeup_sleeping_proc;

	return 0;
}

void cleanup_module(void)
{
	int ret;
	ret = unregister_chrdev(253, "abc");
	if(ret<0)
		printk("Unregistering failed !!\n");
	printk("Done reading!!\n");
	comm_fops.wakeup_proc = prev_wakeup_proc;

//free linked list
}


void wakeup_sleeping_proc(void)
{
    struct list_head *iterator, *safe_temp;
    struct alloc_handle_list *connection_node;
    int num_of_proc;
    unsigned long handle, flags;
    
    co_passage_page_assert_valid();
    co_passage_page_acquire(&flags);

    num_of_proc = co_passage_page->handles[0];
    while(num_of_proc>0){
	handle=co_passage_page->handles[num_of_proc];
	list_for_each_safe(iterator, safe_temp, &communicating_proc_list_head->ptr){
	    connection_node = list_entry(iterator, struct alloc_handle_list, ptr);
	    if(connection_node->handle==handle)
		wake_up_process(connection_node->communicating_process);
	}
	num_of_proc--;
    }
    co_passage_page->handles[0]=0;

    if(co_passage_page->shared_comm_flags & 0x2) {
	co_passage_page->shared_comm_flags &= ~(0x2);
	co_passage_page->operation=CO_OPERATION_GET_WAKEUP_LIST;
	co_switch_wrapper();
	num_of_proc=(int)co_passage_page->params[0];
	while(num_of_proc>0){
	    handle=co_passage_page->params[num_of_proc];
	    list_for_each_safe(iterator, safe_temp, &communicating_proc_list_head->ptr){
		connection_node = list_entry(iterator, struct alloc_handle_list, ptr);
		if(connection_node->handle==handle)
		    wake_up_process(connection_node->communicating_process);
	    }
	    num_of_proc--;
	}
    }
    co_passage_page_release(flags);
	
}
