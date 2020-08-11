#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<pthread.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<time.h>
#include<mysql.h>
#include<signal.h>



/*
    创建一个消息盒子表，
    登录前查看消息盒子表是否有该账户信息，如果有则在底部提示请求查看（pack.type == 16）
    登录时记得设置用户套接字和修改用户状态信息
    挨个查看，挨个处理
    客户端要给足需要传入的参数，
    服务器端进行筛选，最终反馈给mysql数据库
    客户端去反复接受服务器端传入的数据（如需要获取好友列表）
    获取到就printf
    把所有数字改成宏
*/

#include "my_pack.h"
#include "my_sql.h"
MYSQL mysql;
int ACCOUNT;
int CONN_FD;

#define PORT 8888
#define MAX_SIZE 1024
#define EPOLLMAX 1024


#define LOGIN 1
#define REGISTERED 2
#define FIND_PASSWORD 3
#define CHANGE_PASSWORD 33
#define EXIT_CUI 4


#define SIGNAL_SEND 101
#define GROUP_CHAT 102

#define ADD_FRIEND 103
#define DEL_FRIEND 104
#define LOOK_FRIEND 105
#define LOOK_CHAT_RECORD 106
#define SET_REALTION 107

#define CREATE_GROUP 108
#define ADD_GROUP 1081
#define RETURN_GROUP 1082
#define FREE_GROUP 109
#define LOOK_GROUP_MEMBER 110
#define LOOK_GROUP 111

#define SEND_FILE 113
#define EXIT_ACCOUNT 114

#define LOOK_MESSAGEBOX 115
#define MANAGE_FRIEND 116
#define MANAGE_MESSAGE 117
#define MANAGE_GROUP 118
#define MANAGE_FILE 120
#define RECV_FILE 119


int Login_Account(PACK *pack,MYSQL *mysql);
int Registed_Account(PACK *pack,MYSQL *mysql);
int Find_Password(PACK *pack,MYSQL *mysql);
int Change_Password(PACK *pack,MYSQL * mysql);
int Exit_Account(PACK *pack,MYSQL *mysql);
void signal_fun(int signo);
int getNowTime(char *nowTime);
int Find_Friend_Realtion(PACK *pack);
int Add_Friends(PACK *pack,MYSQL *mysql);
int Signal_Chat(PACK *pack);
int Find_Account(int account);
int Del_Friend(PACK * pack,MYSQL *mysql);
int Offline_Messages(int fd,int account);
int Look_MessageBox(int fd,int account);
int Look_Friend(int fd,int account);
int Manage_Friend(int recv_account,int send_account);
int Look_Chat_Record(int fd,int account);
int Manage_Message(int send_account,int recv_account,const char * string);
int Set_Realtion(int send_account,int recv_account,int state);
int Registed_Group(PACK *pack);
int Free_Group(int account,int group);
int Look_Group_Member(int fd,int account,int group);
int Look_Group(int fd,int account);
int Return_Group(int account,int group);
int Group_Chat(PACK * pack);
int Add_Group(int send_account,int group);
int Manage_Group(PACK *pack);
int Recv_File(int fd,PACK *pack);
int Send_File(int fd,PACK * pack);
int Manage_File(int send_account,int recv_account,const char *string);

