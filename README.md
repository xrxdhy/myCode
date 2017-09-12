## 驱动

### 初始化
	static int __init initialization_function(void)
	{
		/* Initialization code here */
	}
	module_init(initialization_function);

* 初始化函数应当声明为静态的。
* `__init`标志：给定函数只用于初始化函数，模块加载之后会释放这部分内存
* `module_init`：这个宏定义增加了特别的段到模块目标代码中，表明在哪里找到模块的初始化函数。

### 卸载
	static void __exit cleanup_function(void)
	{
		/* Cleanup code here */
	}
	module_exit(cleanup_function);
* `__exit`标志：修饰代码只用于模块卸载
*  如果模块没有定义清理函数，内核不会允许它被卸载。

### 预备

	/*常用头文件*/
	#include <linux/module.h>
	#include <linux/init.h>
        
    /*许可申明*/
	MODULE_LICENSE("GPL");
        
### 设备编号
	MAJOR(dev_t dev);
	MINOR(dev_t dev);
有主次编号, 需要将其转换为一个 dev_t, 使用: 

	MKDEV(int major, int minor);
#### 设备号的分配与释放
	#include <linux/fs.h>
    int register_chrdev_region(dev_t first, unsigned int count, char *name);
* `first` 是你要分配的起始设备编号
* `count` 是你请求的连续设备编号的总数.
* `name` 是连接到这个编号的设备的名子  


	int alloc_chrdev_region(dev_t *dev, unsigned int firstminor, unsigned int count, char *name);
	
* 动态分配编号
* `dev`:输出型参数，获得分配到的设备号。
* `firstminor`:从第几个次设备号开始分配
* `count`：次设备号个数
* `name`：驱动名  

> 动态分配的缺点是你无法提前创建设备节点, 因为分配给你的模块的主编号会变化. 对于驱动的正常使用, 这不是问题, 因为一旦编号分配了, 你可从`/proc/devices`中读取它.

	
    int register_chrdev
    (unsignedintmajor,constchar*name,structfile_operat ons*fops);
	register_chrdev  
    #include <linux.fs.h>
	int unregister_chrdev (unsigned int major, const char *name)
比较老的内核注册的形式，早期的驱动。  
其中参数major如果等于0，则表示采用系统`动态分配`的主设备号；不为0，则表示`静态`注册。

	void unregister_chrdev_region(dev_t first, unsigned int count);

### 字符设备的初始化
	#include <linux/cdev.h>
    /*静态方式初始化*/ 
    void cdev_init(struct cdev *cdev, struct file_operations *fops); 
	struct cdev my_cdev;
	cdev_init(&my_cdev, &fops);
	my_cdev.owner = THIS_MODULE;
    
    /*动态方式初始化*/
    struct cdev *my_cdev = cdev_alloc();
	my_cdev->ops = &fops;
	my_cdev->owner = THIS_MODULE;
    
    /*添加cdev*/
    int cdev_add(struct cdev *dev, dev_t num, unsigned int count);
    
    /*去除设备*/
    void cdev_del(struct cdev *dev);

### 内核空间和用户空间的数据交换
	ssize_t read(struct file *filp, char __user *buff, size_t count, loff_t *offp);  
	ssize_t write(struct file *filp, const char __user *buff, size_t count, loff_t *offp);
`filp`是文件指针。  
`buff`参数是用户空间指针，因此不能被内核代码直接引用，所以要使用下面几个方法。  
`offp`是一个指向“long offset type”对象，它指出用户正在存取的文件位置。  
`count`是请求的传输数据大小。

	get_user(x,ptr)
`x`是存储结果的变量  
`ptr`用户空间的原地址  
仅用于用户上下文，如果`pagefaults`打开，该函数可能会休眠。
这个宏将单个简单变量从用户空间复制到内核空间。它支持`char`和`int`这样的简单类型，但不支持较大的数据类型，像结构体或数组。
成功时返回0，出现错误时返回-EFAULT.
    
    /*向用户空间写入值*/
    put_user(x,ptr)
`x`是要拷贝到用户空间的值  
`ptr`用户空间的目标地址

	#include <asm/uaccess.h>
    unsigned long copy_from_user(void *to,const void __user *from,unsigned long count);
    unsigned long copy_to_user(void __user *to,const void *from, unsigned long count);
`__user`是一个宏，表明其后的指针指向用户空间。
`返回值`函数返回不能被复制的字节数，完全复制成功返回0.

	
