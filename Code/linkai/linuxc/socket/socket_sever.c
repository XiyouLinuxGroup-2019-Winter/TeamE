
/*************************************************************************
  > File Name: socket_sever.c
  > Author: NiGo
  > Mail: nigo@xiyoulinux.org
  > Created Time: 2020年06月08日 星期一 22时25分16秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <ctype.h>

#define SERV_PORT 8000

void my_err(const char *str)
{
    perror(str);
    exit(1);
}

int main(int argc, char *argv[])
{
    int listen_fd, connect_fd;
    char buf[BUFSIZ], client_ip[1024];
    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t len_client = sizeof(client_addr);
    //创建套接字(用于监听)
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        my_err("socket error");
    }
    //绑定地址结构(IP & port)
    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        my_err("bind error");
    }
    //设置最大监听客户端数量
    if (listen(listen_fd, 128) == -1)
    {
        my_err("listen error");
    }
    //阻塞等待客户端响应
    if ((connect_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len_client)) == -1)
    {
        my_err("accept error");
    }
    //打印客户端信息
    printf("client : ip = %s, port = %d\n", 
           inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip)),
           ntohs(client_addr.sin_port));
    while (1)
    {
        int ret = read(connect_fd, buf, sizeof(buf));
        write(STDOUT_FILENO, buf, ret);
        for (int i = 0; i < ret; i++)
        {
            buf[i] = toupper(buf[i]);
        }
        //printf("ret = %d\n", ret);
        write(connect_fd, buf, ret);
    }

    close(listen_fd);
    close(connect_fd);

    return 0;
}
