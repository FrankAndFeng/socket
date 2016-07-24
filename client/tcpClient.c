#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/wait.h>
#include<netdb.h>

#include<pthread.h>

/* 服务器程序监听的端口号 */
#define PORT 8848

/* 一次所能够接收的最大字节数 */
#define MAX_BUF_SIZE 256

/* 重新连接服务器的最大等待时间 */
#define MAXSLEEP 64

/* 命令缓存区大小，和命令*/
#define ORDER_BUF_SIZE 10
#define EXIT           "--exit"

/* 头部信息缓存区大小 */
#define HEAD_BUF_SIZE        4
#define CLIST_REQUEST        1  //获取在线客户端列表
#define GET_SOCKFD_IN_SERVER 2  //获取自身在服务器的sockfd
#define SENDTO_SERVER        3  //直接发送数据到服务器
/* 对应头部信息发送命令 */
#define CLIST_REQUEST_SEND          "01:"
#define GET_SOCKFD_IN_SERVER_SEND   "02:"
#define SENDTO_SERVER_SEND          "03:"


/* 从服务器获得的在线客户端sockfd列表，及其解析函数 */
int clist[20];
int getClist(char *str);
void printClist(void);
#define MAX_CLIST_LEN 20

/* 服务器断开重连 */
int connect_retry(int sockfd, const struct sockaddr *addr, socklen_t alen);

/* 可移植的断开重连函数 */
int connect_retry_new(int domain, int type, int protocol,
        const struct sockaddr *sockfd, socklen_t alen);

/* 处理终端交互的线程和入口函数,以及输入参数 */
pthread_t pthd_io;
void *ioHandler(void *argc);

typedef struct _iop
{
	int sockfd;
}iop;
iop *inp;

int main(int argc, char** argv)
{
	int sockfd, numbytes;
	char str_recv[MAX_BUF_SIZE];
	struct hostent *he;
    int head = 0;

	/* 客户端的主机信息 */
	struct sockaddr_in their_addr;

	/* 检查参数信息 */
	if (argc != 2)
	{
		fprintf(stderr, "usage: client hostname\n");
		exit(1);
	}
	/* 获取主机信息 */
	if ((he = gethostbyname(argv[1])) == NULL)
	{
		herror("gethostbyname");
		exit(1);
	}

	/* 获取套接字描述符 */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		exit(1);
	}

	/* 主机字节顺序，定义addrss地址族 */
	their_addr.sin_family = AF_INET;

	/* 将本地端口号转换为网络短整型字节顺序 */
	their_addr.sin_port = htons(PORT);
	their_addr.sin_addr = *((struct in_addr*)he->h_addr);

	/* 余下清零 */
	bzero(&(their_addr.sin_zero), 8);

	/* 连接服务器 */
	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
	{
		perror("connect");
		exit(1);
	}
	inp = (iop *)malloc(sizeof(struct _iop));
	inp->sockfd = sockfd;
	int err;
	if ((err = pthread_create(&pthd_io, NULL, ioHandler, (void *)inp)) == 0)
	{
		printf("create pthread succeed : %d\n", err);
	}
	printf("the client sockfd is : %d\n",((iop *)inp)->sockfd);
	/* 一直接受服务器数据，默认不关心数据来源 */
	while(1)
	{
		numbytes = recv(sockfd, str_recv, MAX_BUF_SIZE, 0);
		if (numbytes > 0)
		{
			str_recv[numbytes] = '\0';
            /* 判根据头信息辨别和解析数据 */
            sscanf(str_recv, "%d:%s", &head, str_recv);

            /* CLIST_REQUEST：来自服务器的客户端sockfd列表 */
            if (head == CLIST_REQUEST)
            {
                if (!(getClist(str_recv)))
                {
                    printClist();
                }
                else
                    printf("get client list failed\n");
            }

            /* GET_SOCKFD_IN_SERVER:客户端在服务器中的sockfd */
            else if (head == GET_SOCKFD_IN_SERVER)
            {

            }

            /* SENDTO_SERVER：来自服务器的数据 */
            else if (head == SENDTO_SERVER)
            {
                printf("Received from server: %s\n", str_recv);
            }

            /* 来自其他客户端的信息 */
            else if (head > SENDTO_SERVER)
            {
                printf("received from client %d:%s", head, str_recv);
            }
		}
		else
		{
            //int res = connect_retry(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
            //int sockfd = connect_retry_new(AF_INET, SOCK_STREAM, 0, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
            //if (sockfd == -1)
            close(sockfd);
			perror("recv");
			exit(1);
		}
		/* 此处添加异常处理，如果捕捉到服务器异常断开连接
		 * retry connect，使用指数补偿算法，尝试重新连接N次，
		 * 如仍未成功连接，则断开连接，关闭socket*/
	}
	close(sockfd);
	return 0;
}

