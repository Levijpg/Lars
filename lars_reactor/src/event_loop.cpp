#include<event_loop.h>
#include<sys/epoll.h>
 //创建epoll
event_loop::event_loop()
{
    //创建一个epoll句柄 文件描述符
    _epfd = epoll_create1(0);
    if(_epfd==-1){
        fprintf(stderr, "epoll create error\n");
        exit(1);
    }
}


//阻塞循环监听事件，并且处理epoll_wait，包含调用对应的触发回调函数
void event_loop::event_process()
{
    io_event_map_it ev_it;
    while(true)
    {
        int nfds = epoll_wait(_epfd, _fired_evs, MAXEVENTS, -1);
        for(int i=0; i<nfds; i++)
        {
            //通过epoll出发的fd  在map中找到对应的io_event
            ev_it = _io_evs.find(_fired_evs[i].data.fd);

            //取出对应事件
            io_event *ev = ev_it->second;
            if(_fired_evs[i].enents & EPOLLIN)
            {
                //读事件，调用读回调函数
                void *args = ev->rcb_args;
                ev->read_callback(this, _fired_evs[i].data.fd, *args);
            }
            else if(_fired_evs[i].events & EPOLLOUT){
                //写事件，调用写回调函数
                void *args = ev->wcb_args;
                ev->write_callback(this, _fired_evs[i].data.fd, *args);
            else{
                //删除
                fprintf(stderr, "fd %d get error, delete from epoll", _fired_evs[i].data.fd);
                this->del_io_event(_fired_evs[i].data.id);
            }
            } 
        }
        // 每次全部的fd读写事件执行完，在此处执行一些其他的task任务流程
        this->execute_ready_tasks();
    }   
}


//添加一个io事件到event_pool中
void event_loop::add_io_envent(int fd, io_callback *proc, void *args)
{
    int op;
    int final_mask;
    //1 找到当前fd是都已经在map中
    // 如果没有事件，ADD方式添加到epoll中；存在，MOD方式添加到epoll中
    io_event_map_it it = _io_evs.find(fd);
    if(it == _io_evs.end()){
        op = EPOLL_CTL_ADD;
        final_mask = mask;
    }else{
        op = EPOLL_CTL_MOD;
        final_mask = it->second.mask|mask;
    }

    // 2 将fd和io_callback绑定到map中
    if(mask&EPOLLIN){
        //该事件是一个读事件
        _io_evs[fd].read_callback = proc; //注册读回调业务
        _io_evs[fd].rcb_args = args;
    }
    else if(mask&EPOLLOUT)
    {
        _io_evs[fd].write_callback = proc;
        _io_evs[fd].wcb_args = args;
    }

    _io_evs[fd].mask = final_mask;
    // 3 将当前时间加入到原生的epoll中
    struct epoll_event event;
    event.events = final_mask;
    event.data.fd = fd;
    if(epoll_ctl(_epfd, op, &event)==-1)
    {
        fprintf(stderr, "epoll ctl %d error\n", fd);
        return ;
    }
    // 4 将当前fd加入到正在监听的fd集合中
    listen_fds.insert(fd);
}


//删除一个io事件从event_pool中
void event_loop::delete_io_event(int fd)
{
    //将fd从_io_evs map中删除
    _io_evs.erase(fd);
    //将fd从listen_fds set中删除
    listen_fd.erase(fd);
    //将fd从原生的epoll堆中删除
    epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
}


//删除一个io事件的某个触发条件(EPOLLIN/EPOOLOUT)
void event_loop::del_io_event(int fd, int mask)
{
    io_event_map_it it = _io_evs.find(fd);
    if(it==_io_evs.end())
        return;

    //此时it为当前要删除的事件  key:fd  value:io_event
    int d_mask = it->second.mask;
    d_mask = d_mask & (~mask);
    if(d_mask==0)
    {
        //通过删除条件，已经没有触发条件
        this->del_io_event(fd);
    }else{
        //如果事件还存在，则修改当前事件
        struct epoll_event event;
        event.events = d_mask;
        event.data.fd = fd;
        epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &event);
    }
}


void event_loop::add_task(task_func func, void *args)
{
    task_func_pair func_pair(func, args);
    _ready_task.push_back(func_pair);
}


void event_loop::execute_ready_tasks()
{
    std::vector<task_func_pair>::it;
    for(it=_ready_task.begin(); it!=_ready_task.end(); it++)
    {
        task_func func = it->first;
        void *args = it->second;

        // 调用任务函数
        func(this, args);
    }

    // 全部执行完毕，清空当前的_ready_task集合
    _ready_task.clear();
}