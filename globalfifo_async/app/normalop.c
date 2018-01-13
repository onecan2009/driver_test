#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define FILENAME "/dev/globalfifo"
#define NUM 128

int main()
{

    int fd = -1;
    int ret = -1;
    char buf[NUM] = {0};

    fd = open(FILENAME,O_RDWR);
    if(fd < 0)
    {
        perror("open file:");
        exit(-1);
    }

    ret = read(fd,buf,NUM);
    if(ret > 0)
    {
        printf("buf :%s\n",buf);
    }
    else if(ret < 0)
    {
        perror("read error:");
        exit(-2);
    }

    return 0 ;
}
