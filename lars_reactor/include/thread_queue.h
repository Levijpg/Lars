#pragma once
#include<queue>
#include<event_loop.h>
#include<pthread.h>
#include<io.h>
template<typename T>
class thread_queue
{
public:
    thread_queue()
    {
        _loop = NULL;
        pthread_mutex_init(&_queue_tex, NULL);
        // 创建一个fd 用来被监听 能够被epoll监听
        _evfd = eventfd(0, EFD_NONBLOCK);
        if(_evfd ==-1)
        {
            perror("eventfd()");
            exit(1);
        }
    }
    ~thread_queue()
    {
        pthread_mutex_destroy(&_queue_tex);
        close(_evfd);
    }

    // 向队列中添加一个任务
    void send(const T &task)
    {
        // 将task加入到queue中，激活_evfd
        pthread_mutex_lock(&_queue_tex);
        _queue.push(task);

        // 激活_evfd可读事件，向_evfd写数据
        unsigned int idle_num = 1;
        int ret = write(_evfd, &idle_num, sizeof(unsigned long long));
        if(ret==-1)
        {
            perror("evfd write error");
        }

        pthread_mutex_unlock(&_queue_tex);
    }

    // 从队列中取数据 将整个queue返回给上层
    void recv(std::queue<T>& queue_msgs)
    {
        unsigned long long idle_num;
        // 拿出queue数据
        pthread_mutex_lock(&_queue_tex);
        int ret = read(_evfd, &idle_num, sizeof(unsigned long long));

        std::swap(queue_msgs, _queue);

        pthread_mutex_unlock(&_queue_tex);
    }

    //设置当前thead_queue是被哪个事件触发event_loop监控
    void set_loop(event_loop *loop)
    {
        this->_loop = loop;
    }

    //设置当前消息任务队列的 每个任务触发的回调业务
    void set_callback(io_callback *cb, void *args)
    {
        if(_loop!=NULL)
        {
            _loop->add_io_envent(_evfd, cv=b, EPOLLIN, args);
        }
    }

private:
    int _evfd; //让某个线程进行监听的
    event_loop *_loop; //目前被那个loop监听
    std::queue<T> _queue;
    pthread_mutex_t _queue_tex; //保护queue的互斥锁
};