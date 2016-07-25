#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "tcpfunc.h"

/* 服务器要监听的本地端口号 */
#define LOCPORT 8848

/* 能够同时接受多少没有accept的连接 */
#define BACKLOG 10

const char exitcode[10] = "--exit";

/* 处理终端交互的线程,入口函数,传入参数的结构体,输入缓冲区 */
pthread_t pthd_io;
void *ioHandler(void *argc);
client_list *clist_head;
char strin[MAX_BUF_SIZE]; /* 终端输入缓冲区 */

/* 处理客户端信息线程的入口函数 */
void *clientHandler(void *argc);

/* 初始化服务器 */
int initserver(int type, struct sockaddr *addr, socklen_t alen, int qlen);

int main(int argc, char** argv)
{
	/* 在sock_fd上进行监听，在new_fd上接受新的连接 */
	int sock_fd, new_fd;

	/* 本地的地址信息 */
	struct sockaddr_in my_addr;

    /* 连接者的地址信息 */
    struct sockaddr_in their_addr;
    int sin_size = 0;

    /************* 配置本地地址***************/
    /* socket类型 */
    int type = SOCK_STREAM;
	/* 主机字节顺序 */
	my_addr.sin_family = AF_INET;
	/* 将主机需要绑定的端口号，转化为网络字节顺序的短整型 */
	my_addr.sin_port = htons(LOCPORT);
	/* 将运行程序主机的IP填充入s_addr,INADDR_ANY = 0 */
	my_addr.sin_addr.s_addr = INADDR_ANY;
	/* 将此结构的其余空间清零 */
	bzero(&(my_addr.sin_zero), 8);
    /*****************************************/

	/* 初始化服务器配置，完成socket，bind和listen */
	sock_fd = initserver(type, (struct sockaddr *)&my_addr, sizeof(struct sockaddr), BACKLOG);
    //printf("sockfd of main pthread: %d\n", sock_fd);

	/* 处理终端输入的线程 */
    clist_head = clist_create();
    clist_head->sock_fd = sock_fd; //头结点的默认sockfd是socket服务器的
    memcpy(&(clist_head->their_addr), &my_addr, sizeof(struct sockaddr_in));
    if ((pthread_create(&pthd_io, NULL, ioHandler, (void *)clist_head)) != 0)
    {
        printf("failed to create the pthread for io\n");
    }

	/* 主循环,一直保持accept()状态，等待客户端连接 */
	while (1)
	{
		sin_size = sizeof(struct sockaddr_in);

		/* 接受远程连接，程序阻塞于accept，一直等待接收，
		 * 如果调用accept出现错误，
		 * 则给出错误提示，并进入下一个循环 */
		if ((new_fd = accept(sock_fd, (struct sockaddr*)&their_addr, &sin_size)) == -1)
		{
			perror("accept");
			close(sock_fd);
			exit(-1);
		}

        /*处理新客户端连接的线程 */
        client_inpara *client_pthread_para = (client_inpara *)malloc(sizeof(struct _client_inpara));
        client_pthread_para->head = clist_head;
        client_pthread_para->sock_fd = new_fd;
        pthread_t pthd_new = (pthread_t)malloc(sizeof(pthread_t));
		if ((pthread_create(&pthd_new, NULL, clientHandler, (void *)client_pthread_para)) != 0)
        {
            printf("fail to create pthread for client %d\n",new_fd);
        }

        /* 将新的客户端信息加入客户端列表 */
        client_list *clist_new = clist_create();
        clist_new->sock_fd = new_fd;
        memcpy(&(clist_new->their_addr), &(their_addr), sin_size);
        printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));
        printf("the new sockfd is %d\n", new_fd);
        if ((insertNodeTail(clist_head, clist_new)) != 0)
        {
            printf("insert node to tail failed\n");
        }
	}
	close(sock_fd);
	return 0;
}

int initserver(int type, struct sockaddr *addr, socklen_t alen, int qlen)
{
    int fd;
    int err;
    /* 调用socket()，生成服务器原始的套接字描述符 */
    if ((fd = socket(addr->sa_family, type, 0)) < 0)
    {
#ifdef DEBUG_ON
        printf("faile to create sockfd\n");
#endif
        return -1;
    }
    /* 将套接字描述符sockfd与地址绑定my_addr */

    if (bind(fd, addr, alen) < 0)
        goto errout;
    /*如果是流或者seq，则开启监听*/
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET)
    {
            if (listen(fd, qlen) < 0)
                goto errout;
    }
    return fd;

errout:
    err = errno;
    close(fd);
    errno = err;
    return -1;
}

/* 终端交互线程的入口函数
 * 返回值：void*
 * 传入参数：void*,根据实际传入的参数，将参数指针赋给argc，若不止一个参数，
 * 则放在一个结构体内，将结构体指针传入
 * */