int main()
{
    signal(SIGINT,signal_fun);
    accept_mysql(&mysql);
    struct sockaddr_in serv_addr,cli_addr;
    int sock_fd,conn_fd;
    socklen_t cli_len= sizeof(struct sockaddr_in);
    if((sock_fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
        my_err("socket",__LINE__);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bzero(&(serv_addr.sin_zero),sizeof(serv_addr.sin_zero));
    
    if(bind(sock_fd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr_in)) < 0)
        my_err("bind",__LINE__);

    if(listen(sock_fd,MAX_SIZE) == -1)
        my_err("listen",__LINE__);

    int epfd = epoll_create(10);
    struct epoll_event events[EPOLLMAX];
    struct epoll_event ev;
    int optval = 1;
    if(setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,(void*)&optval,sizeof(int))<0)
    {
        perror("setsocket\n");
        exit(1);
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sock_fd;
    
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,ev.data.fd,&ev) < 0)
        my_err("epoll ctl ",__LINE__);
    
    int nfds = 1,curfds = 1;
    PACK pack;
    while(1)
    {
        nfds = epoll_wait(epfd,events,curfds,-1);
        if(nfds < 0)
            my_err("epoll wait",__LINE__);
    
        int i = 0;
        for(i = 0;i < nfds;i++)
        {
            if(events[i].data.fd == sock_fd)
            {
                if((conn_fd = accept(sock_fd,(struct sockaddr *)&cli_addr,&cli_len)) < 0)
                    my_err("accept",__LINE__);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_fd;
                if(epoll_ctl(epfd,EPOLL_CTL_ADD,ev.data.fd,&ev) < 0)
                    my_err("epoll ctl",__LINE__);
                time_t ticks;
                ticks = time(NULL);    
                printf("套接字编号为%d连接至服务器\t\t\t\t%.24s\n",conn_fd,ctime(&ticks));
                curfds ++ ;
                continue;
            }
            
            
            if(events[i].events & EPOLLIN)
            {
                memset(&pack,0,sizeof(pack));
                int n = recv(events[i].data.fd,&pack,sizeof(pack),0);
                if( n < 0)
                {
                    close(events[i].data.fd);
                    perror("recv ");
                    continue;
                }
                if(pack.type == LOGIN)
                {
                    int ret_LO = -1;
                    CONN_FD = events[i].data.fd;
                    ret_LO = Login_Account(&pack,&mysql);
                    
                    time_t ticks;
                    ticks = time(NULL);
                    send(events[i].data.fd,&ret_LO,sizeof(ret_LO),0);
                    if(ret_LO == 1)
                    {
                        printf("账户 : %d登录至服务器\t\t\t\t%.24s\n",ACCOUNT,ctime(&ticks));
                        sleep(1);
 //..............................        
                         Offline_Messages(events[i].data.fd,pack.data.send_account);               
                        CONN_FD = 0;
                    }
                    continue;
                }
                else if(pack.type == REGISTERED)
                {
                    int ret_RE = -1;
                    ret_RE = Registed_Account(&pack,&mysql);
                    if(send(events[i].data.fd,&ret_RE,sizeof(ret_RE),0) < 0)
                    {
                        printf("send error,%d\n",__LINE__);
                        continue;
                    }
                    

                }
                else if(pack.type == FIND_PASSWORD)
                {
                    int ret_FP = -1;
                    ret_FP = Find_Password(&pack,&mysql);
                    if(ret_FP == 1)
                    {
                        send(events[i].data.fd,&pack,sizeof(pack),0);

                    }
                    else 
                    {
                        continue;
                    }
                }
                else if(pack.type == CHANGE_PASSWORD)
                {

                    int ret_CP = -1;
                    ret_CP = Change_Password(&pack,&mysql);
                    send(events[i].data.fd,&ret_CP,sizeof(ret_CP),0);


                }
                else if(pack.type == EXIT_CUI)
                {
                    curfds --;
                    time_t ticks;
                    ticks = time(NULL);
                    if(epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&ev) < 0)
                        my_err("epoll ctl",__LINE__);
                    printf("套接字编号为%d退出服务器\t\t\t\t\t%.24s\n",events[i].data.fd,ctime(&ticks));
                    close(events[i].data.fd);
                    continue;
                }
                else if(pack.type == EXIT_ACCOUNT)
                {

                    
                    time_t ticks;
                    ticks = time(NULL);
                    int ret_EA = -1;
                    printf("账户：%d 退出服务器\t\t\t\t%.24s\n",pack.data.send_account,ctime(&ticks));
                    pack.data.ret_AAA = Exit_Account(&pack,&mysql);
                    pack.data.recv_fd = 55;
                    pack.data.send_fd = 0;
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    curfds --;
                    
                    if(epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&ev) < 0)
                        my_err("epoll ctl",__LINE__);
                    printf("套接字编号为%d退出服务器\t\t\t\t\t%.24s\n",events[i].data.fd,ctime(&ticks));
                    close(events[i].data.fd);
                    continue;
                }

                else if(pack.type == SIGNAL_SEND)
                {
                    
                    
                    pack.data.ret_AAA = Signal_Chat(&pack);
                    //printf("%d : \n",ret_SC);
                    pack.data.recv_fd = 55;
                    pack.data.send_fd = 0;
                    if(send(events[i].data.fd,&pack,sizeof(pack),0) < 0)
                    {
                        printf("%d : error \n",__LINE__);
                    }

                    continue;
                }
                else if(pack.type == GROUP_CHAT)
                {
                   
                    pack.data.ret_AAA = Group_Chat(&pack);
                    pack.data.recv_fd = 55;
                    
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    continue;
                }
                else if (pack.type == ADD_FRIEND)
                {
                    
                    
                    pack.data.ret_AAA = Add_Friends(&pack,&mysql);
                //处理返回值
                    pack.data.recv_fd = 55;
                    if(send(events[i].data.fd,&pack,sizeof(pack),0) < 0)
                    {
                        printf("%d : send_error",__LINE__);
                    }
 
                    continue;
                }
                else if(pack.type == MANAGE_FRIEND)
                {
                    if(pack.data.recv_fd == 1)
                    {
                        pack.data.ret_AAA = Manage_Friend(pack.data.recv_account,pack.data.send_account);
                        pack.data.recv_fd = 55;
                        pack.data.send_fd = 0;
                        send(events[i].data.fd,&pack,sizeof(pack),0);
                    }
                    continue;

                }
                else if(pack.type == MANAGE_GROUP)
                {
                    if(pack.data.recv_fd == 1)
                    {
                        pack.data.ret_AAA = Manage_Group(&pack);
                        pack.data.recv_fd = 55;
                        pack.data.send_fd = 0;
                        send(events[i].data.fd,&pack,sizeof(pack),0);
                        
                    }
                    continue;
                }
                else if(pack.type == ADD_GROUP)
                {
                    pack.data.recv_fd = 55;
                    pack.data.send_fd = 0;
                    pack.data.ret_AAA = Add_Group(pack.data.recv_account,pack.data.send_account);
                }
                else if(pack.type == DEL_FRIEND)
                {
                   
                    pack.data.ret_AAA = Del_Friend(&pack,&mysql);
                     pack.data.recv_fd = 55;
                    pack.data.send_fd = 0;
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    continue;
                }
                else if(pack.type == LOOK_FRIEND)
                {
                    pack.data.recv_fd = 66;
                    pack.data.send_fd = 0; 
                    send(events[i].data.fd,&pack,sizeof(pack),0);                   
                    Look_Friend(events[i].data.fd,pack.data.send_account);
                    continue;
                }
                else if (pack.type == LOOK_CHAT_RECORD)
                {
                   
                    Look_Chat_Record(events[i].data.fd,pack.data.send_account);
                    
                    continue;
                }
                else if(pack.type == SET_REALTION)
                {
                    pack.data.recv_fd = 55;
                    pack.data.ret_AAA = Set_Realtion(pack.data.send_account,pack.data.recv_account,pack.data.send_fd);
                    pack.data.send_fd = 0;
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    continue;

                }
                else if(pack.type == CREATE_GROUP)
                {
                    pack.data.recv_fd = 55;
                    pack.data.ret_AAA = Registed_Group(&pack);
                    pack.data.send_fd = 0;
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    continue;
                }
                else if(pack.type == FREE_GROUP)
                {
                    pack.data.recv_fd = 55;
                    pack.data.ret_AAA = Free_Group(pack.data.recv_account,pack.data.send_account);
                    pack.data.send_fd = 0;
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    continue;
                }
                else if(pack.type == LOOK_GROUP_MEMBER)
                {
                    pack.data.ret_AAA = Look_Group_Member(events[i].data.fd,pack.data.recv_account,pack.data.send_account);
                    pack.data.recv_fd = 55;
                    pack.data.send_fd = 0;
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    
                    continue;
                }
                else if(pack.type == LOOK_GROUP)
                {
                    Look_Group(events[i].data.fd,pack.data.send_account);

                    continue;
                }
                else if(pack.type == RETURN_GROUP)
                {
                    pack.data.recv_fd = 55;
                    pack.data.send_fd = 0;
                    pack.data.ret_AAA = Return_Group(pack.data.recv_account,pack.data.send_account);
                    send(events[i].data.fd,&pack,sizeof(pack),0);
                    continue;
                }
                else if(pack.type == SEND_FILE)
                {
                    
                    
    
                    pack.data.ret_AAA = Recv_File(events[i].data.fd,&pack);
                    continue;

                }
                else if(pack.type == RECV_FILE)
                {
                    Send_File(events[i].data.fd,&pack);
                    continue;
                }
                else if(pack.type == MANAGE_FILE)
                {
                    Manage_File(pack.data.send_account,pack.data.recv_account,pack.data.send_user);
                }
                else if(pack.type == MANAGE_MESSAGE)
                {
                    Manage_Message(pack.data.send_account,pack.data.recv_account,pack.data.write_buf);
                    continue;
                }
                else if(pack.type == LOOK_MESSAGEBOX)
                {
                    Look_MessageBox(events[i].data.fd,pack.data.send_account);
                    
                    continue;
                }
              /*
                else if (pack.type == MANAGE_FRIEND)
                {
                    
                    if(pack.data.recv_fd == 1)
                    {
                        char string[1024] = "\0";
                        sprintf(string,"insert into friends values(%d,%d,%d)",pack.data.recv_account,pack.data.send_account,1);
                        mysql_query(&mysql,string);
                        memset(string,0,sizeof(string));
                        sprintf(string,"insert into friends values(%d,%d,%d)",pack.data.send_account,pack.data.recv_account,1);
                        mysql_query(&mysql,string);
                        ret_MF = 1;
                        send(events[i].data.fd,&ret_MF,sizeof(ret_MF),0);
                    }
                    else 
                    {
                        
                        send(events[i].data.fd,&ret_MF,sizeof(ret_MF),0);
                        
                    }
                }
                */

/*
                else if(pack.type == 1)   // 登录
                {
                    /*登陆 通过DATA中的send_account 在user_data表中查找密码
                        密码暂且存放在send_buf 中，strcmp比较
                        返回一个int型数据（0 失败，重新输入）
                                       （1 成功）
                               
                }
                else if (pack.type == 2)   //注册
                {
                    /* 通过传入的type直接判断，
                    把账户存放在send_account 中，密码存放在send_user中，
                    密保问题放在read_buf中，密保答案放在write_buf中
                    操作数据库添加至user_data表中
                    
                }
                else if(pack.type == 3)   //修改密码
                {
                    /*
                        将账户存在send_account 中，密保答案放在send_user中，修改后的密码放在write_buf中
                        操作数据库
                        strcmp   若密保答案正确，
                        update set user_data password = '   ' where account
                        修改成功返回1.，失败返回0 
                    
                }
                else if(pack.type == 4)  //添加好友
                {
                    /*
                        操作数据库 user_data表 查找 发送好友请求的客户端昵称
                        将请求发送到对应的消息盒子表
                        等待接受客户端处理，
                        strcmp比较是否同意，
                        （同意，拒绝，忽略，忽略处理类似与拒绝处理，只是不给发送请求的客户端回应）
                    
                }
                else if (pack.type == 5)   //查看好友列表
                {
                    /*
                        select friend_user from friends where user = '   ';
                        获取到的friend_user作为下一次mysql的语句
                        select (account,nickname,users_state) from user_data where account == 'friend_user';
                        获取到在线 客户端彩色打印
                    
                }
                else if(pack.type == 6)   // 查看聊天记录
                {
                    /*
                        select (send_user,messages) from chat_messages where recv_user = ' *****'
                    
                } 
                else if(pack.type == 7)   //删除好友
                {
                    /*
                        从客户端接收到要删除的好友账户,保存到DATA.send_account和该账户保存到DATA.recv.account
                        delete from friends where user = '当前账户' friend_user = '要删除的好友账户'
                        修改消息表 该账户无法查看删除好友的信息
                        update set chat_messages recv_can_look =0 where recv_user = '该账户'
                    
                }
                else if (pack.type == 8)   //设置好友状态，(特别关心，黑名单，取消特别关心)
                {
                    /*
                        从客户端接收到要设置的账户存到DATA.send_account
                        设置friends表的reation值   （0.普通 1.特别关心，-1.取消特别关心，2.移出黑名单 -2.加入黑名单）
                    
                }
                else if (pack.type == 9)   //创建群         1.群主  2.管理员  0.群成员
                {
                    /*
                        从客户端获取群名，设置该账户群地位为群主（1）
                        得到一个群号存在group_account 
                        加入该账户的账户和昵称 以及群地位
                        insert
                    
                }
                else if(pack.type == 10)  //解散群
                {
                    /*
                        在group表中判断该账户的群地位
                        if(group_state == 1)
                            delete from group_member where group_account ='   ';
                        else 
                            send()  错误提示给客户端
                    
                }
                else if(pack.type == 11)   // 群聊
                {
                    /*
                        通过群号获取到好友账户
                        给该账户的消息盒子发送数据
                    
                }
                else if(pack.type == 12)  //查看群成员
                {
                    /*
                        select (group_member_account  , group_member_nickname  , group_state) from group_member where group_account ='  ';
                    
                }
                else if(pack.type == 13)     //查看以加群
                {
                    /*
                        通过账户查找群号以及群名
                        select (group_account ,group_name) from group_member where group_member_account ='该账户'
                    *
                }
                else if(pack.type == 14)   //设置群管理员
                {
                    /*
                        查看该账户是否为群主
                        if（是）
                            则可以修改group_member 表中 group_state 的信息
                            update
                        else
                            send
                            返回错误提示信息
                    
                }
                else if(pack.type == 15)   // 踢人
                {
                    /*
                        查看该账户是否为群主
                        if（是）
                            通过群成员账户删除信息
                        else
                            返回错误提示信息
                            send
                    *
                }
                else if(pack.type == 16)   //私聊
                {
                    /*
                        通过账户把消息发到消息盒子，等待对方接受
                    *
                }
                
 */               
                
                

            }
        }
    }

    
    

}


