/*
 * Auther: icecream-xu
 * Date: 2022-09-03
 * Email: vlicecream@163.com
 * Detail: Stream Interface
 * */

#ifndef __SOCKET_IStream_H__
#define __SOCKET_IStream_H__

#include "SocketConfig.h"
#include <string>

class IStream
{
public:
	virtual ~IStream() {}

	/* 尝试从源中读取“buf_sz”字节数
	    返回实际读取的字节数 */
	virtual size_t IStreamRead(char *buf, size_t buf_sz) = 0;

	/* 将 'sz' 字节写入目标 */
	virtual void IStreamWrite(const char *buf, size_t sz) = 0;

};

#endif // __SOCKET_IStream_H__