/* 处理终端交互线程的入口函数
* 返回值：void*
* 传入参数：void*,根据实际传入的参数，将参数指针赋给argc，若不止一个参数，
* 则放在一个结构体内，将结构体指针传入*/
void *ioHandler(void *argc)
{
    char strin[MAX_BUF_SIZE];
    char order_str[10];
    char exit_order[10];
    int order = 0;
    int clist[20];
    int sockfd = ((iop *)argc)->sockfd;
	//printf("the client sockfd is : %d\n",((iop *)argc)->sockfd);
	while (1)
	{
        printf("输入对应代码，选择功能，或--exit退出\n");
        printf("1, 向服务器发送数据\n");
        printf("2, 向指定客户端发送数据\n");
        printf("-->");

        if (fgets(order_str, 10, stdin))
        //sscanf(stdin, "%d", order))
        {
             /* 捕捉退出信号 */
            sscanf(order_str, "%s", exit_order);
            if (!(strcmp(exit_order, EXIT)))
            {
                memset(exit_order, 0, 10);
                memset(order_str, 0, 10);
                break;
            }

            sscanf(order_str, "%d", &order);
            if (order == 1)
            {
                /* 直接发送到服务器 */
                while (1)
                {
                    printf("请输入传送的数据，或输入--exit退出\n");
                    printf("发送到服务器：");
                    if (fgets(strin + HEAD_BUF_SIZE - 1, MAX_BUF_SIZE + HEAD_BUF_SIZE - 1, stdin))
                    {
                        /* 捕捉退出信号 */
                        sscanf(strin + HEAD_BUF_SIZE - 1, "%s", exit_order);
                        if (!(strcmp(exit_order, EXIT)))
                        {
                            memset(exit_order, 0, 10);
                            memset(strin, 0, strlen(strin));
                            break;
                        }
                        /* 为发送数据加头部信息 */
                        memcpy(strin, SENDTO_SERVER_SEND, HEAD_BUF_SIZE - 1);
                        /* 消除尾部换行符，发送 */
                        strin[strlen(strin) - 1] = '\0';
                        if (send(sockfd, strin, strlen(strin), 0))
                        {
                            printf("发送成功！\n");
                        }
                        memset(strin, 0, strlen(strin));
                    }
                }
            }
            else if (order == 2)
            {
                /* 发送给指定客户端 */
                printf("请输入对应sockfd，从下列客户端中选择发送目标\n");
                /* 给服务器发送请求，获取在线客户端的列表 */
                if (send(sockfd, CLIST_REQUEST_SEND, 4, 0))
                {
                    printf("请求发送成功\n");
                }

                /* 等待一段时间，打印在线客户端列表 */


                /* 获取选择的目标客户端 */

                while (1)
                {
                    printf("请输入传送的数据，或输入--exit退出\n");
                    printf("发送到客户端 ：");
                    if (fgets(strin + HEAD_BUF_SIZE - 1, MAX_BUF_SIZE + HEAD_BUF_SIZE - 1, stdin));
                    {
                        /* 捕捉退出信号 */

                        /* 为发送数据加头部信息 */

                        /* 消除尾部换行符，发送 */
                    }
                }

                //print client list
                //choose a client
                //while(1),type data ,or --exit to exit
            }
            memset(order_str, 0, 10);
        }
        /*
        //检测输入，选择功能
        //再来一个while（1），
		if (fgets(strin, MAX_BUF_SIZE, stdin))
		{
			strin[strlen(strin) - 1] = '\0';
			printf("thread %d: %s\n", getpid(), strin);
			flag = send(sockfd, strin, strlen(strin), 0);
			printf("send the data? %d\n", flag);
			memset(strin, 0, strlen(strin));
		}
        */
	}
	return ((void*)0);
}
/* 服务器断开重连，采用指数补偿算法，如果和服务器的连接意外断开
 * 进程会休眠一小段，然后进入下次循环再次尝试，
 * 每次循环休眠时间按照指数增加，直到最大延迟为一分钟左右*/
int connect_retry(int sockfd, const struct sockaddr *addr, socklen_t alen)
{
    int numsec;
    /*
     *以指数增长的休眠时间重新连接
     */
    for (numsec = 1; numsec <= MAXSLEEP; numsec <<= 1)
    {
        printf("trying to connect with server....\n");
        if (connect(sockfd, addr, alen) == 0)
        {
            return 0;
        }
        if (numsec <= MAXSLEEP/2)
            sleep(numsec);
    }
    return -1;
}

int connect_retry_new(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t alen)
{
    int numsec, fd;

    for (numsec = 1; numsec <= MAXSLEEP; numsec <<= 1)
    {
        printf("retrying connect...\n");
        if ((fd = socket(domain, type, protocol)) < 0)
            return -1;
        if (connect(fd, addr, alen) == 0)
        {
            return fd;
        }
        close(fd);
        if  (numsec <= MAXSLEEP/2)
        {
            sleep(numsec);
        }
    }
    return -1;
}

int getClist(char *str)
{
    int ret = 0;
    if (NULL == str)
        return --ret;

    int len = 0, i = 0;
    int sockfd = 0;
    len = strlen(str);

    while (1)
    {
        /* 依次提取sockfd，以：为分隔符 */
        sscanf(str, "%d:%s", &sockfd, str);
        if ((sockfd != 0) && (NULL != str))
        {
            clist[i++] = sockfd;
            sockfd = 0;
            if (strlen(str) == len)
            {
                break;
            }
            else
            {
                len = strlen(str);
            }
        }
        else
            break;
    }
    return ret;
}

/* 打印客户端sockfd列表 */
void printfClist()
{
    int i;

    printf("目前在线的客户端：\n");
    for(i = 0;i < MAX_CLIST_LEN; i++)
    {
        if (clist[i] != 0)
        {
            printf("客户端 %d\n", clist[i++]);
        }
        else
        {
            break;
        }
    }
}
