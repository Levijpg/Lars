#pragma once
#include<pthread.h>
#include<ext/hash_map>
#include<unordered_map>
#include<include/io_buf.h>
typedef std::unordered_map<int, io_buf*> pool_t;
//定义一些内存刻度
enum MEM_CAP{
    m4k = 4096,
    m16k = 16384,
    m64k = 65536,
    m128k = 262144,
    m1M = 1048576,
    m4M = 4194304,
    m8M = 8388608 
};

//总内存池大小限制
#define MEM_LIMIT (5U*1024*1024)

class buf_pool
{
public:
    //初始化单例对象
    static void init(){
        _instance = new buf_pool();
    }
    //提供一个静态获取instance的方法
    static buf_pool* instance()
    {   
        pthread_once(&_once, init); //保证init方法在进程的生命周期只执行一次
        return _instance;
    }

    //从内存池申请一块内存
    io_buf *alloc_buf(int N);
    io_buf *alloc_buf();

    //重置io_buf放回pool
    void revert(io_buf* buffer);

    void buf_pool::make_io_buf_list(int cap, int num);
private:
    //==============创建单例================
    //构造函数私有化
    buf_pool(); 
    buf_pool(const buf_pool&);
    const buf_pool& operator = (const buf_pool&);

    //单例对象
    static buf_pool*_instance; 

    //保证创建单例的一个方法，全局只执行一次
    static pthread_once_t _once;

    //==============buf_pool属性==============
    //存放所有io_buf的map句柄
    pool_t _pool; //<key, value>=<内存块刻度， 当前刻度下所挂载的io_buf链表>
    //当前内存池总体大小
    uint64_t _total_mem;
    //保护pool map增删改查的锁
    static pthread_mutex_t _mutex;

};