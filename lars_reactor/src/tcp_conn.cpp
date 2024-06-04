#include<tcp_conn.h>
#include<message.h>
#include<tcp_server.h>
#include<io.h>

void callback_busi(const char *data, uint32_t len, int msgid, void *args, tcp_conn *conn)
{
    // 让tacp_conn主动发送一个消息给对应的client
    conn->send_message(data, len, msgid);
}

void conn_rd_callback(event_loop *loop, int fd, void *args)
{
    tcp_conn *conn = (tcp_conn*)args;
    conn->do_read(loop, fd, conn);
}

void conn_wt_callback(event_loop *loop, int fd, void *args)
{
    tcp_conn *conn = (tcp_conn*) args;
    conn->do_write(loop, fd, conn);
}

// 初始化conn
tcp_conn::tcp_conn(int connfd, event_loop *loop)
{
    _connfd = connfd;
    _loop = loop;
    //1 将connfd设置为非阻塞状态
    int flag = fcntl(_connfd, F_SETFL, 0); //将connfd的全部状态清空
    fcntl(_connfd, F_SETFL, O_NONBLOCK|flag); //设置为非阻塞

    // 2 设置TCP_NODELAY，禁止读写缓存，降低小包延迟
    int op = 1;
    setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));

    // 执行创建连接成功之后要触发的HOOK函数
    if(tcp_server::conn_close_cb!=NULL)
    {
        tcp_server::conn_close_cb(this, tcp_server::conn_start_cb_args);
    }

    //将当前的tcp_conn的读事件 加入到loop中进行监听
    _loop->add_io_envent(_connfd, conn_rd_callback, EPOLLIN, this);

    // 将自己添加到tcp_server中的conns集合
    tcp_server::increase_conn(_connfd, this);
}

// 被动处理读业务方法 由event_loop监听触发
void tcp_conn::do_read(event_loop *loop, int fd, void *args) //隐藏一个this指针
{
    // 1 从connfd中读数据
    int ret = ibuf.read_data(_connfd);
    if(ret==-1)
    {
        fprintf(stderr, "read data from socket\n");
        this->clean_conn();
        return;
    }
    else if(ret==0)
    {
        // 对端客户端正常关闭
        printf("peer client closed\n");
        this->clean_conn();
    }
    // 读出来的消息头
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
        printf("read data = %s\n", ibuf.data());
        tcp_server::router.call(head.msgid, head.msglen, ibuf.data(), this);

        // 消息处理结束
        ibuf.pop(head.msglen);
    }
    ibuf.adjust();
    return;
}

// 被动处理写业务方法
void tcp_conn::do_write(event_loop *loop, int fd, void *args)
{
    // do_write就表示obuf中已经有要写的数据，将obuf的数据io write发给对端
    while(obuf.length())
    {
        int write_num = obuf.write2fd(_connfd);
        if(write_num==-1)
        {
            fprintf(stderr, "tcp_conn write error\n");
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
        //数据已经全部写完,将_connfd的写事件删除
        _loop->del_io_event(_connfd, EPOLLOUT);
    }
    return ;
}


// 主动发送消息的方法
int tcp_conn::send_message(const char *data, int msglen, int msgid)
{
    printf("server send message: data: %s, length: %d \n", data, msglen);
    bool active_epollout = false;
    if(obuf.length()==0)
    {
        // obuf为空，需要再次发送。激活epoll的写事件回调
        // obuf不为空，数据还没有完全写完到对端，等全部写完再激活
        active_epollout = true;
    }

    // 1 封装一个message包头
    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;

    int ret = obuf.send_data((const char *)&head, MESSAGE_HEAD_LEN);
    if(ret!=0){
        fprintf(stderr, "send head error\n");
        return -1;
    }

    // 2 写消息体
    ret = obuf.send_data(data, msglen);
    if(ret!=0){
        fprintf(stderr, "send data error\n");
        // 如果消息体写失败，消息头需要弹出
        obuf.pop(MESSAGE_HEAD_LEN);
        return -1;
    }

    // 3 将_connfd添加一个写事件 EPOLLOUT
    if(active_epollout==true)
    {
        _loop->add_io_envent(_connfd, conn_wt_callback, EPOLLOUT, this);
    }
    return 0;
}


// 销毁当前链接
void tcp_conn::clean_conn()
{
    // 销毁之前要触发的HOOK
    if(tcp_server::conn_close_cb!=NULL)
    {
        tcp_server::conn_close_cb(this, tcp_server::conn_close_cb_args);
    }
    
    // 从tcp_server中把当前连接摘除
    tcp_server::decrease_conn(_connfd);
    
    //连接的清理工作
    _loop->delete_io_event(_connfd);
    ibuf.clear();
    obuf.clear();
    close(_connfd);
}