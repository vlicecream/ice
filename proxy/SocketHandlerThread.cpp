/*
 * Auther:
 * Date: 2022-09-09
 * Email: vlicecream@163.com
 * */


#include "ISocketHandler.h"
#include "SocketHandlerThread.h"
#include "Mutex.h"

SocketHandlerThread::SocketHandlerThread(ISocketHandler& parent) 
	:
		m_parent(parent),
		m_handler(nullptr)
{

}

SocketHandlerThread::~SocketHandlerThread()
{

}

ISocketHandler& SocketHandlerThread::Handler()
{
	return *m_handler;
}

void SocketHandlerThread::Run()
{
	Mutex mutex;
	m_handler = m_parent.Create(mutex, m_parent);
	m_sem.Post();

	ISocketHandler& h = *m_handler;
	h.EnableRelease();
	while (IsRunning()) {
		h.Select(1, 0);
	}
}

void SocketHandlerThread::Wait()
{
	m_sem.Wait();
}