void *ioHandler(void *argc)
{
	char strin[MAX_BUF_SIZE];
    int homecommand = 0;
    int len = 0;
    char str[MAX_BUF_SIZE];

    /* 打印主页信息 */
    homePage();

	while(1)
	{
        if (fgets(strin, MAX_BUF_SIZE, stdin))
        {
            if (sscanf(strin, "%d", &homecommand) == 0)
                homecommand = 99;   //不是数字的非法输入
            //printf("homecommand: %d\n", homecommand);
            memset(strin, 0, strlen(strin));
            switch(homecommand)
            {
                /* 打印帮助信息 */
                case HELP:
                    printhelp();
                    homePage();
                    break;

                /* 显示所有服务器和客户端列表 */
                case DISPLAYALL:
                    printAll((client_list *)argc);
                    homePage();
                    break;

                /* 向所有客户端广播消息 */
                case BROADCAST:
                    broadcastFunc((client_list *)argc);
                    homePage();
                    break;

                /* 向指定客户端发送消息 */
                case SENDTOCLIENT:
                    sendToClientFunc((client_list *)argc);
                    homePage();
                    break;

                /* 关闭指定客户端连接 */
                case CLOSECLIENT:
                    printf("\n输入下列在线客户端的对应sockfd，关闭和它的连接\n");
                    printf("或输入--exit退出到主页\n");
                    printAll((client_list *)argc);
                    printf("-->");

                    closeClientFunc((client_list *)argc);
                    homePage();
                    break;

                /* 关闭服务器和所有客户端连接 */
                case CLOSESERVER:
                    printf("the server is closed\n");
                    //close(((client_list *)argc)->sock_fd);
                    while ((client_list *)argc)
                    {
                        close(((client_list *)argc)->sock_fd);
                        argc = (client_list *)((((node *)argc))->next);
                    }
                    exit(0);
                    break;

                default:
                    printf("请输入正确的对应数字，如下所示\n");
                    homePage();
                    break;

            }
        }
    }
	return ((void *)0);
}

/*处理客户端发送信息线程的入口函数，
 *目前主要功能是recv指定客户端的数据，并打印。
 *传入参数：void *，根据需要将参数放入结构体，将结构体指针转化为
 *			void *传入，在函数体内部，再转化回来
 *返回值；void *
 */
void *clientHandler(void *argc)
{
	char strRecv[MAX_BUF_SIZE];
    char str_head[4];
	int numbytes = 0;
    int head = 0;
    int sockfd =((client_inpara *)argc)->sock_fd;
    client_list *clist_head =(client_list *)((client_inpara *)argc)->head;
	while(1)
	{
        /* sockfd有效性判断，该连接是否已经被删除失效 */
        if (!(searchClient(clist_head, sockfd)))
        {
            printf("当前客户端线程的sockfd已失效，关闭该线程\n");
            break;
        }
        numbytes = recv(sockfd, strRecv, MAX_BUF_SIZE, 0);
		if (numbytes > 0)
		{
			strRecv[numbytes] = '\0';
            //printf("Received from client %d: %s\n", sockfd, strRecv);
            /* 在此处解析头部信息 */
            //sscanf(strRecv, "%d:%s", &head, strRecv);
            memcpy(str_head, strRecv, 3);
            sscanf(str_head, "%d:", &head);

            /* 请求客户端sockfd列表 */
            if (head == CLIST_REQUEST)
            {
                int erro = 0;
                //格式化发送客户端sockfd列表
                if (sendClist(clist_head, sockfd))
                {
                    printf("发送客户端sockfd列表失败\n");
                }
            }
            /* 请求客户端在服务器中的sockfd */
            else if (head == GET_SOCKFD_IN_SERVER)
            {
                sprintf(strRecv + 3, "%02d:", sockfd);
                //printf("GET_SOCKFD_IN_SERVER: %s\n", strRecv);
                if (!(send(sockfd, strRecv, strlen(strRecv), 0)))
                {
                   //printf("client %d GET_SOCKFD_IN_SERVER failed\n", sockfd);
                }
            }
            /* 直接发送到服务器的数据 */
            else if (head == SENDTO_SERVER)
            {
                printf("接收自客户端 %d: %s\n", sockfd, strRecv + 3);
            }
            /* 转发数据到对应客户端，head即为客户端在服务器的sockfd,
             * 头部填充源头客户端的sockfd*/
            else if (head > SENDTO_SERVER)
            {
                sprintf(str_head, "%02d:", sockfd);
                memcpy(strRecv, str_head, 3);
                //printf("转发到客户端 %d\n", head);
                /* 解析头部信息 */
                if (!(send(head, strRecv, strlen(strRecv), 0)))
                    printf("转发到客户端 %d 失败\n", head);
                //else
                //    printf("发送到客户端 %d：%s", head, strRecv + 3);
            }
            head = 0;
            memset(strRecv, 0, strlen(strRecv));
            memset(str_head, 0, HEAD_BUF_SIZE + 1);
		}
        else
		{
            if (!(delClient(clist_head, sockfd)))
            {
                printf("receive failed from client %d, close it\n", sockfd);
            }
            else
            {
                printf("delete client %d failed\n", sockfd);
            }
			break;
		}
	}
    //pthread_exit((void*)1);
	close(sockfd);
	return ((void *)1);
}
