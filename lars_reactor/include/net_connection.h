#pragma once

/*抽象的连接类*/
class net_connection
{
public:
    net_connection(){};
    virtual int send_message(const char *data, int msglen, int msgid)=0;

    void *param; //开发者可以通过该参数传递一些动态的自定义参数
};

// 创建连接/销毁连接要触发的回调函数 的函数类型
typedef void (*conn_callback)(net_connection *conn, void *args);