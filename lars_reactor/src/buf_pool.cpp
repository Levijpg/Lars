#include<buf_pool.h>
#include<assert.h>

//单例对象
buf_pool* buf_pool::_instance = NULL;

//初始化锁
pthread_mutex_t buf_pool::_mutex = PTHREAD_MUTEX_INITIALIZER;

//保证创建单例的一个方法，全局只执行一次
pthread_once_t buf_pool::_once = PTHREAD_ONCE_INIT;

//构造函数私有化
// bif_pool ---> [m4k] ---> io_buf...io_buf...io_buf
//               [m16k] ---> io_buf...io_buf...io_buf
//               ...
buf_pool::buf_pool():_total_mem(0)
{
    make_io_buf_list(m4k, 5000);
    make_io_buf_list(m16k, 5000);
    make_io_buf_list(m64k, 5000);
    make_io_buf_list(m128k, 5000);
    make_io_buf_list(m1M, 5000);
    make_io_buf_list(m4M, 5000);
    make_io_buf_list(m8M, 5000);

}

//从内存池申请一块内存
io_buf *buf_pool::alloc_buf(int N)
{
    //1 找到最接近N的一个刻度链表，返回一个io_buf
    int index;
    if(N<=m4k) index = m4k;
    else if(N<=m16k) index = m16k;
    else if(N<=m64k) index = m64k;
    else if(N<=m128k) index = m128k;
    else if(N<=m1M) index = m1M;
    else if(N<=m4M) index = m4M;
    else if(N<=m8M) index = m8M;
    else return NULL;

    //2 如果该index已经没有了，则需要额外的申请内存
    pthread_mutex_lock(&_mutex);
    if(_pool[index]==NULL)
    {   //index链表为空，需要新申请index大小的io_buf
        if(_total_mem+index/1024 >=MEM_LIMIT)
        {
            fprintf(stderr, "memory overflow\n");
            exit(1);
        }

        io_buf *new_buf = new io_buf(index);
        if(new_buf==NULL)
        {
            fprintf(stderr, "no io_buf error\n");
            exit(1);
        }

        _total_mem += index/1024;
        pthread_mutex_unlock(&_mutex);
        return new_buf;
    }
    
    //3 如果改index有剩余节点，从pool中拆除一块内存返回
    io_buf *target = _pool[index];
    _pool[index] = target->next;
    pthread_mutex_unlock(&_mutex);
    target->next = NULL;
    return target;
}


//重置io_buf放回pool
void buf_pool::revert(io_buf* buffer)
{
    //将buffer放回pool中
    int index = buffer->capacity;
    buffer->length = 0;
    buffer->head = 0;

    pthread_mutex_lock(&_mutex);
    assert(_pool.find(index)!=_pool.end());  //一定能找到key=index
    //将buffer设置为对应的buf链表开始节点
    buffer->next = _pool[index];
    _pool[index] = buffer;
    pthread_mutex_unlock(&_mutex);
}


void buf_pool::make_io_buf_list(int cap, int num)
{
    io_buf *prev;//链表头指针
    _pool[cap] = new io_buf(cap);
    if(_pool[cap]==NULL)
    {
        fprintf(stderr, "new io_buf m64k error");

        exit(1);
    }
    prev = _pool[cap];

    for(int i=1; i<=num; i++){
        prev->next = new io_buf(cap);
        if(prev->next==NULL){
            fprintf(stderr, "new io_buf m64k error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += cap*num;
}