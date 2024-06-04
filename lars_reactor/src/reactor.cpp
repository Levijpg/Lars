#include<reactor.h>
#include<assert.h>
#include<io.h>
#include<winioctl.h>

reactor_buf::reactor_buf()
{
    _buf = NULL;

}
reactor_buf::~reactor_buf()
{
    this->clear();
}

//得到当前的buf还有多少有效数据
int reactor_buf::length()
{
    if(_buf==NULL){
        return 0;
    }else{
        return _buf->length;
    }
}

//已经处理了多少数据
void reactor_buf::pop(int len)
{
    assert(_buf!=NULL && len<=_buf->length);
    _buf->pop(len);
    if(_buf->length==0){
        //当前的_buf已经全部用完
        this->clear();
    }
}

void reactor_buf::clear()
{
    if(_buf!=NULL){
        //将_buf放回_pool中
        buf_pool::instance()->revert(_buf);
        _buf = NULL;
    }
}

//========================================================


//从一个fd中读取数据到reactor_buf中
int input_buf::read_data(int fd)
{
    int need_read; //有多少数据待读取
    //一次性将io中的缓存数据全部读出来
    //传一个参数，目前缓存中一共有多少数据是可读

    if(_buf==NULL)
    {
        //如果input_buf里的_buf是空，需要从buf_pool中重新获取
        _buf = buf_pool::instance()->alloc_buf(need_read);
        if(_buf==NULL){
            fprintf(stderr, "no buf for alloc\n");
            return -1;
        }
    }else{
        //如果_buf可用，判断当前buf时候够存
        assert(_buf->head==0);
        if(_buf->capacity-_buf->length < need_read)
        {
            //当前情况不够村，需要重新申请一个buf
            io_buf *new_buf = buf_pool::instance()->alloc_buf(need_read+_buf->length);
            //将之前的_buf数据拷贝到新的buf中
            new_buf->copy(_buf);
            //将之前的_buf放回pool
            buf_pool::instance()->revert(_buf);
            //新申请的buf为当前的io_buf
            _buf = new_buf;
        }
    }

    int already_read = 0; //
    //此时，当前buf空间足够，可以开始读取数据
    do{
        if(need_read==0)
            already_read = read(fd, _buf->data+_buf->length, m4k); //阻塞直到有数据
        else
            already_read = read(fd, _buf->data+_buf->length, need_read);
    }while(already_read==-1&&errno==EINTR);

    if(already_read>0)
    {
        if(need_read!=0)
            assert(already_read==need_read);
        _buf->length += already_read; //如成功读取数据，更新缓冲区长度
    }
    return already_read; //返回读取字节数
}

//获取当前的数据的方法
const char*input_buf::data()
{
    return _buf!=NULL?_buf->data+_buf->head:NULL;
}

//重置缓冲区
void input_buf::adjust()
{
    if(_buf!=NULL)
        _buf->adjust();
}

//========================================================
//将一段数据写到自身的_buf中
int output_buf::send_data(const char *data, int datalen)
{
    
    if(_buf==NULL)
    {
        //如果outputput_buf里的_buf是空，需要从buf_pool中重新获取
        _buf = buf_pool::instance()->alloc_buf(datalen);
        if(_buf==NULL){
            fprintf(stderr, "no buf for alloc\n");
            return -1;
        }
    }else{
         assert(_buf->head==0);
        if(_buf->capacity-_buf->length < datalen)
        {
            //当前情况不够村，需要重新申请一个buf
            io_buf *new_buf = buf_pool::instance()->alloc_buf(datalen+_buf->length);
            //将之前的_buf数据拷贝到新的buf中
            new_buf->copy(_buf);
            //将之前的_buf放回pool
            buf_pool::instance()->revert(_buf);
            //新申请的buf为当前的io_buf
            _buf = new_buf;
        }
    }

    //将data数据写入io_buf中
    memcpy(_buf->data+_buf->length, data, datalen);
    _buf->length += datalen;
}


//将_buf中的数据写到一个fd中
int output_buf::write2fd(int fd)
{
    assert(_buf!=NULL && _buf->head==0);
    int already_write = 0;
    do{
        already_write = write(fd, _buf->data, _buf->length);
    }while(already_write==-1&&errno==EINTR);

    if(already_write>0)
    {
        //写成功
        _buf->pop(already_write); //把已经处理的数据弹出，后移head
        _buf->adjust();
    }

    //如果fd是非阻塞的，会报already_write == -1 errno==EAGAIN
	if (already_write == -1 && errno == EAGAIN)
	{
		already_write = 0;//表示非阻塞导致的-1不是一个错误，表示是正确的只是写0个字节
	}

	return already_write;
    
}