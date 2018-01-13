#include<linux/module.h>
#include<linux/types.h>
#include<linux/fs.h>
#include<linux/errno.h>
#include<linux/mm.h>
#include<linux/slab.h>
#include<linux/sched.h>
#include<linux/init.h>
// #include<asm/io.h>
// #include<asm/system.h>
// #include<asm/uaccess.h>
#include<linux/cdev.h>
#include<linux/semaphore.h>
#include<linux/poll.h>
#include <linux/device.h>


struct class *mymodule_class;

#define GLOBALFIFO_SIZE 0x1000
#define MEM_CLEAR 0x1
#define GLOBALFIFO_MAJOR 250

int globalfifo_major = GLOBALFIFO_MAJOR;

struct globalfifo_dev {
	struct cdev cdev;
	unsigned int current_len;
	unsigned char mem[GLOBALFIFO_SIZE];
	struct semaphore sem;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
	struct fasync_struct *async_queue;
};

struct globalfifo_dev *globalfifo_devp;
struct fasync_struct Temp_async_queue;

/* Declare functions*/
int 	globalfifo_open(struct inode *inode, struct file *filp);
int 	globalfifo_release(struct inode *inode ,struct file* filp);
long 	globalfifo_ioctl(struct file *filp ,unsigned int cmd , unsigned long arg);
static ssize_t globalfifo_read(struct file *filp, char __user *buf ,size_t size ,loff_t *ppos);
static ssize_t globalfifo_write(struct file *filp, const char __user *buf ,size_t size ,loff_t *ppos);
static 	loff_t globalfifo_llseek(struct file *filp, loff_t offset , int orig);
int 	__init globalfifo_init(void);
void 	__exit globalfifo_exit(void);
static unsigned int globalfifo_poll(struct file *filp, poll_table *wait);
static int globalfifo_fasync(int fd, struct file *filp, int mode);

static const struct file_operations globalfifo_fops = { 
/* Hooks file operations with functions*/
	.owner = THIS_MODULE,
	.llseek = globalfifo_llseek,
	.release = globalfifo_release,
	.unlocked_ioctl = globalfifo_ioctl,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.open = globalfifo_open,
	.poll = globalfifo_poll,
    .fasync = globalfifo_fasync,
};

static int globalfifo_fasync(int fd, struct file *filp, int mode)
{
	struct globalfifo_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

/* file open*/
int globalfifo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = globalfifo_devp;
	return 0;
}

/*file close*/
int globalfifo_release(struct inode *inode ,struct file* filp)
{
	globalfifo_fasync(-1, filp, 0);
	return 0;
}

long globalfifo_ioctl(struct file *filp, unsigned int cmd , unsigned long arg)
{
	struct globalfifo_dev *dev = filp->private_data;

	switch(cmd) {
	case MEM_CLEAR:
		memset(dev->mem, 0 , GLOBALFIFO_SIZE);
		printk(KERN_INFO "[GLOBALFIFO]: globalfifo is clear to zero\n");
		break;
	default:
		return -EINVAL; 
	}

	return 0;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf ,size_t count ,loff_t *ppos)
{
	int ret;
	struct globalfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait,current);
	
	down(&dev->sem);
	add_wait_queue(&dev->r_wait , &wait);
	
	while(dev->current_len == 0) {
		if(filp->f_flags &O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}
	
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&dev->sem);

		schedule();
		if(signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}
		down(&dev->sem);
	}
	
	if(count > dev->current_len)
		count = dev->current_len;
	if(copy_to_user(buf,dev->mem,count)) {
		return -EFAULT;
		goto out;
	}else {
		memcpy(dev->mem,dev->mem + count , dev->current_len - count);
		dev->current_len -= count;
		printk(KERN_INFO "[GLOBALFIFO]: read %d byte(s) ,current_len %d\n",count,dev->current_len);
		wake_up_interruptible(&dev->w_wait);
		ret = count;
	}
	out: up(&dev->sem);
	out2: remove_wait_queue(&dev->w_wait ,&wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buf ,size_t count ,loff_t *ppos)
{
	int ret;
	struct globalfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait,current);
	
	down(&dev->sem);
	add_wait_queue(&dev->w_wait , &wait);
	
	while(dev->current_len == GLOBALFIFO_SIZE) {

		if(filp->f_flags &O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}
	
	
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&dev->sem);

		schedule();
		if(signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}
		down(&dev->sem);
	}
	
	if(count > GLOBALFIFO_SIZE - dev->current_len)
		count = GLOBALFIFO_SIZE - dev->current_len;
	if(copy_from_user(dev->mem + dev->current_len ,buf, count)) {
		return -EFAULT;
		goto out;
	}else {
		dev->current_len += count;
		printk(KERN_INFO "[GLOBALFIFO]: write %d byte(s) ,current_len %d\n",count,dev->current_len);
		wake_up_interruptible(&dev->r_wait);
		
		printk("dev->async_queue : %p\n",dev->async_queue);
		if(dev->async_queue) 
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

		ret = count;
	}
	out:  up(&dev->sem);
	out2: remove_wait_queue(&dev->w_wait ,&wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static loff_t globalfifo_llseek(struct file *filp, loff_t offset , int orig)
{
	loff_t ret = 0;
	switch(orig) {
	case 0:
		if(offset < 0) {
			ret = -EINVAL;
			break;
		}
		if((unsigned int ) offset > GLOBALFIFO_SIZE) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int ) offset;
		ret = filp->f_pos;
		break;
	case 1:
		if((filp->f_pos + offset) > GLOBALFIFO_SIZE) {
			ret = -EINVAL;
			break;
		}
		
		if((filp->f_pos + offset) < 0) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos += (unsigned int ) offset;
		ret = filp->f_pos;
		break;
	default:
		ret = -EINVAL;
		break;
	} 
	return ret;
}

