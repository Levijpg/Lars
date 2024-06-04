#pragma once
/*thread_queue 消息队列 所能接受的消息类型*/

struct task_msg
{
    //两大类task：新建立连接任务+一般普通任务（主线程希望分发给每个线程的任务）
    // 
    enum TASK_TYPE{
        NEW_CONN, //新建连接的任务
        NEW_TASK //一般的任务
    };
    TASK_TYPE type;


    // 任务本身数据内容
    union 
    {
        int connfd;
        // 任务2  具体的数据参数和具体的回调业务
        struct{
        void(*task_cb)(event_loop *loop, void *args);
        void *args;
    };
    };
};