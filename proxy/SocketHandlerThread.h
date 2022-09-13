/*
 * Auther:
 * Date: 2022-09-09
 * Email: vlicecream@163.com
 * */


#ifndef __SOCKET_SocketHandlerThread_H__
#define __SOCKET_SocketHandlerThread_H__

#include "Thread.h"
#include "Semaphore.h"

class ISocketHandler;

class SocketHandlerThread : public Thread
{
public:
	// 构造函数
	SocketHandlerThread(ISocketHandler& parent);

	// 析构函数
	~SocketHandlerThread();

	virtual void Run();

	ISocketHandler& Handler();

	void Wait();

private:
	ISocketHandler& m_parent;
	ISocketHandler* m_handler;
	Semaphore m_sem;
};

#endif // __SOCKET_SocketHandlerThread_H__
