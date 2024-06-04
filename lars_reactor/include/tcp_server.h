#pragma once
#include <winsock2.h>
#include<ws2tcpip.h>
#include<event_loop.h>
#include<thread_pool.h>
#pragma comment(lib,“ws2_32.lib”)



class tcp_server
{
public:
    //构造函数
    tcp_server(event_loop *_loop, const char* ip, __uint128_t port);
    
    //开始提供创建链接的服务
    void do_accept();

    //析构函数  资源的释放
    ~tcp_server();


private:
    int _sockfd; //套接字
    struct sockaddr_in _connaddr; //客户端连接地址
    socklen_t _addrlrn; //客户端链接地址长度

    //event_loop epoll的多路IO复用机制
    event_loop *_loop;

public:
    static tcp_conn **conns; //全部已经在线的连接
    static void increase_conn(int connfd, tcp_conn *conn); //新增一个连接
    static void decrease_conn(int connfd);
    static void get_conn_num(int *cur_conn); //得到当前连接个数
    static pthread_mutex_t _conns_mutex;//保护conns添加删除的互斥锁
 
    #define MAX_CONNS 3
    static int _max_conns;
    static int _curr_conns; //当前所管理的连接个数

    // 添加一个路由分发机制句柄
    static msg_router router;
    // 提供一个添加路由的方法
    void add_msg_router(int msgid, msg_callback *cb, void *user_data=NULL){
        router.registered_msg_router(msgid, cb, user_data);
    }

    // 设置连接创建之后的hook函数
    static void set_conn_start(conn_callback cb, void *args =NULL)
    {
        conn_start_cb = cb;
        conn_start_cb_args = args;
    }
    // 创建连接销毁之前的hook函数
    static void set_conn_close(conn_callback cb, void *args = NULL)
    {
        conn_close_cb = cb;
        conn_close_cb_args = args;
    }
    // 创建连接之后  要触发的回调函数
    static conn_callback conn_start_cb;
    static void *conn_start_cb_args;
    // 销毁连接之前  要触发的回调函数
    static conn_callback conn_close_cb;
    static void *conn_close_cb_args;

    // 连接池
    thread_pool* _thread_pool;

    thread_pool *get_thpool()
    {
        return _thread_pool;
    }
};