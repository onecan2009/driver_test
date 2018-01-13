#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#define NUM 128
#define FILENAME "/dev/globalfifo"

static int fd = -1;

void test(int num)
{
	char data[NUM] = {0};
	int len = 0;
	printf("signum is %d\n",num);
	len = read(fd,data,NUM);
	if(len > 0)
	{
		data[len] = '\0';
		printf("read data size is %d\n",len);
		printf("%s\n",data);
	}

}

void stop(int num)
{
	printf("pid termited...\n");
	close(fd);
	exit(0);
}

int main()
{
	int oflags;
	
	fd = open(FILENAME,O_RDWR | O_NONBLOCK);
	if(fd < 0)
	{
		perror("open file error...:");
		return 0;
	}
	signal(SIGIO,test);
	signal(SIGINT,stop);
	fcntl(fd,F_SETOWN,getpid());
	oflags = fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL,oflags | FASYNC);

	while(1)
	{
		usleep(10);
	}

}
