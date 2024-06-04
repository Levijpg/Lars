#include<udp_client.h>
#include<message.h>
#include<io.h>

udp_client::udp_client(event_loop *loop, const char *ip, uint16_t port)
{
    // 创建套接字
    _sockfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC, IPPROTO_UDP);
    if(_sockfd==-1)
    {
        perror("create udp server");
        exit(1);
    }

    // 2 初始化要连接服务器的地址
    struct sockaddr_in server_addr;
    ZeroMemory(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ip, &server_addr.sin_addr);

    // 连接服务器
    int ret = connect(_sockfd, (const sockaddr*)&server_addr, sizeof(server_addr));
    if(ret==-1)
    {
        perror("connect");
        exit(1);
    }

    // 3 添加一个读事件
    this->_loop = loop;
    _loop->add_io_envent(_sockfd, read_callback, EPOLLIN, this);



}
udp_client::~udp_client()
{
    _loop->delete_io_event(_sockfd);
    closesocket(_sockfd);
}


void udp_client::add_msg_router(int msgid, msg_callback *cb, void *user_data)
{
    _router.registered_msg_router(msgid, cb, user_data);
}


int udp_client::send_message(const char *data, int msglen, int msgid)
{
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
    int ret = sendto(_sockfd, _write_buf, msglen+MESSAGE_HEAD_LEN, 0, NULL, NULL);
    return ret;
}


void udp_client::do_read()
{
    while(true)
    {
        int pkg_len = recvfrom(_sockfd, _read_buf, sizeof(_read_buf), 0, NULL, 0);
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
            perror("udp client recv from error");
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


void read_callback(event_loop *loop, int fd, void *args)
{
    udp_client *client = (udp_client *)args;
    // 处理读业务
    client->do_read();
}