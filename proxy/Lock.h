/*
 * Auther: 
 * Date: 2022-09-05
 * Email: vlicecream@163.com
 * Detail: IMutex encapsulation class
 * */

#ifndef __SOCKET_Lock_H__
#define __SOCKET_Lock_H__

#include "SocketConfig.h"


class IMutex;

/* IMutex 封装类 */
class Lock
{
public:
	Lock(const IMutex&);
	~Lock();

private:
	const IMutex& m_mutex;
};

#endif // __SOCKET_Lock_H__
