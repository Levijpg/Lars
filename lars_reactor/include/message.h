#pragma once
#include<unordered_map>
#include<tcp_conn.h>
#include<iostream>
/*解决tcp粘包问题的消息封装*/

struct msg_head
{
    int msgid; //当前消息类型
    int msglen; //消息体长度
};

//消息头长度，为固定值
#define MESSAGE_HEAD_LEN 8

//消息头+体的最大长度限制
#define MESSAGE_LENGTH_LIMIT (65535-MESSAGE_HEAD_LEN)

// 定义一个路由回调函数的数据类型
typedef void msg_callback(const char *data, uint32_t len, int msgid, net_connection* conn, void *user_data);

// 定义一个抽象的连接类：tcp_conn+tcp_client


/*定义消息路由分发机制*/
class msg_router{
public:
    // msg_router构造函数，初始化lianggemap
    msg_router():_router(), _args(){}

    // 给一个消息ID注册一个对应的回调业务函数
    int registered_msg_router(int msgid, msg_callback* msg_cb, void *user_data){
        if(_router.find(msgid)!=_router.end())
        {
            std::cout<< "msgid: " << msgid << "is already registered"<< std::endl;
            return -1;
        }
        _router[msgid] = msg_cb;
        _args[msgid] = user_data;
        return 0;
    }

    // 调用注册的回调函数业务
    void call(int msgid, uint32_t msglen, const char *data, net_connection* conn){
        std::cout << "call msgid= " << msgid << std::endl;
        if(_router.find(msgid)==_router.end())
        {
            std::cout<< "msgid: " << msgid << "is not registered"<< std::endl;
            return;
        }

        // 取出并执行回调函数
        msg_callback* callback = _router[msgid];
        void *user_data = _args[msgid];
        callback(data, msglen, msgid, conn, user_data);
    }

private:
    std::unordered_map<int, msg_callback*> _router; //针对消息的路由分发 <消息ID, 处理的业务回调函数>
    std::unordered_map<int, void*> _args; //每个路由业务函数对应的形参
};