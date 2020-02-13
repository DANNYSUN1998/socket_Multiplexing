#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string>
#include<sys/time.h>
#include<sys/epoll.h>

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

int main(int argc,char* argv[])
{
     //argc表示参数的个数，argv表示这些参数
    if(argc != 3)//此处的判断是确定程序必须有两个参数传入，一个是ip地址，一个是端口号
    {
        Usage(argv[0]);//argv[0]内固定的存储本程序的地址
        return -1;
    }

    int listensock = start_up(argv[1],atoi(argv[2]));
    //生成一个epoll函数的专用文件描述符，既申请了一个内核空间，用来存放向关注的文件描述符是否发生了什么事件，参数size是所创建的文件描述符能关注的最大fd数目
    int epollfd = epoll_create(256);
    if(epollfd < 0)
    {
        perror("epoll_create");
        exit(5);
    }
    struct epoll_event ev;
    ev.data.fd = listensock;
    ev.events = EPOLLIN;
    if(epoll_ctl(epollfd,EPOLL_CTL_ADD,listensock,ev) < 0)
    {
        perror("epoll_ctr");
        exit(6);
    }

    int evnums = 0;//用于存放epoll_wait函数的返回值
    struct epoll_event evs[64];//用于存放调用epoll_wait函数后从内核拷贝出来的就绪的文件描述符
    int timeout = -1;//epoll_wait函数的超时参数，
    while(1)
    {
        switch(evnums = epoll_wait(epollfd,evs,64,timeout))//从内核拷贝就绪的描述符到evs中
        {
            case 0:
            {
                printf("timeout\n");
                break;
            } 

            case -1:
            {
                perror("epoll_wait");
                break;
            }  

            default:
            {
                //遍历拷贝到的描述符
                int i = 0;
                for(;i  < evnums; i++)
                {
                   //扫描到用于监听连接请求的socket，调用accept函数建立新的socket
                    if(evs[i].data.fd == listensock && evs[i].events == EPOLLIN)
                    {
                         struct sockaddr_in client;
                        socklen_t len = sizeof(client);
                        int new_stock = accept(listensock,(struct sockaddr*) & client,sizeof(client));
                        if(new_stock < 0)
                        {
                            perror("accept");
                            continue;
                        }
                        else
                        {
                            printf("get a new client\n");
                            ev.data.fd = new_stock;
                            ev.events = EPOLLIN;
                            epoll_ctl(epollfd,EPOLL_CTL_ADD,new_stock,&ev);
                        }
                        
                    }
                    //扫描到可读的描述符
                    else if(evs[i].data.fd != listensock && evs[i].events == EPOLLIN)
                    {
                        char buf[1024];
                        ssize_t s = read(evs[i].data.fd,buf,sizeof(buf)-1);
                        if(s > 0)
                        {
                            buf[s] = 0;
                            printf("client send %s",buf);
                            ev.data.fd = evs[i].data.fd;
                            ev.events = EPOLLOUT;
                            epoll_ctl(epollfd,EPOLL_CTL_MOD,evs[i].data.fd, &ev);
                        }
                        else//没有内容，socket内容接受完成，关闭对应的socket
                        {
                            close(evs[i].data.fd);
                            epoll_ctl(epollfd,EPOLL_CTL_DEL,evs[i].data.fd,NULL);
                        }
                        
                    }
                    //扫描到可写的描述符
                    else if(evs[i].data.fd != listensock && evs[i].events == EPOLLOUT)
                    {
                        write(evs[i].data.fd,"200 ok",sizeof("200 ok"));
                        close(evs[i].data.fd);
                        epoll_ctl(epollfd,EPOLL_CTL_DEL,evs[i].data.fd,NULL);
                    }
                    else
                    {
                        
                    }
                    
                }
                break;
            }
                
        }
    }
    return 0;

}

/*
epoll是多路服用I/O接口select和poll函数的增强版本。他显著的减少了程序在有大量的并发连接中只有少量活跃的情况下CPU的利用率。
epoll函数不会复用文件描述符集合来传递结果，而是专门开辟一个集合存储内核中就绪的描述符。
首先调用epoll_create函数创建一个epoll函数专用的描述符（申请一个内核空间，用于存放想要关注的文件描述符）；
调用epoll_create的同时，底层创建了一个红黑树和一个就绪连表，红黑树存储所监控的文件描述符的节点数据，就绪链表存储就绪的文件描述符的节点数据；
epoll_ctl函数可以修改所有的文件描述符的集合，包括增添、删除、修改。
当接收到某个文件描述符的数据时，内核将该结点插入到就绪队列当中。
epoll_wait函数会收到信息，并将数据拷贝到用户空间，清空链表。

epoll函数所具备的优点：
1. 支持一个进程打开多个（大数目的）文件描述符，select函数一次打开的数目是有限的，对于支持上万连接数的服务器来说太小了
2. I/O效率不会随着文件描述符的数量增加而线性下降，传统的select/poll函数的最大弊端在于当进程拥有一个很大的文件描述符集合时，由于网络的时延，任一时间只有部分的socket是活跃的，
但是select和poll函数都会线性的扫描全部的集合，导致效率下降。epoll函数下只需要遍历就绪的文件描述符即可。


*/