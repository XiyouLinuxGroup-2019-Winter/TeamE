/*************************************************************************
	> File Name: my_recv.c
	> Author: 
	> Mail: 
	> Created Time: 2020年07月13日 星期一 12时01分18秒
 ************************************************************************/

#define MY_RECV_C

#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<errno.h>
#include "my_recv.h"
#include<stdlib.h>

void my_err(const char *err_string,int line)
{
    fprintf(stderr,"line : %d ",line);
    perror(err_string);
    exit(1);
}

int my_recv(int conn_fd,char * data_buf,int len)
{
    static char recv_buf[BUFSIZE];
    static char *pread;
    static int len_remain = 0;
    int i;
    if(len_remain <= 0)
    {
        if((len_remain = recv(conn_fd,recv_buf,sizeof(recv_buf),0)) < 0)
        {
            my_err("recv",__LINE__);
        }
        else if(len_remain == 0)
            return 0;
        pread = recv_buf;
    }

    for(i = 0; * pread != '\n';i++)
    {
        if(i > len)
            return -1;
        data_buf[i] = *pread++;
        len_remain -- ;

    }
    len_remain --;
    pread ++;
    return i;
}
