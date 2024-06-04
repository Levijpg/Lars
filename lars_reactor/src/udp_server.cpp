#include<udp_server.h>
#include<io.h>


udp_server::udp_server(event_loop *loop, const char *ip, uint16_t port)
{
     //0.忽略一些信号 SIGHUB, SIGPIPE
    if(singal(SIGHUP, SIG_IGN)==SIG_ERR){
        fprintf(stderr, "signal ingore SIGHUB\n");
    }
    //1.创建socket
    _sockfd = socket(AF_INET, SOCK_DGRAM|SOCKNONBLOCK|SOCK_CLOEXEC, IPPROTO_UDP);
    if(_sockfd==-1)
    {
        perror("create udp server");
        exit(1);
    }

    //2.初始化服务器地址
    struct sockaddr_in server_addr;
    ZeroMemory(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    //3.绑定端口
    if(bind(_sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr))<0)
    {
        fprintf(stderr, "bind error\n");
        exit(1);
    }

    // 4.添加loop事件监控
    this->_loop = loop;
    

    // 5.清空客户端地址
    ZeroMemory(& _client_addr, sizeof(_client_addr));
    _client_addrlen = sizeof(_client_addr);

    // 监控_sockfd读事件，如果刻度，代表客户端有数据
    loop->add_io_envent(_sockfd, read_callback, EPOLLIN, this);

}

// 服务端处理客户端的读事件
void read_callback(event_loop *loop, int fd, void *args)
{
    udp_server *server = (udp_server*)args;
    server->do_read();
}

// 读客户端数据
void udp_server::do_read()
{
    while(true)
    {
        int pkg_len = recvfrom(_sockfd, _read_buf, sizeof(_read_buf), 0, (sockaddr*)&_client_addr, &_client_addrlen);
        if(pkg_len==-1)
        {
            // 中断错误
            continue;
        }
        else if(errno==EAGAIN)
        {
            // 非阻塞
            break;
        }
        else{
            perror("udp server recv from error");
            break;}

        msg_head head;
        memcpy(&head, _read_buf, MESSAGE_HEAD_LEN);
        if(head.msglen>MESSAGE_LENGTH_LIMIT||head.msglen<0||head.msglen+MESSAGE_HEAD_LEN!=pkg_len)
        {
            fprintf(stderr, "error message head format error\n");
            continue;
        }

        _router.call(head.msgid, head.msgid, _read_buf+MESSAGE_HEAD_LEN, this);
    }
}

int udp_server::send_message(const char *data, int msglen, int msgid)
{
    // udpserver主动发包给客户端
    if(msglen>MESSAGE_LENGTH_LIMIT)
    {
        fprintf(stderr, "message too big\n");
        return -1;
    }

    msg_head head;
    head.msglen = msglen;
    head.msgid = msgid;
    // 将head写到输出缓冲中
    memcpy(_write_buf, &head, MESSAGE_HEAD_LEN);
    // 将data写到输出缓冲中
    memcpy(_write_buf+MESSAGE_HEAD_LEN, data, msglen);
    // 通过_client_addr将报文发送给对方客户端
    int ret = sendto(_sockfd, _write_buf, msglen+MESSAGE_HEAD_LEN, 0, (const sockaddr*)&_client_addr, _client_addrlen);
    return ret;
}

// 注册一个msgid和回调业务的路由关系
void udp_server::add_msg_router(int msgid, msg_callback *cb, void *user_data)
{
    _router.registered_msg_router(msgid, cb, user_data);
}

udp_server::~udp_server()
{
    _loop->delete_io_event(_sockfd);
    close(_sockfd);
}