void signal_fun(int signo)
{
    char string[1024] = "\0";
    sprintf(string,"update user_data set user_socket = 0,user_state = 0");
    mysql_query(&mysql,string);
    mysql_close(&mysql);
    mysql_library_end();
    exit(-1);
}

int Login_Account(PACK *pack,MYSQL *mysql)
{
    char string[1024] = "\0";
    ACCOUNT = pack->data.send_account;
    sprintf(string,"select password from user_data where account = %d",pack->data.send_account);
    if((mysql_query(mysql,string)) == 0)
    {
        MYSQL_RES *result = mysql_store_result(mysql);
        MYSQL_ROW row = mysql_fetch_row(result);
        if(row == NULL)
            return -1;
        if(strcmp(row[0],pack->data.send_user) == 0)
        {
            memset(string,0,sizeof(string));
            sprintf(string,"update user_data set user_state = 1,user_socket = %d where account = %d",CONN_FD,ACCOUNT);
            if((mysql_query(mysql,string)) != 0)
                my_err("update user_data failed!",__LINE__);
            return 1;
        }
        else 
        {
            return -1;
        }
    }
    else return -1;
}

int Registed_Account(PACK *pack,MYSQL *mysql)
{
    int i;
    int account = 0;
    int a[100] = {0};
    srand(time(NULL));
    for(i = 0;i < 100;i++)
    {
        a[i] = rand()%100000000 + 10000000;

    }
    i = rand()%100;
    account = a[i];
    char string[1024] = "\0";
    sprintf(string,"insert into user_data values(%d,'%s','%s',0,0,'%s','%s')",account,pack->data.recv_user,pack->data.send_user,pack->data.read_buf,pack->data.write_buf);
    
    if((mysql_query(mysql,string)) == 0)
    {
        return account;

    }
    else 
        return -1;

}

