#pragma once

/*定义一个buffer 一块内存的数据结构体*/
class io_buf{
    public:
    io_buf(int size); //构造函数，创建一个size大小的buf
    //清空数据
    void clear();
    //处理长度为数据，移动head
    void pop(int len); //len表示已经处理的数据的长度

    //将已经处理的数据清空，从内存删除，降维处理的数据移至buf的首地址，length减小
    void adjust();

    //将其他的io_buf对象拷贝到自己
    void copy(const io_buf *other);

    int capacity; //当前buf的总容量
    int length;   //当前buf有效数据长度
    int head;     //当前未处理有效数据的头部索引
    char *data;   //当前buf的内存首地址
    io_buf *next;//存在多个io_buf  采用链表的形式进行管理
};