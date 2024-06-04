#include<include/io_buf.h>
#include<string.h>
#include<iostream>
#include<vector> 
#include<assert.h>

//创建一个size大小的buf
io_buf::io_buf(int size){
    capacity = size;
    length=0;
    head=0;
    next=NULL;

    data = new char[size];
    assert(data!=NULL); //如果data==NULL, 则程序直接退出
}

//清空数据
void io_buf::clear()
{
    length = head =0;
}

//处理长度为数据，移动head,len表示已经处理的数据的长度
void io_buf::pop(int len)
{
    length -= len;
    head += len;

} 
 
//将已经处理的数据清空，从内存删除，降维处理的数据移至buf的首地址，length减小
void io_buf::adjust()
{
    if(head!=0)
    {
        //length==0代表所有数据已经处理完
        if(length!=0)
            memmove(data, data+head, length);
        head=0;
    }
}

//将其他待处理部分的io_buf对象拷贝到自己
void io_buf::copy(const io_buf *other)
{
    memcpy(data, other->data+other->head, other->length);
    head=0;
    length = other->length;
}
