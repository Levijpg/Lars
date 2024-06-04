#pragma once
#include<net_connection.h>
#include<event_loop.h>
#include<message.h>
class udp_server :public net_connection
{
public:
    udp_server(event_loop *loop, const char *ip, uint16_t port);
    virtual int send_message(const char *data, int msglen, int msgid);
    // 注册一个msgid和回调业务的路由关系
    void add_msg_router(int msgid, msg_callback *cb, void *user_data=NULL);

    ~udp_server();
    void do_read(); //处理客户端数据的业务
private:
    int _sockfd;
    event_loop *_loop;
    char _read_buf[MESSAGE_LENGTH_LIMIT];
    char _write_buf[MESSAGE_LENGTH_LIMIT];

    // 客户端IP地址
    struct sockaddr_in _client_addr;
    socklen_t _client_addrlen;

    //消息路由分发机制
	msg_router _router;
};