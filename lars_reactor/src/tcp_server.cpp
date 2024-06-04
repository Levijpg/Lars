#include<iostream>
#include<stdio.h>
#include<include/tcp_server.h>
#include<sys/types.h>
#include<stdlib.h>
#include<strings.h>
#include<winsock2.h>
#include<ws2tcpip.h>
#include<signal.h>
#include<errno.h>
#include<io.h>
#include<windows.h>
#include<reactor.h>
#include<message.h>
#include<tcp_conn.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;


// ======================针对连接管理的初始化===========================
tcp_conn **tcp_server::conns = NULL;
pthread_mutex_t tcp_server::_conns_mutex = PTHREAD_MUTEX_INITIALIZER;
int tcp_server::_curr_conns = 0;
int tcp_server::_max_conns = 0;

// ======================初始化  路由分发机制句柄===========================
msg_router tcp_server::router;

// ======================初始化  HOOK函数===========================
conn_callback tcp_server::conn_close_cb = NULL;
conn_callback tcp_server::conn_start_cb = NULL;
void *tcp_server::conn_close_cb_args = NULL;
void *tcp_server::conn_start_cb_args = NULL;

void tcp_server::increase_conn(int connfd, tcp_conn *conn)
{
    pthread_mutex_lock(&_conns_mutex);
    conns[connfd] = conn;
    _curr_conns++;
    pthread_mutex_unlock(&_conns_mutex);
}

void tcp_server::decrease_conn(int connfd)
{
    pthread_mutex_lock(&_conns_mutex);
    conns[connfd] = NULL;
    _curr_conns--;
    pthread_mutex_unlock(&_conns_mutex);
}

void tcp_server::get_conn_num(int *curr_conn)
{
    pthread_mutex_lock(&_conns_mutex);
    *curr_conn = _curr_conns;
    pthread_mutex_unlock(&_conns_mutex);
}
// ======================以上都是连接管理相关函数===========================


void accept_callback(event_loop* loop, int fd, void *args);
void server_wt_callback(event_loop *loop, int fd, void *args);

//临时的收发消息结构
struct message{
    char data[m4k];
    char len;
};

void accept_callback(event_loop* loop, int fd, void *args)
{
    tcp_server *server = (tcp_server*)args;
    server->do_accept();
}

//客户端connfd注册的写事件的回调业务
void server_wt_callback(event_loop *loop, int fd, void *args)
{
    struct message *msg = (struct message*) args;
    output_buf obuf;

    //将msg加入到obuf
    obuf.send_data(msg->data, msg->len);
    while(obuf.length())
    {
        int write_num = obuf.write2fd(fd);
        if(write_num==-1)
        {
            fprintf(stderr, "write connfd error\n");
            return;
        }
        else if(write_num==0)
        {
            //当前不可写
            break;
        }
    } 

    //删除写事件  添加读事件
    loop->del_io_event(fd, EPOLLOUT);
    loop->add_io_envent(fd, server_rd_callback, EPOLLIN, msg);
}

//客户端connfd注册的读事件的回调业务
void server_rd_callback(event_loop *loop, int fd, void *args)
{
    struct message *msg = (struct message*) args;
    int ret = 0;
    input_buf ibuf;
    ret = ibuf.read_data(fd);
    if(ret==-1){
        fprintf(stderr, "ibuf_read errror\n");
        //当前的读事件删除
        loop->delete_io_event(fd);

        //关闭
        close(fd);
        return ;
    }

    if(ret==0)
    {
        //对方正常关闭，当前的读事件删除
        loop->delete_io_event(fd);
        close(fd);
        return ;
    }

    //将读到的数据拷贝到msg中
    msg->len = ibuf.length();
    ZeroMemory(msg->data, msg->len);
    memcpy(msg->data, ibuf.data(), msg->len);

    ibuf.pop(msg->len);
    ibuf.adjust();
    printf("recv data = %s\n", msg->data);

    //删除读事件，添加写事件
    loop->del_io_event(fd, EPOLLIN);
    loop->add_io_envent(fd, server_wt_callback, EPOLLOUT, msg); //epoll_wait会立刻触发EPOLLOUT事件
}


//构造函数
tcp_server::tcp_server(event_loop *loop, const char* ip, __uint128_t port)
{   
    //0.忽略一些信号 SIGHUB, SIGPIPE
    if(singal(SIGHUP, SIG_IGN)==SIG_ERR){
        fprintf(stderr, "signal ingore SIGHUB\n");
    }
    //1.创建socket
    _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(_sockfd==-1)
    {
        fprintf(stderr, "tcp::server socket()\n");
        exit(1);
    }
    
    //2.初始化服务器地址
    struct sockaddr_in server_addr;
    ZeroMemory(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    //设置socket可以重复监听
    int op=1;
    if(setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&op, sizeof(op))<0){
        fprintf(stderr, "set socketopt error\n");
        exit(1);
    }

    //3.绑定端口
    if(bind(_sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr))<0)
    {
        fprintf(stderr, "bind error\n");
        exit(1);
    }
    
    //4.监听
    if(listen(_sockfd, 500)==-1)
    {
        fprintf(stderr, "listen error\n");
        exit(1);
    }

    // 5.将形参loop添加到tcp_server _loop中
    _loop = loop;

    //6 创建连接
    _max_conns = MAX_CONNS;
    conns = new tcp_conn*[_max_conns+3]; //3表示stdin, stdout, stderr
    if(conns==NULL)
    {
        fprintf(stderr, "new conns error\n");
        exit(1);
    }

    //7.创建线程池
    int thread_cnt = 3;
    if(thread_cnt>0){
        _thread_pool = new thread_pool(thread_cnt);
        if(_thread_pool==NULL)
        {
            fprintf(stderr, "tcp server new thread_pool error\n");
        }
    }

    
}
    
//开始提供创建链接的服务
void tcp_server::do_accept(){
    int connfd;
    while(1){
        //1 accept
        connfd = accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlrn);
        if(connfd==-1)
        {
            if(errno==EINTR){
                fprintf(stderr, "accept error = EINTR\n"); //中断错误
                continue;
            }
            else if(errno==EAGAIN){
                fprintf(stderr, "accept error = EAGAIN\n"); 
                break;
            }
            else if(errno == EMFILE){
                //建立连接过多，资源不够
                fprintf(stderr, "accpet error = EMFILE\n");
                continue;
            }
            else exit(1);
        }else{
            //accept success
            // 判断连接个数是否已经超过最大值_max_conns
            int cur_conns;
            get_conn_num(&cur_conns);
            if(cur_conns>=_max_conns)
            {
                fprintf(stderr, "so many connection\n");
                close(connfd);
            }else{
                if(_thread_pool!=NULL)
                {
                    // 开启多线程模式(将connfd交给线程去创建并监听)
                    thread_queue<task_msg>* queue = _thread_pool->get_thread();
                    task_msg task;
                    task.type = task_msg::NEW_CONN;
                    task.connfd = connfd;

                    queue->send(task);

                }else{
                    // 创建一个新的tcp_conn连接对象
                    tcp_conn *conn = new tcp_conn(connfd, _loop);
                    if(conn==NULL)
                    {
                        fprintf(stderr, "new tcp_conn error\n");
                        exit(1);
                    
                    }
                    break;
                }
            }
            printf("get noew conn\n");
            break;
        }
    }
}

//析构函数  资源的释放
tcp_server::~tcp_server(){

}