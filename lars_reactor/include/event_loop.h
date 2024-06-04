#pragma once
#include<unordered_map>
#include<unordered_set>
#include<event_base.h>
#include <ws2tcpip.h>
#include<winsock2.h>
#include<vector>

#define MAXEVENTS 10
//map:<key, value>------><fd, io_event>
typedef std::unordered_map<int, io_event> io_event_map;
typedef std::unordered_map<int, io_event>::iterator io_event_map_it;

//set: fd
typedef std::unordered_set<int> listen_fd_set;
typedef void(*task_func)(event_loop *loop, void *args);
class event_loop
{
public:
    //创建epoll
    event_loop();
    //阻塞循环监听事件，并且处理
    void event_process();
    //添加一个io事件到event_pool中
    void add_io_envent(int fd, io_callback *proc, int mask, void *args);
    //删除一个io事件从event_pool中
    void delete_io_event(int fd);
    //删除一个io事件的某个触发条件(EPOLLIN/EPOOLOUT)
    void del_io_event(int fd, int mask);
    //----->针对异步task任务的方法
    // 添加一个task到_ready_task
    void add_task(task_func func, void *args);
    void execute_ready_tasks();
    // 获取当前event_loop中正在监听的fd集合
    void get_listen_fds(listen_fd_set &fds)
    {
        fds = listen_fd;
    }
private:
    int _epfd; //通过epoll_create创建

    //当前event_loop监控的fd和对应事件的关系
    io_event_map _io_evs; //关系表

    //当前event_loop都有哪些fd正在监听[epoll_wait()正在等待哪些fd触发状态]
    listen_fd_set listen_fd;

    //每次epoll_wait所返回的是被激活的事件集合
    struct epoll_event _fired_evs[MAXEVENTS];

    //----->针对异步task任务的属性
    typedef std::pair<task_func, void *> task_func_pair;
    // 目前已经就绪的task任务
    std::vector<task_func_pair> _ready_task;

};