int Find_Password(PACK *pack,MYSQL *mysql)
{
    char string[1024] = "\0";
    sprintf(string,"select mibao,daan,password from user_data where account = %d",pack->data.send_account);
    if(mysql_query(mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(mysql);
        MYSQL_ROW row;

  /*      if((row = mysql_fetch_row(result)) != NULL)
        {
            strcpy(pack->data.write_buf,row[0]);
            while((row = mysql_fetch_row(result)) != NULL)
            {
                
                strcpy(pack->data.read_buf,row[1]);
                strcpy(pack->data.send_user,row[2]);
            }
        }
        else 
        {
            return -1; 
        }
*/
       while((row = mysql_fetch_row(result)) != NULL)
        {

            
            strcpy(pack->data.write_buf,row[0]);
            strcpy(pack->data.read_buf,row[1]);

            strcpy(pack->data.send_user,row[2]);

        }
        
        return 1;
  
    }

    return -1;
}

int Change_Password(PACK *pack,MYSQL * mysql)
{
    char string[1024] = "\0";
    sprintf(string,"update user_data set password = '%s' where account = %d",pack->data.send_user,pack->data.send_account);
    
    if(mysql_query(mysql,string) == 0)
    {
        return 1;
    }
    else 
    {
        return -1;
    }
}

int Exit_Account(PACK *pack,MYSQL *mysql)
{
    char string[1024] = "\0";
    sprintf(string,"update user_data set user_state = 0,user_socket = 0 where account = %d",pack->data.send_account);
    printf("%s\n",string);
    if(mysql_query(mysql,string) == 0)
    {
        return 1;
    }
    else 
        return -1;
}


int getNowTime(char *nowTime)
{
    char acYear[5] = {0};
    char acMonth[5] = {0};
    char acDay[5] = {0};
    char acHour[5] = {0};
    char acMin[5] = {0};
    time_t now;
    struct tm* timenow;
    time(&now);
    timenow = localtime(&now);
    strftime(acYear,sizeof(acYear),"%Y",timenow);
    strftime(acMonth,sizeof(acMonth),"%m",timenow);
    strftime(acDay,sizeof(acDay),"%d",timenow);
    strftime(acHour,sizeof(acHour),"%H",timenow);
    strftime(acMin,sizeof(acMin),"%M",timenow);
    strncat(nowTime, acYear, 4);
    strncat(nowTime, acMonth, 2);
    strncat(nowTime, acDay, 2);
    strncat(nowTime, acHour, 2);
    strncat(nowTime, acMin, 2);
    return 0;
}


//return 1  没有该好友，可添加
//return 0  有该好友，不能重复添加
int Add_Friends(PACK *pack,MYSQL *mysql)
{
    
    int ret_FA = Find_Account(pack->data.recv_account);
    printf("ret_FA%d\n",ret_FA);
    if(ret_FA == -1)
        return -1;   
    int ret_FFR = Find_Friend_Realtion(pack);
    printf("ret_FFR : %d\n",ret_FFR);
    if(ret_FFR == 3 && ret_FA == 1)
    {
        
        //保存到chat_messages,
        char nowTime[32] = {0};
        getNowTime(nowTime);
        char string[1024] = "\0";
        sprintf(string,"select user_state,user_socket from user_data where account = %d",pack->data.recv_account);
        mysql_query(mysql,string);
        
            MYSQL_RES * result = mysql_store_result(mysql);
            MYSQL_ROW row = mysql_fetch_row(result);
            memset(string,0,sizeof(string));
                sprintf(string,"insert into chat_messages values(%d,%d,'%s',%d,%d,%d,'%s')",
                pack->data.send_account,pack->data.recv_account,"好友请求",1,1,3,nowTime);
                if(mysql_query(mysql,string) < 0)
                    printf("query error!\n");
           
                if(atoi(row[0]) == 1)
                {
                    printf("........");
                    send(atoi(row[1]),pack,sizeof(*pack),0);
                    pack->data.send_fd = 0;

                    return 1;

                }
                pack->data.send_fd = 0;
                return 1;
    }
    else if(ret_FFR == 1 || ret_FFR == 2)
        return 0;
    else 
        return -1;


}

//设计思路：
//不是好友关系或是黑名单 返回-1  是好友发送消息 返回0
int Signal_Chat(PACK *pack)
{
        int ret_FFR = -1;
        //pack.data.send_fd         1.xiaoxi     2.wenjian      3.haoyouqingqiu
        ret_FFR = Find_Friend_Realtion(pack);
        //保存到chat_messages,
        if(ret_FFR == 3 || ret_FFR == 0)
        {
            return -1;
        }
        if(ret_FFR == 1 || ret_FFR == 2)
        {
            char nowTime[32] = {0};
            getNowTime(nowTime);
            char string[1024] = "\0";
            
            // ret_FFR  haoyouguanxi     pack->data.send_fd xiaoxileixing   1.weichakan
            pack->data.recv_fd = ret_FFR;
            sprintf(string,"insert into chat_messages values(%d,%d,'%s',%d,%d,%d,'%s')"   
                ,pack->data.send_account,pack->data.recv_account,pack->data.write_buf,1,ret_FFR,pack->data.send_fd,nowTime);
            
            
            time_t ticks;
            ticks = time(NULL);
            if(mysql_query(&mysql,string) == 0)
            {
                if(pack->data.send_fd == 1)
                    printf("%.24s...%d发送给%d消息：%s\n",ctime(&ticks),pack->data.send_account,pack->data.recv_account,pack->data.write_buf);
                else if(pack->data.send_fd == 3)
                    printf("%.24s...%d发送给%d好友请求\n",pack->data.send_account,pack->data.recv_account);
                else if(pack->data.send_fd == 2)
                    printf("%.24s...%d发送给%d文件\n",pack->data.send_account,pack->data.recv_account);
            }
        
        //判断好友是否在线     如果在线，直接发送，修改chat_messages表中消息位为已发送
        //查看好友关系
        
            
            sprintf(string,"select user_state,user_socket from user_data where account = %d",pack->data.recv_account);
            mysql_query(&mysql,string);
            MYSQL_RES * result = mysql_store_result(&mysql);
            MYSQL_ROW row = mysql_fetch_row(result);
            
            
            if(atoi(row[0]) != 0 && pack->data.send_fd == 1)
            {

//尤其注意该处传参 
                pack->data.recv_fd = 0;               
                send(atoi(row[1]),pack,sizeof(*pack),0);
                return 0;
            }
            

        }
        else 
        {
            return 0;
        }
        if(pack->data.send_fd == 2)
        {
            /*
            文件
            */
        }
        return 0;
        
    

}

//查看是否是好友关系，如果是返回关系标志位 ，不是返回 3 
//不是好友  3    黑名单 0 好友 1 特别关心2
int Find_Friend_Realtion(PACK *pack)
{
    if((Find_Account(pack->data.recv_account)) == -1)
        return 0;

    char string[1024] = "\0";
    sprintf(string,"select realtion from friends where user = %d and friend_user = %d",pack->data.send_account,pack->data.recv_account);

    if(mysql_query(&mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(&mysql);
        MYSQL_ROW  row = mysql_fetch_row(result);
        if(row == NULL)
            return 3;    //不是好友关系
        else 
        {
            int i = atoi(row[0]);
            return i;
        }
    }
    else 
        return -1;
}


//查看该账户是否存在，如果存在返回1,不存在返回-1
int Find_Account(int account)
{
    char string[1024] = "\0";
    strcpy(string,"select account from user_data\0");
    if(mysql_query(&mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(&mysql);
        MYSQL_ROW row;
        while((row = mysql_fetch_row(result)) != NULL)
        {
            if(atoi(*row) == account)
            {
                printf("cunzai\n");
                return 1;
            }
        }
        return -1;
    }
    else 
        return -1;
}


int Del_Friend(PACK * pack,MYSQL *mysql)
{
    int ret_FFR = Find_Friend_Realtion(pack);
    if(ret_FFR == 3)
        return -1;
    if(ret_FFR == 0)
        return 0;
    else 
    {
        char string[1024] = "\0";
        sprintf(string,"delete from friends where friend_user = %d",pack->data.recv_account);
        if(mysql_query(mysql,string) == 0)       
            return 1;
        else 
            return -2;
    }
}

int Offline_Messages(int fd,int account)
{
    int i = 0;
    char string[1024] = "\0";
    sprintf(string,"select message_type from chat_messages where  recv_user = %d and send_can_look = 1",account);
    if(mysql_query(&mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(&mysql);
        MYSQL_ROW row = NULL;
        
        while((row = mysql_fetch_row(result)) != NULL)
        {
            i++;
        }
    }
    send(fd,&i,sizeof(i),0);
    return 0;
}

int Look_MessageBox(int fd,int account)
{
    BOX box;
    memset(&box,0,sizeof(box));
    char string[1024] = "\0";
    sprintf(string,"select send_user,messages,message_type,date from chat_messages where  recv_user = %d and send_can_look = 1",account);
    if(mysql_query(&mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(&mysql);
        MYSQL_ROW row = NULL;
        while((row = mysql_fetch_row(result)) != NULL)
        {
            box.number ++;
            box.type[box.number] = atoi(row[2]);
        
            if(box.type[box.number] == 1)                    //消息
            {
                box.talk_number ++;
                box.message_account[box.talk_number] = atoi(row[0]);
                strcpy(box.message_date[box.talk_number],row[3]);
                strcpy(box.messages[box.talk_number],row[1]);
                printf("%d : %s \n",box.talk_number,box.messages[box.talk_number]);
            }
            else if(box.type[box.number] == 3)            //好友请求
            {
                box.friend_number ++;
                box.plz_account[box.friend_number] = atoi(row[0]);
                strcpy(box.plz_date[box.friend_number],row[3]);
            }
            else if(box.type[box.number] == 4)
            {
                box.plz_number ++;
                box.plz_group[box.plz_number] = atoi(row[0]);
                strcpy(box.group_date[box.plz_number],row[3]);
            }
            else if(box.type[box.number] == 2)
            {
//发送文件        
                box.talk_number ++;
                box.message_account[box.talk_number] = atoi(row[0]);
                strcpy(box.message_date[box.talk_number],row[3]);
                strcpy(box.messages[box.talk_number],row[1]);
            }

        }
    }
    PACK pack;
    pack.data.recv_fd = 66;
    send(fd,&pack,sizeof(pack),0);
    sleep(1);
    send(fd,&box,sizeof(box),0);
}

int Manage_Friend(int recv_account,int send_account)

{
    char string[1024] = "\0";
    sprintf(string,"insert into friends values(%d,%d,%d)",recv_account,send_account,1);
    if(mysql_query(&mysql,string) < 0)
    {
        printf("%d : query friends error!\n",__LINE__);
        return -1;
    }
    sprintf(string,"insert into friends values(%d,%d,%d)",send_account,recv_account,1);
    if(mysql_query(&mysql,string) < 0)
    {
        printf("%d : quert friends error! \n",__LINE__);
        return -1;
    }
    PACK pack;
    pack.type = SIGNAL_SEND;
    pack.data.send_fd = 1;
    pack.data.recv_fd = 0;
    pack.data.ret_AAA = 0;
    pack.data.send_account = recv_account;
    pack.data.recv_account = send_account;
    strcpy(pack.data.write_buf,"已添加该用户为好友！");
    Signal_Chat(&pack);
    return 1;
}

int Look_Friend(int fd,int account)
{
   
    printf("************\n");
    BOX box;
    MYSQL_RES *result1;
    MYSQL_ROW row1;
    char string[1024] = "\0";
    char string1[1024] = "\0";
    sprintf(string,"select friend_user,realtion from friends where user = %d",account);
    mysql_query(&mysql,string);
    MYSQL_RES *result = mysql_store_result(&mysql);
    MYSQL_ROW row = NULL;
    while((row = mysql_fetch_row(result)) != NULL)
    {
        sprintf(string1,"select user_state from user_data where account = %d",atoi(*row));
        mysql_query(&mysql,string1);
        result1 = mysql_store_result(&mysql);
        row1 = mysql_fetch_row(result1);
        box.number ++;
        box.plz_account[box.number] = atoi(row[1]);
        box.type[box.number] = atoi(*row1);
        box.message_account[box.number] = atoi(row[0]);
        
        

    }
    send(fd,&box,sizeof(box),0);
}

int Manage_Message(int send_account,int recv_account,const char * string)
{
    char string1[1024] = "\0";
    sprintf(string1,"update chat_messages set send_can_look = 0 where send_user = %d and recv_user = %d and messages = '%s'",send_account,recv_account,string);
    if(mysql_query(&mysql,string1))
    {
        printf("%d : update error!\n",__LINE__);
    }

}

int Manage_File(int send_account,int recv_account,const char *string)
{
    char string1[1024] = "\0";
    sprintf(string1,"update chat_messages set send_can_look = 0 where send_user = %d and recv_user = %d and messages = '%s'"
    ,send_account,recv_account,string);
    mysql_query(&mysql,string1);
}
int Look_Chat_Record(int fd,int account)
{
    BOX box;
    memset(&box,0,sizeof(box));
    char string[1024] = "\0";
    sprintf(string,"select send_user,messages,message_type,date from chat_messages where  recv_user = %d and recv_can_look = 1",account);
    if(mysql_query(&mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(&mysql);
        MYSQL_ROW row = NULL;
        while((row = mysql_fetch_row(result)) != NULL)
        {
            box.number ++;
            box.type[box.number] = atoi(row[2]);
        
            if(box.type[box.number] == 1)                    //消息
            {
                box.talk_number ++;
                box.message_account[box.talk_number] = atoi(row[0]);
                strcpy(box.message_date[box.talk_number],row[3]);
                strcpy(box.messages[box.talk_number],row[1]);
                printf("%d : %s \n",box.talk_number,box.messages[box.talk_number]);
            }
            else if(box.type[box.number] == 3)            //好友请求
            {
                box.friend_number ++;
                box.plz_account[box.friend_number] = atoi(row[0]);
                strcpy(box.plz_date[box.friend_number],row[3]);
            }
            else if(box.type[box.number] == 4)
            {
                box.plz_number ++;
                box.plz_group[box.plz_number] = atoi(row[0]);
                strcpy(box.group_date[box.plz_number],row[3]);
            }
            else if(box.type[box.number] == 2)
            {
//发送文件          
                box.talk_number ++;
                box.message_account[box.talk_number] = atoi(row[0]);
                strcpy(box.message_date[box.talk_number],row[3]);
                strcpy(box.messages[box.talk_number],row[1]);
            }

        }
    }
    PACK pack;
    pack.data.recv_fd = 66;
    send(fd,&pack,sizeof(pack),0);
    sleep(1);
    send(fd,&box,sizeof(box),0);
}     



//-1   不是好友   1 设置成功 
int Set_Realtion(int send_account,int recv_account,int state)
{
    
    if((Find_Account(recv_account)) == -1)
        return 0;

    char string[1024] = "\0";
    sprintf(string,"select realtion from friends where user = %d and friend_user = %d",send_account,recv_account);

    if(mysql_query(&mysql,string) == 0)
    {
        MYSQL_RES *result = mysql_store_result(&mysql);
        MYSQL_ROW  row = mysql_fetch_row(result);
        if(row == NULL)
            return -1;    //不是好友关系
        else 
        {
           memset(string,0,sizeof(string));
           sprintf(string,"update friends set realtion = %d where user = %d and friend_user = %d",state,send_account,recv_account);
           if(mysql_query(&mysql,string) == 0)
           {
               return 1;
           }
           else 
            return 0;
        }
    }
    else 
        return 0;
}

int Registed_Group(PACK *pack)
{
    int i;
    int group_account = 0;
    int a[100] = {0};
    srand(time(NULL));
    for(i = 0;i < 100;i++)
    {
        a[i] = rand() %10000000 + 1000000;

    }
    i = rand()% 100;
    group_account = a[i];
    char string[1024] = "\0";
    sprintf(string,"insert into group_data values(%d,'%s',%d)",group_account,pack->data.send_user,pack->data.send_account);
    mysql_query(&mysql,string);

    memset(string,0,sizeof(string));
    sprintf(string,"select nickname from user_data where account = %d",pack->data.send_account );
    char nickname[50]= "\0";
    mysql_query(&mysql,string);
    MYSQL_RES * result = mysql_store_result(&mysql);
    MYSQL_ROW row = mysql_fetch_row(result);
    strcpy(nickname,*row);
    memset(string,0,sizeof(string));
    sprintf(string,"insert into group_member values(%d,'%s',%d,'%s',%d)",group_account,pack->data.send_user,pack->data.send_account,nickname,1);
    mysql_query(&mysql,string);
    return group_account;
}

int Free_Group(int account,int group)
{
    char string[1024] = "\0";
    sprintf(string,"select group_account from group_data where group_number = %d",account);
    mysql_query(&mysql,string);
    
    MYSQL_RES * result = mysql_store_result(&mysql);
    MYSQL_ROW row;
    int ret = 0; 
    while((row = mysql_fetch_row(result)) != NULL)
    {
        if(group == atoi(*row))
        {
            ret = 1;
            break;
        }
    }
    if(ret == 0)
        return -1;
    memset(string,0,sizeof(string));
    sprintf(string,"delete from group_data where group_number = %d and group_account = %d",account,group);
    
    if(mysql_query(&mysql,string) < 0)
    {
        printf("%d : delete error! ",__LINE__);
        return -1;
    }
    memset(string,0,sizeof(string));
    sprintf(string,"delete from group_member where group_account = %d",group);
    
    mysql_query(&mysql,string);
    return 1;

}

//-1 无权查看   1 可查看
int Look_Group_Member(int fd,int account,int group)
{
    BOX box;
    memset(&box,0,sizeof(box));
    char string[1024] = "\0";
    sprintf(string,"select group_account from group_member where group_member_account =%d",account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    MYSQL_RES * result = mysql_store_result(&mysql);
    MYSQL_ROW row;
    int ret = 0; 
    while((row = mysql_fetch_row(result)) != NULL)
    {
        printf("%d\n",atoi(*row));
        if(group == atoi(*row))
        {
            ret = 1;
            break;
        }
    }
    if(ret == 0)
        return -1;
    memset(string,0,sizeof(string));
    sprintf(string,"select group_member_account,group_member_nickname,group_state from group_member where group_account = %d",group);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    while((row = mysql_fetch_row(result)) != NULL)
    {
        box.number ++;
        box.message_account[box.number] = atoi(row[0]);
        box.type[box.number] = atoi(row[2]);
        strcpy(box.message_date[box.number],row[1]);
    }
    memset(string,0,sizeof(string));
    sprintf(string,"select group_name from group_member where group_account = %d",group);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    row = mysql_fetch_row(result);
    strcpy(box.message_date[0],*row);
    box.message_account[0] = group;


    PACK pack;
    pack.data.send_fd = 0;
    pack.data.recv_fd = 66;
    send(fd,&pack,sizeof(pack),0);

    send(fd,&box,sizeof(box),0);
    return 1;

}


int Look_Group(int fd,int account)
{
    BOX box;
    memset(&box,0,sizeof(box));
    char string[1024] = "\0";
    sprintf(string,"select group_account,group_name,group_state from group_member where group_member_account = %d",account);
    mysql_query(&mysql,string);
    MYSQL_RES * result = mysql_store_result(&mysql);
    MYSQL_ROW row;
    while((row = mysql_fetch_row(result)) != NULL)
    {
        box.number ++;
        box.message_account[box.number] = atoi(row[0]);
        box.type[box.number] = atoi(row[2]);
        strcpy(box.message_date[box.number],row[1]);
    }

    PACK pack;
    pack.data.recv_fd = 66;
    pack.data.send_fd = 0;
    send(fd,&pack,sizeof(pack),0);

    send(fd,&box,sizeof(box),0);
    return 0;

}


  //2  是群主不能退出群，只能解散群
  //1  退出成功
int Return_Group(int account,int group)
{
    char string[1024] = "\0";
    sprintf(string,"select group_state from group_member where group_member_account = %d and group_account = %d",account,group);
    mysql_query(&mysql,string);
    MYSQL_RES * result = mysql_store_result(&mysql);
    MYSQL_ROW row;
    row = mysql_fetch_row(result);
    if(1 == atoi(*row))
        return 2;
    sprintf(string,"delete from group_member where group_member_account = %d and group_account = %d",account,group);
    mysql_query(&mysql,string);
    return 1;
    
}


//-1 未加入该群
//1  发送消息成功
int Group_Chat(PACK * pack)
{
    char string[1024] = "\0";
    sprintf(string,"select group_account from group_member where group_member_account = %d",pack->data.recv_account);
    mysql_query(&mysql,string);
    MYSQL_RES *result = mysql_store_result(&mysql);
    MYSQL_ROW row ;
    int ret = 0;
    while((row = mysql_fetch_row(result)) != NULL)
    {
        if(atoi(*row) == pack->data.send_account)
        {
            ret = 1;
            break;
        }
    }
    if(ret != 1)
        return -1;

    memset(string,0,sizeof(string));
    char nowTime[32] = {0};
    MYSQL_RES *result1;
    MYSQL_ROW row1;
    sprintf(string,"select group_member_account from group_member where group_account = %d",pack->data.send_account);
    printf("%s\n",string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    while((row = mysql_fetch_row(result)) != NULL)
    {
        if(atoi(*row) == pack->data.recv_account)
            continue;
        memset(nowTime,0,sizeof(nowTime));
            getNowTime(nowTime);
        sprintf(string,"insert into chat_messages values(%d,%d,'%s',%d,%d,%d,'%s')",pack->data.send_account,atoi(*row),pack->data.write_buf,1,1,1,nowTime);
        printf("%s\n",string);
        if(mysql_query(&mysql,string) < 0)
        {
            printf("%d : send error!\n",__LINE__);
            continue;
        }
        memset(string,0,sizeof(string));
        sprintf(string,"select user_socket from user_data where account = %d",atoi(*row));
        printf("%s\n",string);
        mysql_query(&mysql,string);
        result1 = mysql_store_result(&mysql);
        row1 = mysql_fetch_row(result1);
        if(atoi(*row1) != 0)
        {
            pack->data.recv_fd = 0;
            pack->data.send_fd = 4;
            send(atoi(*row1),pack,sizeof(*pack),0);
            continue;
        }
    }
    return 1;
}

// 1 已加入   2 等待同意  -1 不存在
int Add_Group(int send_account,int group)
{

    //查看群是否存在
    char string[1024] = "\0";
    strcpy(string,"select group_account from group_data\0");
    
    mysql_query(&mysql,string);
    printf("%d : %s\n",__LINE__,string);
    MYSQL_RES *result;
    int ret = 0;
    MYSQL_ROW row;
    result = mysql_store_result(&mysql);
    while((row = mysql_fetch_row(result)) != NULL)
    {
        if(atoi(*row) == group)
        {
            ret = 1;
            break;
        }
    }
    if(ret != 1)
        return -1;


    //查看该成员是否已经加入群
    memset(string,0,sizeof(string));
    sprintf(string,"select group_name from group_member where group_account = %d and group_member_account = %d",group,send_account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    
    result = mysql_store_result(&mysql);
    row = mysql_fetch_row(result);
    if(row != NULL)
        return 1;


    //找到群主
    memset(string,0,sizeof(string));
    sprintf(string,"select group_number from group_data where group_account = %d",group);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    row = mysql_fetch_row(result);
    int recv_account = atoi(*row);

    //发送给群主请求
    memset(string,0,sizeof(string));
    sprintf(string,"select user_socket from user_data where account = %d",recv_account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    row = mysql_fetch_row(result);

    if(atoi(*row) != 0)
    {
        PACK pack;
        pack.data.send_fd = 4;
        pack.data.recv_fd = 0;
        send(atoi(*row),&pack,sizeof(pack),0);

    }

    //保存到消息表
    char nowTime[32] = {0};
    getNowTime(nowTime);
    memset(string,0,sizeof(string));
    sprintf(string,"insert into chat_messages values(%d,%d,'%s',%d,%d,%d,'%s')",send_account,recv_account,"群聊请求",1,1,4,nowTime);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    return 2;

}


int Manage_Group(PACK *pack)
{
    //找到群账户 群昵称
    char string[1024] = "\0";
    sprintf(string,"select group_account,group_name from group_data where group_number = %d",pack->data.recv_account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    MYSQL_RES *result = mysql_store_result(&mysql);
    MYSQL_ROW row = mysql_fetch_row(result);
    int group_account = atoi(row[0]);
    char group_nickname[50] = "\0";
    strcpy(group_nickname,row[1]);

    //找到用户昵称
    memset(string,0,sizeof(string));
    sprintf(string,"select nickname from user_data where account = %d",pack->data.send_account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    row = mysql_fetch_row(result);
    char user_nickname[50] = "\0";
    strcpy(user_nickname,*row);


    //插入群信息表
    memset(string,0,sizeof(string));
    sprintf(string,"insert into group_member values(%d,'%s',%d,'%s',%d)",group_account,group_nickname,pack->data.send_account,user_nickname,3);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    
    return 1;

    
}



// 0 接收失败   1  接收成功
int Recv_File(int fd,PACK *pack)
{
    
    char buffer[1024] = "\0";
    char File_Name[50] = "\0";
                    
    sprintf(File_Name,"recv%s",pack->data.send_user);
    FILE * fp = fopen(File_Name,"w+");
    fclose(fp);
    memset(buffer,0,sizeof(buffer));
    int length = 0;
    int i = 0;
    //接收文件
     while( (length = recv(fd,buffer,1024,0) ) > 0 )
    {
               
        i++;
        printf("server : %d\n",length);
        if(strcmp(buffer,"exit25908") == 0)
            break;
        if(length <0)
        {
            printf("Recieve Data From client Failed\n");
            return 0;
        }
        fp = fopen(File_Name,"a+");
        int write_length = fwrite(buffer,sizeof(char),length,fp);
                        
        if(write_length < length)
        {
            printf("write failed\n");
            return 0;
        }
        memset(buffer,0,sizeof(buffer));
        fclose(fp);
    }
    
    //查看好友关系
    
    printf("*******************************8\n");
    char string[1024] = "\0";
    sprintf(string,"select realtion from friends where user = %d and friend_user = %d",pack->data.send_account,pack->data.recv_account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    MYSQL_RES *result = mysql_store_result(&mysql);
    MYSQL_ROW row = mysql_fetch_row(result);

    int realtion = atoi(*row);
    int recv_can_look;
    if(realtion == -1)
    
         recv_can_look = 0;
    
    else 
        recv_can_look = 1;

        //插入消息表
    memset(string,0,sizeof(string));
    char nowTime[32] = {0};
        getNowTime(nowTime);
    sprintf(string,"insert into chat_messages values(%d,%d,'%s',%d,%d,%d,'%s')",pack->data.recv_account,pack->data.send_account,File_Name,realtion,recv_can_look,2,nowTime);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);


    //查看账户是否在线
    memset(string,0,sizeof(string));
    sprintf(string,"select user_socket from user_data where account = %d",pack->data.send_account);
    printf("%d : %s\n",__LINE__,string);
    mysql_query(&mysql,string);
    result = mysql_store_result(&mysql);
    row = mysql_fetch_row(result);
    printf("user_socket = %d\n",atoi(*row));
    if(atoi(*row) != 0)
    {
        PACK pack1;
        pack1.data.send_fd = 2;
        pack1.data.recv_fd = 0;
        send(atoi(*row),&pack1,sizeof(pack1),0);
        
    }

    return 1;

}

int Send_File(int fd,PACK * pack)
{
    
    char buffer[1024] = "\0";
    FILE *fp = fopen(pack->data.recv_user,"r");
    if(fp == NULL)
    {
        printf("file not found\n");
        return -1;
    }

    else 
    {
        int i = 0;
        memset(buffer,0,sizeof(buffer));
        int file_block_length = 0;
        while(( file_block_length = fread(buffer,sizeof(char),1024,fp)) > 0)
        {
            i++;
            
            printf("file_block_length = %d\n",file_block_length);
            printf("client : %d\n",i);
            if(send(fd,buffer,file_block_length,0) < 0)
            {
                printf("Send file failed\n");
                break;
            }
            sleep(1);
            memset(buffer,0,sizeof(buffer));

        }
        
        fclose(fp);
        sleep(1);
        strcpy(buffer,"exit25908");
        send(fd,buffer,strlen(buffer),0);
        printf("File : %s\t发送成功，等待好友接收...\n",pack->data.recv_user);
    }

}