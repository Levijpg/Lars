#pragma once
#include<event_loop.h>
#include<reactor.h>
#include<net_connection.h>
#include<message.h>

typedef void msg_callback(const char *data, uint32_t len, int msgid, tcp_client *client, void *args);
class tcp_client: public net_connection
{
public:
    tcp_client(event_loop *loop, const char *ip, unsigned short port);

    // 发送方法
    virtual int send_message(const char *data, int msglen, int msgid);

    // 处理读写业务
    void do_read();
    void do_write();

    // 释放连接
    void clean_conn();

    // 连接服务器
    void do_connect();

    // 消息分发机制
    msg_router _router;


    // 注册消息路由回调函数
    void add_msg_callback(int msgid, msg_callback *cb, void *user_data=NULL)
    {
        _router.registered_msg_router(msgid, cb, user_data);
    }
    
    // 输入缓冲
    input_buf ibuf;
    // 输出缓冲
    output_buf obuf;
    // server端ip地址
    struct  sockaddr_in _server_addr;

    void set_conn_start(conn_callback cb, void *args = NULL)
    {
        _conn_start_cb = cb;
        _conn_start_cb_args = args;
    }

        void set_conn_close(conn_callback cb, void *args = NULL)
    {
        _conn_close_cb = cb;
        _conn_close_cb_args = args;
    }

    // 创建连接之后触发的回调函数
    conn_callback _conn_start_cb;
    void* _conn_start_cb_args;
    // 销毁连接之前触发的回调函数
    conn_callback _conn_close_cb;
    void* _conn_close_cb_args;
private:
    int _sockfd; //当前客户端连接


    socklen_t _addrlen;

    // 客户端时间的处理机制
    event_loop *_loop;



    
};