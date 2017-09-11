#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>

#define		HELLO_MAJOR 	231
#define 	DEVICE_NAME		"HelloModule"

#define 	COMMAND1 		1
#define 	COMMAND2 		2

static struct cdev hello_cdev;
static dev_t dev;

static int hello_open(struct inode *inode,struct file *file)
{
	printk(KERN_EMERG "hello open.\n");
	return 0;
}

static int hello_write(struct file *file,const char __user *buf,size_t count,loff_t *ppos)
{
	printk(KERN_EMERG "hello write.\n");
	return 0;
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
	cdev_init(&hello_cdev,&hello_flops);

	/*add cdev*/
	ret = cdev_add(&hello_cdev,dev,1);
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
	cdev_del(&hello_cdev);
	unregister_chrdev_region(dev,1);
	printk(KERN_EMERG DEVICE_NAME " removed.\n");
}

MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);