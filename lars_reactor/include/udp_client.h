#pragma once
#include<net_connection.h>
#include<event_loop.h>

class udp_client:public net_connection
{
public:
    udp_client(event_loop *loop, const char *ip, uint16_t port);
    ~udp_client();

    virtual int send_message(const char *data, int msglen, int msgid);
    void do_read();
    void add_msg_router(int msgid, msg_callback *cb, void *user_data=NULL);
private:
    int _sockfd;
    event_loop *_loop;
    char _read_buf[MESSAGE_LENGTH_LIMIT];
    char _write_buf[MESSAGE_LENGTH_LIMIT];


    //消息路由分发机制
	msg_router _router;
};