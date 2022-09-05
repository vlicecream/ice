/*
 * Auther: icecream-xu
 * Date: 2022-09-04
 * Email: vlicecream@163.com
 * Detail: Encapsulate the lock's API
 * */

#ifndef __SOCKET_Mutex_H__
#define __SOCKET_Mutex_H__

#include <pthread.h>
#include "SocketConfig.h"
#include "SocketInclude.h"
#include "IMutex.h"

/* 锁的容器类, used by Lock. 
	\ingroup threading */
class Mutex : public IMutex
{
public:
	Mutex();
	~Mutex();

	virtual void Lock() const;
	virtual void Unlock() const;

private:
	mutable pthread_mutex_t m_mutex;
};

#endif // __SOCKET_Mutex_H__
