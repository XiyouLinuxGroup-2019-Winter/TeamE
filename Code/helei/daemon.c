/*************************************************************************
	> File Name: daemon.c
	> Author: 
	> Mail: 
	> Created Time: 2020年04月25日 星期六 10时46分12秒
 ************************************************************************/

#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include<signal.h>
#include<sys/param.h>
#include<sys/stat.h>
#include<time.h>
#include<syslog.h>

int init_daemon(void);
int main(void)
{
    time_t now;
    intit_daemon();
    syslog(LOG_USER | LOG_INFO,"测试守护进程！\n");
    while(1)
    {
        sleep(8);
        time(&now);
        syslog(LOG_USER | LOG_INFO , "系统时间：\t%s\t\t\n",ctime(&now));
    }
}



int init_daemon(void)
{
    int pid;
    int i;
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
    pid = fork();
    if(pid >0)
    {
        exit(0);
    }
    else if (pid <0)
        return -1;
    setsid();
    pid = fork();
    if(pid >0)
        exit(0);
    else if(pid <0)
        return -1;
    for(i =0 ;i<NOFILE,close(i++));
    chdir("/");
    umask(0);
    signal(SIGCHLD,SIG_IGN);
    return 0;
}

