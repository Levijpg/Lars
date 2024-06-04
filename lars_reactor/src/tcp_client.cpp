#include<tcp_client.h>
#include<io.h>
#include<errno.h>
#include<message.h>
#include<ws2tcpip.h>
#include <WinSock2.h>

void read_callback(event_loop *loop, int fd, void *args)
{
    tcp_client *cli = (tcp_client*)args;
    cli->do_read();
}

void write_callback(event_loop *loop, int fd, void *args)
{
    tcp_client *cli = (tcp_client*)args;
    cli->do_write();
}

tcp_client::tcp_client(event_loop *loop, const char *ip, unsigned short port):_router()
{
    _sockfd = -1;
    _loop = loop;
    _conn_start_cb = NULL;
    _conn_close_cb = NULL;
    _conn_start_cb_args = NULL;
    _conn_close_cb_args = NULL;
    // 封装即将要连接的远程server的IP地址
    ZeroMemory(&_server_addr, sizeof(_server_addr));
    _server_addr.sin_family = AF_INET;
    inet_pton(ip, &_server_addr.sin_addr);
    _server_addr.sin_port = htons(port);

    // 连接远程
    this->do_connect();

}

// 发送方法
int tcp_client::send_message(const char *data, int msglen, int msgid)
{
    bool active_epollout = false;
    if(obuf.length()==0)
    {
        active_epollout = true;
    }
    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;

    // 写消息头
    int ret = obuf.send_data((const char*)&head, MESSAGE_HEAD_LEN);
    if(ret!=0)
    {
        fprintf(stderr, "send head error\n");
        return -1;
    }

    // 消息体
    ret = obuf.send_data(data, msglen);
    if(ret!=0)
    {
        fprintf(stderr, "send dara error\n");
        obuf.pop(MESSAGE_HEAD_LEN); 
        return -1;
    }
    if(active_epollout==true)
    {
        _loop->add_io_envent(_sockfd, write_callback, EPOLLOUT, this);
    }
}

// 处理读写业务
void tcp_client::do_read()
{
    // 1 从connfd中读数据
    int ret = ibuf.read_data(_sockfd);
    if(ret==-1)
    {
        fprintf(stderr, "read data from socket\n");
        this->clean_conn();
        return;
    }
    else if(ret==0)
    {
        // 对端客户端正常关闭
        printf("peer server closed\n");
        this->clean_conn();
    }

    msg_head head;
    // 2 读过来的数据是否满足8字节
    while(ibuf.length()>=MESSAGE_HEAD_LEN)
    {
        // 2.1 先读头部，得到msgid， msglen
        memcpy(&head, ibuf.data(), MESSAGE_HEAD_LEN);
        if(head.msglen>MESSAGE_LENGTH_LIMIT || head.msglen<0){
            fprintf(stderr, "data format error\n");
            this->clean_conn();
            break;
        }
        ibuf.pop(MESSAGE_HEAD_LEN);

        // 2.2 判断得到的消息体长度和头部里的长度是否一致
        if(ibuf.length()<MESSAGE_HEAD_LEN+head.msglen)
        {
            // 缓存中buf剩余的收数据小于应该接收到的数据，说明当前不是一个完整包
            break;
        }
        // 表示当前包是合法的
        ibuf.pop(MESSAGE_HEAD_LEN);
        // 处理ibuf.data()业务数据
        this->_router.call(head.msgid, head.msglen, ibuf.data(), this);

        // 消息处理结束
        ibuf.pop(head.msglen);
    }
    ibuf.adjust();
    return;
}
void tcp_client::do_write()
{
    while(obuf.length())
    {
        int write_num = obuf.write2fd(_sockfd);
        if(write_num==-1)
        {
            fprintf(stderr, "tcp_client write error\n");
            this->clean_conn();
            return ;
        }
        else if(write_num==0)
        {
            //当前不可写
            break;
        }
    }

    if(obuf.length()==0)
    {
        //数据已经全部写完,将_sockfd的写事件删除
        _loop->del_io_event(_sockfd, EPOLLOUT);
    }
    return ;
}

// 释放连接
void tcp_client::clean_conn()
{
    if(_conn_close_cb!=NULL)
    {
        _conn_close_cb(this, _conn_close_cb_args);
    }
    if(_sockfd!=-1)
    {
        printf("clean conn, del socket\n");
        _loop->delete_io_event(_sockfd);
        close(_sockfd);
    }
}

// 如果该函数被执行，表示连接创建成功
void conn_succ(event_loop *loop, int fd, void *args)
{
    tcp_client *cli = (tcp_client*)args;
    loop->delete_io_event(fd);
    // 对fd进行一次错误编码的获取
    int result=0;
    socklen_t result_len = sizeof(result);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&result, &result_len);
    if(result==0)
    {
        if(cli->_conn_close_cb!=NULL){
            cli->_conn_start_cb(cli, cli->_conn_start_cb_args);
        }

        // 添加当前cli fd的读回调
        loop->add_io_envent(fd, read_callback, EPOLLIN, cli);

        if(cli->obuf.length()!=0)
        {
            // 让event_loop触发写回调业务
            loop->add_io_envent(fd, write_callback, EPOLLOUT, cli);
        }
    }else{
        fprintf(stderr, "connection %s:%d error\n", inet_ntoa(cli->_server_addr.sin_addr), ntohs(cli->_server_addr.sin_port));
        return;
    }   
}

// 连接服务器
void tcp_client::do_connect()
{
    if(_sockfd!=-1)
    {
        close(_sockfd);
    }

    // 创建套接字(非阻塞)
    _sockfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, IPPROTO_TCP);
    if(_sockfd==-1)
    {
        fprintf(stderr, "create tcp client socker error\n");
        exit(1);
    }

    int ret = connect(_sockfd, (const sockaddr*)&_server_addr, _addrlen);
    if(ret==0)
    {
        // 创建连接成功，主动调用send_message发包
        printf("connect %s:%d succ!\n", inet_ntoa(_server_addr.sin_addr), ntohs(_server_addr.sin_port));

        conn_succ(_loop, _sockfd, this);
    }else{
        // 失败
        if(errno==EINPROGRESS)
        {
            // fd是非阻塞的，会出现这个错误，但不代表创建连接失败
            // 如果fd可写，表示连接创建成功
            printf("do connect, EINPROGRESS\n");

            // event_loop检测当前sockfd是否可写，如果回调被执行，说明可写，连接创建成功
            _loop->add_io_envent(_sockfd, conn_succ, EPOLLOUT, this);
        }
        fprintf(stderr, "connection error\n");
        exit(1);
    }
}