static void globalfifo_setup_cdev(struct globalfifo_dev *dev,int index)
{
	int err ,devno = MKDEV(globalfifo_major, index);
	
	cdev_init(&dev->cdev , &globalfifo_fops); 
	dev->cdev.owner = THIS_MODULE;
	err= cdev_add(&dev->cdev,devno,1);
	printk("[GLOBALFIFO]: char device globalfifo has regitered as %d\n",globalfifo_major);
	if(err)
		printk(KERN_NOTICE "[GLOBALFIFO]: Error %d adding globalfifo %d",err,devno);
}

static unsigned int globalfifo_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct globalfifo_dev *dev = filp->private_data;
	
	down(&dev->sem);
		
	poll_wait(filp ,&dev->r_wait, wait);
	poll_wait(filp ,&dev->w_wait, wait);
	
	if(dev->current_len != 0) {
		mask |= POLLIN | POLLRDNORM;
	}

	if(dev->current_len != GLOBALFIFO_SIZE) {
		mask |= POLLOUT | POLLWRNORM;
	}
	
	up(&dev->sem);

	return mask;
}

int __init globalfifo_init(void)
{
	int result ;
	dev_t devno = MKDEV(globalfifo_major,0);
	
	if(globalfifo_major) 
		result = register_chrdev_region(devno,1,"globalfifo");
	else {
		result = alloc_chrdev_region(&devno,0,1,"globalfifo");
		globalfifo_major = MAJOR(devno); 
	}

	if(result < 0 )
		return result;
	
	globalfifo_devp = kmalloc(sizeof(struct globalfifo_dev),GFP_KERNEL);
	
	if(!globalfifo_devp) {
		result = -ENOMEM;
		goto fail_malloc;
	}
	
	memset(globalfifo_devp,0,sizeof(struct globalfifo_dev));
	
	globalfifo_setup_cdev(globalfifo_devp,0);
	
	globalfifo_devp->async_queue = (struct fasync_struct*)&Temp_async_queue;
    printk("globalfifo_devp->async_queue : %p\n",globalfifo_devp->async_queue);
	
	// 自动生成文件节点
	mymodule_class = class_create(THIS_MODULE, "global_fifo");
	device_create(mymodule_class, NULL, MKDEV(globalfifo_major, 0), NULL, "globalfifo");
	
	sema_init(&globalfifo_devp->sem,1);
	init_waitqueue_head(&globalfifo_devp->r_wait);
	init_waitqueue_head(&globalfifo_devp->w_wait);
	
	return 0;

fail_malloc:
	unregister_chrdev_region(devno,1);
	return result;
}

void globalfifo_exit(void)
{
	cdev_del(&globalfifo_devp->cdev);
	// 卸载时卸载文件节点
	device_destroy(mymodule_class, MKDEV(globalfifo_major, 0));   
    class_destroy(mymodule_class); 
	kfree(globalfifo_devp);
	unregister_chrdev_region(MKDEV(globalfifo_major,0),1);
}

MODULE_LICENSE("Dual BSD/GPL");
module_param(globalfifo_major, int ,S_IRUGO);
module_init(globalfifo_init);
module_exit(globalfifo_exit);
