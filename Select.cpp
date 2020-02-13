#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string>
#include<sys/time.h>

static void Usage(const char * proc)
{
    printf("%s [local_ip][local+port]\n",proc);
}

//用于保存需要监视的文件描述符
int array[4096];

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

int main(int argc,char * argv[])
{
    //argc表示参数的个数，argv表示这些参数
    if(argc != 3)//此处的判断是确定程序必须有两个参数传入，一个是ip地址，一个是端口号
    {
        Usage(argv[0]);//argv[0]内固定的存储本程序的地址
        return -1;
    }

    //调用start_up函数建立对应参数的套接字，传入的参数为ip地址和端口号，此套接字用于监听外界的连接请求
    int listenSock = start_up(argv[1],atoi(argv[2]));
    int maxfd = 0;    //最大文件描述符数，
    fd_set rfds;         //可读描述符集合
    fd_set wfds;       //可写描述符集合
    array[0] = listenSock; //array数组的第一个位置用于存储用于监听连接请求的socket

    int i = 1;
    int array_size = sizeof(array) / sizeof(array[0]);
    
    //将所有的没有保存描述符的位置初始化为-1，方便遍历数组
    for(;i < array_size;i++)
    {
        array[i] = -1;
    }

    while(1)
    {
        //清空可读文件描述符集合和可写文件描述符集合
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        for(i = 0;i < array_size;i++)//将array数组中一寸处文件描述符的位加入到可读集合和可写集合当中
        {
            if(array[i] > 0)
            {
                FD_SET(array[i],&rfds);
                FD_SET(array[i],&wfds);
                if(array[i] > maxfd)
                {
                    maxfd = array[i];
                }
            }
        }

        switch(select(maxfd+1,&rfds,&wfds,NULL,NULL))  //调用select函数等待所关心的文件描述符，有文件描述符就绪以后，select函数返回；没有
        {                                                                                                      //就绪的，就把对应的描述符集合对应的位置归0
            case 0://超时
            {
                    printf("timeout");
                    break;
            }

            case -1://select调用失败
            {
                perror("se;ect");
                break;
            }

            default:
            {
                int j = 0;
                for(;j < array_size; i++)
                {
                    if( j == 0 && FD_ISSET(array[j],&rfds))    //用于接收连接请求的socket可读
                    {//建立新的socket处理客户请求
                        struct sockaddr_in client;
                        socklen_t len = sizeof(client);
                        int new_sock = accept(listenSock,(struct sockaddr*)& client,&len);
                        if(new_stock < 0)
                        {
                            perror("accept");
                            continue;
                        }
                        else
                        {
                            printf("get a new client%s\n",inet_addr(client.sin_addr));
                            fflush(stdout);
                            int k = 1;
                            for(;k < array_size;k++)
                            {
                                if(array[k] < 0)
                                {
                                    array[k] = new_sock;
                                    if(new_sock > maxfd)
                                        maxfd = new_sock;
                                    break;
                                }
                            }
                            if(k == array_size)
                                close(new_sock);
                        }
                        
                    }
                    else if(j!=0 && FD_ISSET(array[j],&rfds))//有事件准备就绪
                    {
                        //
                        char buf[1024];
                        ssize_t s = read(array[j],buf,sizeof(buf)-1);
                        if(s > 0)
                        {
                            buf[s] = 0;
                            printf("client say %s\n",buf);
                            if(FD_ISSET(array[j],&wfds))
                            {
                                write(array[j],"200 ok",strlen("200 ok"));
                            }

                        }
                        else if(s == 0)
                        {
                            printf("client quit\n");
                            close(array[j]);
                            array[j] = -1;
                        }
                        else
                        {
                            perror("read");
                            close(array[j]);
                            array[j] = -1;
                        }
                        
                    }
                }
                break;
            }
        }
    }

    return 0;

}

//使用select函数时，一个进程可以监视的文件描述符数最大为1024个
//select函数需要维护一个存放大量的fd数据结构是的用户态和内核态在复制传递时开销较大
//扫描fd是线性的扫描，fd剧增后，I/O的效率降低
