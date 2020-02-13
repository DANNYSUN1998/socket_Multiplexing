#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string>
#include<sys/time.h>
#include<poll.h>

static void Usage(const char * proc)
{
    printf("%s [local_ip][local+port]\n",proc);
}



//程序开始之初先建立一个用于监听连接请求的套接字
static int start_up(const char * _ip,int _port)
{
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock  < 0)//sock创建不成功
    {
        perror("socket");
        exit(1);
    }

    //创建socket地址数据
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(_port);
    local.sin_addr.s_addr = inet_addr(_ip);

    //将之前建立的socket对象绑定到对应ip地址和端口上
    if(bind(sock , (struct sockaddr*)& local ,sizeof(local)) < 0)
    {
        perror("bind");
        exit(2);
    }
    //将套接字设置为监听模式等待连接请求
    if(listen(sock,10)<0)
    {
        perror("listen");
        exit(3);
    }
    return sock;
}

int main(int argc, char * argv[])
{
    //argc表示参数的个数，argv表示这些参数
    if(argc != 3)//此处的判断是确定程序必须有两个参数传入，一个是ip地址，一个是端口号
    {
        Usage(argv[0]);//argv[0]内固定的存储本程序的地址
        return -1;
    }

    //调用start_up函数建立对应参数的套接字，传入的参数为ip地址和端口号，此套接字用于监听外界的连接请求
    int listenSock = start_up(argv[1],atoi(argv[2]));
    struct pollfd peerfd[1024];
    peerfd[0].fd = listenSock;
    peerfd[0].events = POLLIN;
    int nfds = 1;
    int ret; //用于记录poll函数的返回值
    int maxsize = sizeof(peerfd) / sizeof(peerfd[0]);
    int i = 1;
    int timeout = -1;
    //将所有的没有保存描述符的位置初始化为-1，方便遍历数组
    for(;i < maxsize;i++)
    {
        peerfd[i].fd = -1;
    }
    while(1)
    {
        switch(ret = poll(peerfd,nfds,timeout))
        {
            case 0:
            {
                printf("timeout\n");
                break;
            }
            case -1:
            {
                perror("poll");
                break;
            }
            default:
            {
                if(peerfd[0].revents == POLLIN)//有新的连接请求
                {
                    struct sockaddr_in client;
                    socklen_t len = sizeof(client);
                    int new_sock = accept(sock,(struct sockaddr*)& client,&len);   //创建新的socket用于服务当前的client
                    printf("accept finish %d\n",new_sock);
                    if(new_sock < 0)
                    {
                        perror("accept");
                        continue;
                    }
                    printf("get a new client\n");
                    int j = 1;
                    for(;j < maxsize;j++)
                    {
                        if(peerfd[j].fd < 0)
                        {
                            peerfd[j].fd = new_sock;
                            break;
                        }
                    }
                    if (j == maxsize)    //达到可以服务的client数量的上限
                    {
                        printf("too many clients\n");
                        close(new_sock);
                    }
                    peerfd[j].events = POLLIN;
                    if(j + 1 > nfds)
                        nfds = j + 1;
                }

                for(i = 1; i < nfds; i++)
                {
                    if(peerfd[i].revents == peerfd[i].revents && peerfd[i].events == POLLIN)
                    {
                        printf("read ready\n");
                        char buf[1024];
                        ssize_t s = read(peerfd[i],buf,sizeof(buf) - 1);
                        if(s > 0)
                        {
                            buf[s] = 0;
                            printf("client say %s",buf);
                            fflush(stdout);
                            peerfd[i].events = POLLOUT;
                        }
                        else if(s <= 0)
                        {
                            close(peerfd[i].fd);
                            peerfd[i].fd = -1;
                        }
                        else
                        {
                            
                        }
                        
                    }
                    else if(peerfd[i].revents ==  peerfd[i].events && peerfd[i].events == POLLOUT)
                    {
                        write(peerfd[i].fd,"200 ok",sizeof("200 ok"));
                        close(peerfd[i].fd);
                        peerfd[i].fd = -1;
                    }
                    else
                    {
                        /* code */
                    }
                    
                }
            }
            break;
        }
    }
    return 0;
}


/*
struct pollfd{
    int fd;    //文件描述符
    short events; //需要关心的事件
    short revents; //关心的事件就绪时，revents会被设置为上述对应的事件
}

对于poll函数
int poll(struct pollfd * fds, unsigned int nfds,timeout);
fds：结构体指针，pollfd数组
nfds：数组元素的总个数
timeout：超时时间，0表示非阻塞式等待，小于0表示阻塞式等待，大于0表示等待的时间

poll函数返回-1表示调用失败，返回0表示超时，返回正数表示就绪的文件描述符的个数

poll函数的实现原理：
将需要关心的文件描述符放到fds数组中；
调用poll函数；
函数成功的返回了以后便利fds数组，将关心的事件与结构体当中的revents相与判断事件是否已经就绪
对就绪的事件，执行相关的操作
*/