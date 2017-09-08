#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/ioctl.h>

#define COMMAND1 1
#define COMMAND2 2

int main(void)
{
    int fd;
    int ret;
    int val = 1;
    char data = {0};
    fd = open("/dev/helloDevice_234", O_RDWR);
    if(fd < 0){
        printf("can't open!\n");
    }
    if((ret = write(fd, &val, 4)) < 0)
    {
    	printf("write error\n");
    	exit(ret);
    }
    if((ret = ioctl(fd,COMMAND1,0)) < 0)
    {
    	printf("ioctl error!\n");
    	exit(ret);
    }
    close(fd);
    return 0;
}
