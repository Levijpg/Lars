#pragma once
#include<event_loop.h>
#include<net_connection.h>
#include<reactor.h>

class tcp_conn:public net_connection
{
public:
    // 初始化conn
    tcp_conn(int connfd, event_loop *loop);
    // 被动处理读业务方法 由event_loop监听触发
    void do_read(event_loop *loop, int fd, void *args);
    // 被动处理写业务方法
    void do_write(event_loop *loop, int fd, void *args);
    // 主动发送消息的方法
    virtual int send_message(const char *data, int msglen, int mgsid);
    // 销毁当前链接
    void clean_conn();
private:
    // 当前连接的套接字fd
    int _connfd;
    // 当前连接归属于哪个event_loop
    event_loop *_loop;
    // 输出output_buf
    output_buf obuf;
    // 输入input_buf
    input_buf ibuf;
};