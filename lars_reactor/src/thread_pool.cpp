#include<thread_pool.h>
#include<tcp_conn.h>

void deal_task(event_loop *loop, int fd, void *args)
{
    // 从thread_queue中取数据
    thread_queue<task_msg>* queue = (thread_queue<task_msg>*) args;
    std::queue<task_msg> new_task_queue;
    queue->recv(new_task_queue);//new_task_queue存放的是thread_queue的全部任务

    while(!new_task_queue.empty())
    {
        // 从队列中得到一个任务
        task_msg task = new_task_queue.front();
        new_task_queue.pop();
        if(task.type==task_msg::NEW_CONN)
        {
            // 让当前线程去创建连接，同时将这个连接的connfd加入到当前的thread的event_loop中监听
            tcp_conn *conn = new tcp_conn(task.connfd, loop);
            if(conn==NULL)
            {
                fprintf(stderr, "in thread new tcp_conn errror\n");
                exit(1);
            }
            
        }else if(task.type==task_msg::NEW_TASK)
        {
            // 将该任务添加到event_loop的循环中去执行
            loop->add_task(task.task_cb, task.args);
        }
        else{
            fprintf(stderr, "unknown task\n");
        }
    }
    // 判断task类型，如果是任务1：新建连接任务(new tcp_conn； 任务2：普通任务
}


// 线程主业务函数
void *thread_main(void *args)
{
    thread_queue<task_msg> *queue = (thread_queue<task_msg>*) args;
    event_loop *loop = new event_loop();//每个线程都有loop
    if(loop==NULL)
    {
        fprintf(stderr, "new event_loop error");
        exit(1);
    }

    queue->set_loop(loop);
    queue->set_callback(deal_task, queue);

    // 启动loop监听
    loop->event_process();
}


thread_pool::thread_pool(int thread_cnt)
{
    _queues = NULL;
    _thread_cnt = thread_cnt; 
    _index = 0;
    
    // 创建thread_queue
    _queues = new thread_queue<task_msg>*[thread_cnt];  // 开辟了cnt个thread_queue指针
    _tids = new pthread_t[thread_cnt];
 
    // 开辟线程
    int ret;
    for(int i = 0; i<thread_cnt; i++)
    {   
        // 给每一个thread_queue开辟内存，创建对象
        _queues[i] = new thread_queue<task_msg>();
        
        //第i个线程绑定第i个thread_queue
        ret = pthread_create(&_tids[i], NULL, thread_main, _queues[i]);
        if(ret==-1)
           { perror("thread_pool create error");
            exit(1);}

        // 将线程设置为detach模型  线程分离模式
        pthread_detach(_tids[i]);
    }
}


thread_queue<task_msg>* thread_pool::get_thread()
{
    if(_index==_thread_cnt)
    {
        _index=0;
    }
    return _queues[_index++];
}

void thread_pool::send_task(task_func func, void *args)
{
    // 给当前thread_pool中每一个thread里的queue去发送当前任务
    task_msg task;
    task.type = task_msg::NEW_TASK;
    task.task_cb = func;
    task.args = args;

    for(int i=0; i<_thread_cnt; i++)
    {
        // 取出第i个现成的消息队列queue
        thread_queue<task_msg> *queue = _queues[i];
        // 给queue发送一个task
        queue->send(task);
    }
}