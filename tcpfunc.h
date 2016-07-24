#ifndef __LINK_H__
#define __LINK_H__

/* 终端输入可以接受字符串的最大长度 */
#define MAX_BUF_SIZE 1024

/* homepage命令列表 */
#define HELP            '0'
#define DISPLAYALL      '1'
#define BROADCAST       '2'
#define SENDTOCLIENT    '3'
#define CLOSECLIENT     '4'
#define CLOSESERVER     '5'

/* 命令缓存区大小 */
#define HEAD_BUF_SIZE 4
/* 退出当前选项命令 */
#define EXIT            "--exit"

/* 头部信息缓存区大小 */
#define HEAD_BUF_SIZE 4

/* 头部信息解析，大于3的头部信息就自动解析为发送到指定的客户端 */
#define CLIST_REQUEST        1 //请求在线客户端sockfd列表
#define GET_SOCKFD_IN_SERVER 2 //获取客户端在服务器分配的sockfd
#define SENDTO_SERVER        3 //直接发送到服务器的数据
/* 对应头部信息发送 */
#define CLIST_REQUEST_SEND          "01:"
#define GET_SOCKFD_IN_SERVER_SEND   "02:"
#define SENDTO_SERVER_SEND          "03:"
/* 链表结点 */
typedef struct _node
{
    struct _node *next;
}node;

/* 客户端列表 */
typedef struct _client_list
{
    node *pnode;
    int sock_fd;
    struct sockaddr_in their_addr;
}client_list;

/* 传入客户端线程的参数结构体 */
typedef struct _client_inpara
{
    client_list *head;
    int sock_fd;
}client_inpara;

/***********链表操作相关函数*************/
/* 创建客户端列表 */
client_list *clist_create(void);

/* 在尾部列表插入客户端结点 */
int insertNodeTail(client_list *clist_head, client_list *clist_new);

/* 根据sockfd删除结点 */
int delClient(client_list *clist_head, int sockfd);

/* 根据sock查找客户端 */
client_list *searchClient(client_list *clist_head, int sockfd);

/* 打印所有客户端sockfd */
void printAllClient(client_list *clist_head);

/************终端交互操作函数**********/
/* 主页信息显示 */
int homePage(void);

/* 打印帮助信息 */
int printhelp(void);

/* 打印服务器和终端信息 */
int printAll(client_list *head);

/* 向所有终端广播消息 */
int broadcast(client_list *head, char *str_send, int len);
/* 集成的广播命令 */
int broadcastFunc(client_list *head);

/* 向指定客户端发送消息 */
int sendToClient(client_list *head, int sockfd, char *str_send, int len);
/* 集成的，向指定客户端发送消息的命令 */
int sendToClientFunc(client_list *head);

/* 关闭指定客户端连接 */
int closeClient(client_list *head, int sockfd);
/* 集成的，关闭指定客户端连接命令 */
int closeClientFunc(client_list *head);

/* 获取sockfd的有效性判断 */
int validSockfd(client_list *head, int sockfd);

/* 封装和发送在线客户端列表,发送成功返回0，否则返回-1 */
int sendClist(client_list *head, int sockfd);

#endif
