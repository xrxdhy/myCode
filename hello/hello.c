#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define		HELLO_MAJOR 	231
#define 	DEVICE_NAME		"HelloModule"

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

static struct file_operations hello_flops = {
	.owner = THIS_MODULE,
	.open  = hello_open,
	.write = hello_write,
	.open  = hello_release,
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
	unregister_chrdev(HELLO_MAJOR,DEVICE_NAME);
	printk(KERN_EMERG DEVICE_NAME " removed.\n");
}

MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);