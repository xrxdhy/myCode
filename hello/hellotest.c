#include <fcntl.h>
#include <stdio.h>

int main(void)
{
    int fd;
    int val = 1;
    fd = open("/dev/helloDevice", O_RDWR);
    if(fd < 0){
        printf("can't open!\n");
    }
    write(fd, &val, 4);
    close(fd);
    return 0;
}
