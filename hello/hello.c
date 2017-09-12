#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/mm.h>

#define		HELLO_MAJOR 	231
#define 	DEVICE_NAME		"HelloModule"
#define		HELLO_SIZE 		0x1000

#define 	COMMAND1 		1
#define 	COMMAND2 		2

static dev_t dev;
struct hello_device{
	struct cdev m_cdev;
	unsigned char mem[HELLO_SIZE];
};
static struct hello_device hello_dev;

static int hello_open(struct inode *inode,struct file *file)
{
	printk(KERN_EMERG "hello open.\n");
	file->private_data = &hello_dev;
	return 0;
}

static ssize_t hello_write(struct file *file,const char __user *buff,size_t count,loff_t *ppos)
{
	printk(KERN_EMERG "hello write.\n");
	int ret = 0;
	struct hello_device *dev = file->private_data;
	unsigned long p = *ppos;

	if(p >= HELLO_SIZE)
	{
		printk(KERN_EMERG "The offset is out of bounds!");
		return 0;
	}
	if(count > HELLO_SIZE - p)
		count = HELLO_SIZE - p;
	if(copy_from_user(dev->mem + p, buff, count))
		ret = - EFAULT;
	else
	{
		ppos += count;
		ret = count;
		printk(KERN_EMERG "written %u bytes from lu\n",count, p);
		printk(KERN_EMERG "There is kernel data = %s\n",buff);
	}
	return 0;
}

static ssize_t hello_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	unsigned long p = *offp;
	int ret = 0;
	printk(KERN_EMERG "hello read.\n");
	struct hello_device *dev = filp->private_data;
	dev->mem[0] = 'D';
	dev->mem[1] = 'J';
	if(p >= HELLO_SIZE)
	{
		printk(KERN_EMERG "The offset is out of bounds!");
		return 0;
	}
	if(count > HELLO_SIZE - p)
		count = HELLO_SIZE - p;
	if(copy_to_user(buff,(void*)(dev->mem + p),count))
		ret = - EFAULT;
	else
	{
		*offp += count;
		ret = count;
		printk(KERN_EMERG "read %d byte(s) form %d\n",count,p);
	}
	return ret;
}

static int hello_release(struct inode *inode,struct flie *flip)
{
	printk(KERN_EMERG "I execute release here!\n");
}

static int hello_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd){
	case COMMAND1:
		printk(KERN_EMERG "ioctl command 1 successfully\n");
		break;
	case COMMAND2:
		printk(KERN_EMERG "ioctl command 2 successfully\n");
		break;
	default:
		printk(KERN_EMERG "ioctl error,unknow ioctl cmd!\n");
		return -EFAULT;
	}
	return 0;
}

static struct file_operations hello_flops = {
	.owner = THIS_MODULE,/* 这是一个宏，推向编译模块时自动创建的__this_module变量 */
	.open  = hello_open,
	.read  = hello_read,
	.write = hello_write,
	.release  = hello_release,
	.unlocked_ioctl = hello_ioctl,
};

static int __init hello_init(void)
{
	int ret;

	//ret = register_chrdev(HELLO_MAJOR,DEVICE_NAME,&hello_flops);
	/*Dynamic registration of device number*/
	ret = alloc_chrdev_region(&dev,0,1,DEVICE_NAME);
	if(ret < 0)
	{
		printk(KERN_EMERG DEVICE_NAME "can't register major number.\n");
		return ret;
	}
	printk(KERN_EMERG DEVICE_NAME "initialized.\n");
	printk(KERN_EMERG "major = %d\n",MAJOR(dev));
	printk(KERN_EMERG "minor = %d\n",MINOR(dev));

	/*init cdev*/
	cdev_init(&hello_dev.m_cdev,&hello_flops);

	/*add cdev*/
	ret = cdev_add(&hello_dev.m_cdev,dev,1);
	if(ret < 0)
	{
		printk(KERN_EMERG "cdev failed!\n");
		return ret;
	}
	return 0;
}

static void __exit hello_exit(void)
{
	/*remove cdev*/
	cdev_del(&hello_dev.m_cdev);
	unregister_chrdev_region(dev,1);
	printk(KERN_EMERG DEVICE_NAME " removed.\n");
}

MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);