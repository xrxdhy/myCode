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
    char data[20] = "Are you ok, yhd?";
    fd = open("/dev/helloModule_device", O_RDWR);
    if(fd < 0){
        perror("can't open!");
        exit(0);
    }
    if((ret = write(fd, &data, sizeof(data))) < 0)
    {
    	perror("write error");
    	exit(ret);
    }
    if((ret = ioctl(fd,COMMAND1,0)) < 0)
    {
    	perror("ioctl error!");
    	exit(ret);
    }
    if((ret = read(fd,data,2)) < 0)
    {
        perror("read error!");
        exit(ret);
    }
    printf("There is userspace : data = %s\n",data);
    close(fd);
    return 0;
}
